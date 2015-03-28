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

#include <atomic>

#include "OVR_CAPI.h"
#include "Kernel/OVR_Threads.h"
#include "Kernel/OVR_System.h"
#include "Net/OVR_RPC1.h"
#include "Net/OVR_BitStream.h"


namespace OVR {

class HMDInfo;

namespace Service {


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
		NetId(InvalidVirtualHmdId)
	{
	}

	// Network identifier for HMD
	VirtualHmdId NetId;

	// Names of the shared memory objects
	SharedMemoryNames SharedMemoryName;

    bool Serialize(Net::BitStream* bs, bool write = true)
	{
        bs->Serialize(write, NetId);
        bs->Serialize(write, SharedMemoryName.Hmd);
        if (!bs->Serialize(write, SharedMemoryName.Camera))
            return false;
        return true;
	}

    // Assignment operator
    HMDNetworkInfo& operator=(HMDNetworkInfo const & rhs)
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

	Net::Plugins::RPC1* GetRPC1() const
    {
        return pRPC;
    }
	Net::Session* GetSession() const
    {
        return pSession;
    }

	static bool SerializeHMDInfo(Net::BitStream* bitStream, HMDInfo* hmdInfo, bool write = true);

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
    static bool IsServiceProperty(EGetterSetters e, const char* key);

protected:
    std::atomic<bool>   Terminated; // Thread termination flag
    Net::Session*       pSession;   // Networking session
    Net::Plugins::RPC1* pRPC;       // Remote procedure calls object
};


}} // namespace OVR::Service

#endif // OVR_Service_NetSessionCommon_h
