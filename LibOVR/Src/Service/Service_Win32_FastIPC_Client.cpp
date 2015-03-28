/************************************************************************************

Filename    :   Service_Win32_FastIPC_Client.cpp
Content     :   Client side of connectionless fast IPC
Created     :   Sept 16, 2014
Authors     :   Chris Taylor

Copyright   :   Copyright 2014 Oculus VR, LLC All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.2 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.2 

Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#include "Service_Win32_FastIPC_Client.h"

namespace OVR { namespace Service { namespace Win32 {

using namespace OVR::Net;


//// FastIPCClient

FastIPCClient::FastIPCClient()
{
}

bool FastIPCClient::ReadInitialData(const char* buffer)
{
    uint32_t magic    = *(uint32_t*)(buffer);
    uint32_t verMajor = *(uint32_t*)(buffer + 4);
    uint32_t verMinor = *(uint32_t*)(buffer + 8);

    if (magic != Magic)
    {
        LogError("Magic does not match");
        return false;
    }

    if (verMajor != MajorVersion)
    {
        LogError("Major version mismatch");
        return false;
    }

    if (verMinor < MinorVersion)
    {
        LogError("Remote minor version too old for our feature level");
        return false;
    }

    HANDLE remoteDataEvent   = (HANDLE)*(uint64_t*)(buffer + 12);
    HANDLE remoteReturnEvent = (HANDLE)*(uint64_t*)(buffer + 20);
    pid_t serverProcessId    = (pid_t) *(uint64_t*)(buffer + 28);
    OVR_UNUSED(serverProcessId);

    if (!remoteDataEvent || !remoteReturnEvent)
    {
        LogError("Handshake was malformed.  It seems like a version mismatch.");
        return false;
    }

    DataEvent.Attach(remoteDataEvent);
    ReturnEvent.Attach(remoteReturnEvent);

    return true;
}

bool FastIPCClient::Initialize(const char* sharedMemoryName)
{
    SharedMemory::OpenParameters params;
    params.accessMode   = SharedMemory::AccessMode_ReadWrite;
    params.globalName   = sharedMemoryName;
    params.minSizeBytes = RegionSize;
    params.openMode     = SharedMemory::OpenMode_OpenOnly;
    params.remoteMode   = SharedMemory::RemoteMode_ReadWrite;
    Scratch             = SharedMemoryFactory::GetInstance()->Open(params);
    IPCMessageIndex     = 1;

    if (!Scratch || Scratch->GetSizeI() < RegionSize)
    {
        OVR_ASSERT(false);
        LogError("Unable to open shared memory region");
        return false;
    }

    char* data = (char*)Scratch->GetData();

    // If unable to read handshake,
    if (!ReadInitialData(data))
    {
        return false;
    }

    return true;
}

FastIPCClient::~FastIPCClient()
{
}

bool FastIPCClient::Call(Net::BitStream* parameters, Net::BitStream* returnData, int timeoutMs)
{
    // If not initialized,
    if (!ReturnEvent.IsValid())
    {
        OVR_ASSERT(false);
        return false;
    }

    volatile unsigned char* scratch = (unsigned char*)Scratch->GetData();

    uint32_t bytesUsed = ((uint32_t)parameters->GetNumberOfBitsUsed() + 7) / 8;

    // If data is too long,
    if (bytesUsed > RegionSize - 4)
    {
        OVR_ASSERT(false);
        return false;
    }

    // First 4 bytes will be the size of the parameters
    // Note that this is for IPC so endian-ness is not important
    *(uint32_t*)(scratch + 4) = bytesUsed;

    // Copy data into place
    memcpy((char*)scratch + 8, parameters->GetData(), bytesUsed);

    // Don't allow read/write operations to move around this point
    MemoryBarrier();

    // Write message index
    *(volatile uint32_t*)scratch = IPCMessageIndex;

    // Wake the remote thread to service our request
    if (!::SetEvent(DataEvent.Get()))
    {
        OVR_ASSERT(false);
        return false;
    }

    // Wait for result of call:

    ++IPCMessageIndex;

    // Use the GetTickCount() API for low-resolution timing
    DWORD t0 = ::GetTickCount();
    int remaining = timeoutMs;

    // Forever,
    for (;;)
    {
        // Wait on the return event
        DWORD result = ::WaitForSingleObject(ReturnEvent.Get(), timeoutMs < 0 ? INFINITE : remaining);

        if (result == WAIT_FAILED)
        {
            int err = GetLastError();
            LogError("[FastIPC] Wait failed with error %d", err);
        }

        // If wait succeeded,
        if (result == WAIT_OBJECT_0)
        {
            if (*(volatile uint32_t*)scratch != IPCMessageIndex)
            {
                double callTimeoutStart = Timer::GetSeconds();

                while (*(volatile uint32_t*)scratch != IPCMessageIndex)
                {
                    if (Timer::GetSeconds() - callTimeoutStart > 1.)
                    {
                        LogError("[FastIPC] Timed out waiting for remote IPC message to be written.");
                        OVR_ASSERT(false);
                        return false;
                    }

                    Thread::YieldCurrentThread();
                }
            }

            _ReadBarrier();

            // Wrap the scratch buffer
            uint32_t len = *(uint32_t*)(scratch + 4);
            returnData->WrapBuffer((unsigned char*)scratch + 8, len);

            ++IPCMessageIndex;

            // Return true for success
            return true;
        }

        // If not waiting forever,
        if (timeoutMs > 0)
        {
            // If wait time has elapsed,
            int elapsed = ::GetTickCount() - t0;
            if (elapsed >= timeoutMs)
            {
                // Break out of loop returning false
                break;
            }

            // Calculate remaining wait time
            remaining = timeoutMs - elapsed;
        }

        // Continue waiting
    }

    return false;
}


}}} // namespace OVR::Service::Win32
