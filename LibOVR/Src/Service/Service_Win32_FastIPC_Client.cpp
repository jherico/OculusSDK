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
#include "Service_NetSessionCommon.h"

namespace OVR { namespace Service { namespace Win32 {

using namespace OVR::Net;


//-----------------------------------------------------------------------------
// FastIPCKey

bool FastIPCKey::Serialize(bool write, BitStream& bs)
{
    return bs.Serialize(write, SharedMemoryName);
}


//-----------------------------------------------------------------------------
// FastIPCClient

FastIPCClient::FastIPCClient() :
    IsInitialized(false),
    IPCKey(),
    Scratch(),
    DataEvent(),
    ReturnEvent(),
    IPCMessageIndex(0)
{
}

OVRError FastIPCClient::readInitialData(const char* buffer)
{
    uint32_t magic    = *(uint32_t*)(buffer);
    uint32_t verMajor = *(uint32_t*)(buffer + 4);
    uint32_t verMinor = *(uint32_t*)(buffer + 8);

    if (magic != Magic)
    {
        return OVR_MAKE_ERROR(ovrError_Initialize, "IPC magic does not match");
    }

    if (verMajor != MajorVersion)
    {
        return OVR_MAKE_ERROR(ovrError_Initialize, "IPC major version mismatch");
    }

    if (verMinor < MinorVersion)
    {
        return OVR_MAKE_ERROR(ovrError_Initialize, "IPC remote minor version too old for our feature level");
    }

    HANDLE remoteDataEvent   = (HANDLE)*(uint64_t*)(buffer + 12);
    HANDLE remoteReturnEvent = (HANDLE)*(uint64_t*)(buffer + 20);
    pid_t serverProcessId    = (pid_t) *(uint64_t*)(buffer + 28);
    OVR_UNUSED(serverProcessId);

    // Open server process for duplication
    ScopedProcessHANDLE proc = ::OpenProcess(PROCESS_DUP_HANDLE, FALSE, serverProcessId);

    if (!proc.IsValid())
    {
        return OVR_MAKE_ERROR(ovrError_Initialize, "IPC unable to open server process. Did it die?");
    }

    if (!::DuplicateHandle(proc.Get(), remoteDataEvent,
            ::GetCurrentProcess(), &DataEvent.GetRawRef(), GENERIC_ALL, FALSE, 0) ||
        !::DuplicateHandle(proc.Get(), remoteReturnEvent,
            ::GetCurrentProcess(), &ReturnEvent.GetRawRef(), GENERIC_ALL, FALSE, 0))
    {
        return OVR_MAKE_ERROR(ovrError_Initialize, "IPC unable to duplicate server event handles. Did it die?");
    }

    if (!DataEvent.IsValid() || !ReturnEvent.IsValid())
    {
        return OVR_MAKE_ERROR(ovrError_Initialize, "IPC corrupt data");
    }

    return OVRError::Success();
}

OVRError FastIPCClient::Initialize(const FastIPCKey& key)
{
    // Make sure we release the old IPCKey handles
    Shutdown();

    SharedMemory::OpenParameters params;
    params.accessMode   = SharedMemory::AccessMode_ReadWrite;
    params.globalName   = key.SharedMemoryName;
    params.minSizeBytes = RegionSize;
    params.openMode     = SharedMemory::OpenMode_OpenOnly;
    params.remoteMode   = SharedMemory::RemoteMode_ReadWrite;
    Scratch             = SharedMemoryFactory::GetInstance()->Open(params);
    IPCMessageIndex     = 1;

    // If unable to open,
    if (!Scratch || Scratch->GetSizeI() < RegionSize)
    {
        return OVR_MAKE_ERROR(ovrError_Initialize, "Unable to open shared memory region");
    }

    OVRError err = readInitialData((char*)Scratch->GetData());

    // If unable to read handshake,
    if (!err.Succeeded())
    {
        return err;
    }

    IPCKey = key;

    return OVRError::Success();
}

void FastIPCClient::Shutdown()
{
    DataEvent     = nullptr;
    ReturnEvent   = nullptr;
    IsInitialized = false;

    Scratch         = nullptr; // Extraneous
    IPCKey          = FastIPCKey(); // Extraneous
    IPCMessageIndex = 0; // Extraneous
}

FastIPCClient::~FastIPCClient()
{
}

OVRError FastIPCClient::Call(BitStream& parameters, BitStream& returnData, int timeoutMs)
{
    // TBD: Currently timeouts are not recovered gracefully, so do not use them!
    // Please pardon our dust. -cat
    OVR_ASSERT(timeoutMs == -1);

    // If not initialized,
    if (!IsInitialized)
    {
        return OVR_MAKE_ERROR(ovrError_NotInitialized, "IPC not initialized");
    }

    volatile unsigned char* scratch = (unsigned char*)Scratch->GetData();

    uint32_t bytesUsed = ((uint32_t)parameters.GetNumberOfBitsUsed() + 7) / 8;

    // If data is too long,
    if (bytesUsed > RegionSize - 4)
    {
        return OVR_MAKE_ERROR_F(ovrError_InvalidParameter, "IPC region size %d too small to fit buffer of size %d bytes", RegionSize, bytesUsed);
    }

    // First 4 bytes will be the size of the parameters
    // Note that this is for IPC so endian-ness is not important
    *(uint32_t*)(scratch + 4) = bytesUsed;

    // Copy data into place
    memcpy((char*)scratch + 8, parameters.GetData(), bytesUsed);

    // Don't allow read/write operations to move around this point
    MemoryBarrier();

    // Write message index
    *(volatile uint32_t*)scratch = IPCMessageIndex;

    // Wake the remote thread to service our request
    if (!::SetEvent(DataEvent.Get()))
    {
        return OVR_MAKE_SYS_ERROR(ovrError_ServiceError, ::GetLastError(), "IPC set event failed");
    }

    // Wait for result of call:

    ++IPCMessageIndex;

    // Use the GetTickCount() API for low-resolution timing
    ULONGLONG t0 = ::GetTickCount64();
    int remaining = timeoutMs;

    // Breaks out of this loop when a timeout occurs.
    for (;;)
    {
        // Wait on the return event
        DWORD result = ::WaitForSingleObject(ReturnEvent.Get(), timeoutMs < 0 ? INFINITE : (DWORD)remaining);

        if (result == WAIT_FAILED)
        {
            return OVR_MAKE_SYS_ERROR(ovrError_ServiceError, ::GetLastError(), "IPC wait failed");
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
                        break;
                    }

                    Thread::YieldCurrentThread();
                }
            }

            // If message index is synchronized,
            if (*(volatile uint32_t*)scratch == IPCMessageIndex)
            {
                _ReadBarrier();

                // Wrap the scratch buffer
                uint32_t len = *(uint32_t*)(scratch + 4);
                returnData.WrapBuffer((unsigned char*)scratch + 8, len);

                ++IPCMessageIndex;

                OVRError err;
                if (!Service::NetSessionCommon::SerializeOVRError(returnData, err, false))
                {
                    return OVR_MAKE_ERROR(ovrError_ServiceError, "IPC corrupt");
                }

                // Return error code from server
                return err;
            }
        }

        // If not waiting forever,
        if (timeoutMs > 0)
        {
            // If wait time has elapsed,
            int elapsed = (int)(::GetTickCount64() - t0); // elapsed is guaranteed to be >= 0.
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

    return OVR_MAKE_ERROR(ovrError_Timeout, "IPC timeout");
}


}}} // namespace OVR::Service::Win32
