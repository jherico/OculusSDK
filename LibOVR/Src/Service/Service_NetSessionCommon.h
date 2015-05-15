/************************************************************************************

Filename    :   Service_NetSessionCommon.h
Content     :   Shared networking for service
Created     :   June 12, 2014
Authors     :   Kevin Jenkins, Chris Taylor

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

#ifndef OVR_Service_NetSessionCommon_h
#define OVR_Service_NetSessionCommon_h

#include "OVR_CAPI.h"
#include "OVR_Error.h"
#include "Kernel/OVR_Threads.h"
#include "Kernel/OVR_System.h"
#include "Net/OVR_RPC1.h"
#include "Net/OVR_BitStream.h"
#include <atomic>


namespace OVR {

class HMDInfo;

namespace Service {

using namespace Net;


//-----------------------------------------------------------------------------
// VirtualHmdId

// This is an identifier that is unique to each VirtualHmd object on the server
// side.  The client side uses this to opaquely reference those objects.

typedef int32_t VirtualHmdId;
static const int32_t InvalidVirtualHmdId = -1;

// Localhost-bound TCP port that the service listens on for VR apps
static const int VRServicePort = 30322; // 0x7672 = "vr" little-endian

// Stores the names of shared memory regions
struct SharedMemoryNames
{
    String Hmd;
    String Camera;
};

// HMDInfo section related to networking
struct HMDNetworkInfo
{
    HMDNetworkInfo() :
        NetId(InvalidVirtualHmdId),
        SharedMemoryName()
    {
    }

    // Network identifier for HMD
    VirtualHmdId NetId;

    // Names of the shared memory objects
    SharedMemoryNames SharedMemoryName;

    bool Serialize(BitStream& bs, bool write = true)
    {
        bs.Serialize(write, NetId);
        bs.Serialize(write, SharedMemoryName.Hmd);
        bs.Serialize(write, SharedMemoryName.Camera);

        String temp;
        bs.Serialize(write, temp);
        if(!bs.Serialize(write, temp))
            return false;

        return true;
    }

    // Assignment operator
    HMDNetworkInfo& operator=(HMDNetworkInfo const& rhs)
    {
        NetId = rhs.NetId;
        SharedMemoryName = rhs.SharedMemoryName;
        return *this;
    }
};


//-------------------------------------------------------------------------------------
// ***** NetSessionCommon

// Common part networking session/RPC implementation shared between client and server.

class NetSessionCommon : public Thread
{
protected:
    virtual void onSystemDestroy();
    virtual void onThreadDestroy();

public:
    NetSessionCommon();
    virtual ~NetSessionCommon();

    Plugins::RPC1* GetRPC1() const
    {
        return pRPC;
    }
    Session* GetSession() const
    {
        return pSession;
    }

    /// \brief Serializes the hmdInfo to or from the bitStream, depending on the write parameter.
    /// \return true if the serialization succeeded, false if it failed, which can occur only due to a memory allocation failure.
    /// \note This function doesn't generate an OVRError upon failure; it merely returns false and expects the caller to act accordingly.
    static bool SerializeHMDInfo(BitStream& bitStream, HMDInfo& hmdInfo, bool write = true);

    /// \brief Serializes the OVRError to or from the bitStream, depending on the write parameter.
    /// \return true if the serialization succeeded, false if it failed, which can occur only due to a memory allocation failure.
    /// \note This function doesn't generate an OVRError upon failure; it merely returns false and expects the caller to act accordingly.
    static bool SerializeOVRError(BitStream& bitStream, OVRError& ovrError, bool write = true);

public:
    // Getter/setter tools
    enum EGetterSetters
    {
        // Note: If this enumeration changes, then the Servce_NetSessionCommon.cpp
        // IsServiceProperty() function should be updated.

        EGetStringValue,
        EGetBoolValue,
        EGetIntValue,
        EGetNumberValue,
        EGetNumberValues,
        ESetStringValue,
        ESetBoolValue,
        ESetIntValue,
        ESetNumberValue,
        ESetNumberValues,

        ENumTypes
    };

    static const char* FilterKeyPrefix(const char* key);
    static bool        IsServiceProperty(EGetterSetters e, const char* key);

protected:
    std::atomic<bool> Terminated; // Thread termination flag
    Session*          pSession;   // Networking session
    Plugins::RPC1*    pRPC;       // Remote procedure calls object
};


}} // namespace OVR::Service

#endif // OVR_Service_NetSessionCommon_h
