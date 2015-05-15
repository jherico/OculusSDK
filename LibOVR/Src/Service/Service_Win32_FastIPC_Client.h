/************************************************************************************

Filename    :   OVR_Service_Win32_FastIPC_Client.h
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

#ifndef OVR_Service_Win32_FastIPC_Client_h
#define OVR_Service_Win32_FastIPC_Client_h

#include "Kernel/OVR_SharedMemory.h"
#include "Net/OVR_RPC1.h"
#include "Kernel/OVR_Win32_IncludeWindows.h"

namespace OVR { namespace Service { namespace Win32 {

using namespace Net;


//-----------------------------------------------------------------------------
// FastIPCKey
//
// Key to pass to the client to be able to connect to server IPC.

struct FastIPCKey
{
    String SharedMemoryName;

    bool IsValid() const
    {
        return SharedMemoryName.GetLengthI() > 0;
    }

    void Invalidate()
    {
        SharedMemoryName.Clear();
    }

    bool Serialize(bool write, BitStream& bs);
};


//-----------------------------------------------------------------------------
// FastIPCClient
//
// This class implements the client side for connectionless IPC messaging.
//
// The client reads the shared memory name provided and retrieves the data
// and return event handles.  It can push data to the server synchronously
// by signaling the data handle and waiting on the return handle.

class FastIPCClient
{
public:
    FastIPCClient();
    ~FastIPCClient();

    // Call this to initialize the shared memory section
    OVRError Initialize(const FastIPCKey& key);

    // Tear down the IPC connection
    void Shutdown();

    // Check if initialized
    bool IsValid() const
    {
        return IsInitialized;
    }

    // Make a blocking call to the server
    // Pass -1 for the timeout to wait forever
    OVRError Call(BitStream& parameters, BitStream& returnData, int timeoutMs = -1);

public:
    static const int      RegionSize = 8192;
    static const uint32_t Magic      = 0xfe67bead;

    // Semantic versioning
    static const uint32_t MajorVersion = 3;
    static const uint32_t MinorVersion = 0;

protected:
    bool              IsInitialized;
    FastIPCKey        IPCKey;
    Ptr<SharedMemory> Scratch;
    ScopedEventHANDLE DataEvent, ReturnEvent;
    uint32_t          IPCMessageIndex;

protected:
    // Read the initial data from the buffer.
    OVRError readInitialData(const char* buffer);
};


}}} // namespace OVR::Service::Win32

#endif // OVR_Service_Win32_FastIPC_Client_h
