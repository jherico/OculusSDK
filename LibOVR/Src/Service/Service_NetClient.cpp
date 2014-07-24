/************************************************************************************

Filename    :   Service_NetClient.cpp
Content     :   Client for service interface
Created     :   June 12, 2014
Authors     :   Michael Antonov, Kevin Jenkins, Chris Taylor

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.1 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.1 

Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#include "Service_NetClient.h"
#include "../Net/OVR_MessageIDTypes.h"

OVR_DEFINE_SINGLETON(OVR::Service::NetClient);

namespace OVR { namespace Service {

using namespace OVR::Net;


//// NetClient

NetClient::NetClient() :
    LatencyTesterAvailable(false)
{
    GetSession()->AddSessionListener(this);

    // Register RPC functions
    registerRPC();

    Start();

    PushDestroyCallbacks();
}

NetClient::~NetClient()
{
}

void NetClient::OnSystemDestroy()
{
    onSystemDestroy();
}

void NetClient::OnThreadDestroy()
{
    onThreadDestroy();
}

int NetClient::Run()
{
    SetThreadName("NetClient");

    while (!Terminated)
    {
        if (GetSession()->GetActiveSocketsCount()==0)
            Thread::MSleep(10);

        GetSession()->Poll(false);
    }

    return 0;
}

void NetClient::OnReceive(ReceivePayload* pPayload, ListenerReceiveResult* lrrOut)
{
    OVR_UNUSED(lrrOut);
    OVR_UNUSED(pPayload);

}

void NetClient::OnDisconnected(Connection* conn)
{
    OVR_UNUSED(conn);

    OVR_DEBUG_LOG(("[NetClient] Disconnected"));
}

void NetClient::OnConnectionAttemptFailed(Net::Connection* conn)
{
    OVR_UNUSED(conn);

    OVR_DEBUG_LOG(("[NetClient] OnConnectionAttemptFailed"));
}

void NetClient::OnConnected(Connection* conn)
{
    OVR_UNUSED(conn);

    OVR_DEBUG_LOG(("[NetClient] Connected to a server running version %d.%d.%d (my version=%d.%d.%d)",
        conn->RemoteMajorVersion, conn->RemoteMinorVersion, conn->RemotePatchVersion,
        RPCVersion_Major, RPCVersion_Minor, RPCVersion_Patch));
}

bool NetClient::Connect()
{
    // Set up bind parameters
	OVR::Net::BerkleyBindParameters bbp;
	bbp.Address = "::1"; // Bind to localhost only!
    bbp.blockingTimeout = 5000;
	OVR::Net::SockAddr sa;
	sa.Set("::1", VRServicePort, SOCK_STREAM);

    // Attempt to connect
    return GetSession()->ConnectPTCP(&bbp, &sa, true) == Net::SessionResult_OK;
}

void NetClient::Disconnect()
{
    GetSession()->Shutdown();
}

bool NetClient::IsConnected(bool attemptReconnect)
{
    // If it was able to connect,
    if (GetSession()->GetConnectionCount() > 0)
    {
        return true;
    }
    else if (attemptReconnect)
    {
        // Attempt to connect here
        Connect();

        // If it connected,
        if (GetSession()->GetConnectionCount() > 0)
        {
            return true;
        }
    }

    // No connections
    return false;
}

void NetClient::GetLocalProtocolVersion(int& major, int& minor, int& patch)
{
    major = RPCVersion_Major;
    minor = RPCVersion_Minor;
    patch = RPCVersion_Patch;
}

bool NetClient::GetRemoteProtocolVersion(int& major, int& minor, int& patch)
{
    Ptr<Connection> conn = GetSession()->GetConnectionAtIndex(0);

    if (conn)
    {
        major = conn->RemoteMajorVersion;
        minor = conn->RemoteMinorVersion;
        patch = conn->RemotePatchVersion;
        return true;
    }

    return false;
}


//// NetClient API

const char* NetClient::GetStringValue(VirtualHmdId hmd, const char* key, const char* default_val)
{
    if (!IsConnected(true))
        return "";

    ProfileGetValue1_Str = default_val;

    OVR::Net::BitStream bsOut, returnData;
    bsOut.Write(hmd);
    bsOut.Write(key);
    bsOut.Write(default_val);
    if (!GetRPC1()->CallBlocking("GetStringValue_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
    {
        OVR_ASSERT(false);
    }
    if (!returnData.Read(ProfileGetValue1_Str))
    {
        OVR_ASSERT(false);
    }
    return ProfileGetValue1_Str.ToCStr();
}
bool NetClient::GetBoolValue(VirtualHmdId hmd, const char* key, bool default_val)
{
    if (!IsConnected(true))
        return default_val;

    OVR::Net::BitStream bsOut, returnData;
    bsOut.Write(hmd);
    bsOut.Write(key);
    bsOut.Write(default_val);
    if (!GetRPC1()->CallBlocking("GetBoolValue_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
    {
        OVR_ASSERT(false);
    }
    uint8_t out = 0;
    if (!returnData.Read(out))
    {
        OVR_ASSERT(false);
    }
    return out != 0;
}
int NetClient::GetIntValue(VirtualHmdId hmd, const char* key, int default_val)
{
    if (!IsConnected(true))
        return default_val;

    OVR::Net::BitStream bsOut, returnData;
    bsOut.Write(hmd);
    bsOut.Write(key);
    bsOut.Write(default_val);
    if (!GetRPC1()->CallBlocking("GetIntValue_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
    {
        OVR_ASSERT(false);
    }
    int32_t out = (int32_t)default_val;
    if (!returnData.Read(out))
    {
        OVR_ASSERT(false);
    }
    return out;
}
double NetClient::GetNumberValue(VirtualHmdId hmd, const char* key, double default_val)
{
    if (!IsConnected(true))
        return default_val;

    OVR::Net::BitStream bsOut, returnData;
    bsOut.Write(hmd);
    bsOut.Write(key);
    bsOut.Write(default_val);
    if (!GetRPC1()->CallBlocking("GetNumberValue_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
    {
        OVR_ASSERT(false);
    }
    double out = 0.;
    returnData.Read(out);
    return out;
}
int NetClient::GetNumberValues(VirtualHmdId hmd, const char* key, double* values, int num_vals)
{
    if (!IsConnected(true))
        return 0;

    OVR::Net::BitStream bsOut, returnData;
    bsOut.Write(hmd);
    bsOut.Write(key);

    int32_t w = (int32_t)num_vals;
    bsOut.Write(w);

    if (!GetRPC1()->CallBlocking("GetNumberValues_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
    {
        OVR_ASSERT(false);
    }

    int32_t out = 0;
    if (!returnData.Read(out))
    {
        OVR_ASSERT(false);
    }
    OVR_ASSERT(out >= 0 && out <= num_vals);
    if (out < 0)
    {
        out = 0;
    }
    else if (out > num_vals)
    {
        out = num_vals;
    }

    for (int i = 0; i < out && i < num_vals; i++)
    {
        if (!returnData.Read(values[i]))
        {
            return i;
        }
    }

    return out;
}
void NetClient::SetStringValue(VirtualHmdId hmd, const char* key, const char* val)
{
    if (!IsConnected(true))
        return;

    OVR::Net::BitStream bsOut;
    bsOut.Write(hmd);
    bsOut.Write(key);

    bsOut.Write(val);

    if (!GetRPC1()->Signal("SetStringValue_1", &bsOut, GetSession()->GetConnectionAtIndex(0)))
    {
        OVR_ASSERT(false);
    }
}
void NetClient::SetBoolValue(VirtualHmdId hmd, const char* key, bool val)
{
    if (!IsConnected(true))
        return;

    OVR::Net::BitStream bsOut;
    bsOut.Write(hmd);
    bsOut.Write(key);

    uint8_t b = val ? 1 : 0;
    bsOut.Write(b);

    if (!GetRPC1()->Signal("SetBoolValue_1", &bsOut, GetSession()->GetConnectionAtIndex(0)))
    {
        OVR_ASSERT(false);
    }
}
void NetClient::SetIntValue(VirtualHmdId hmd, const char* key, int val)
{
    if (!IsConnected(true))
        return;

    OVR::Net::BitStream bsOut;
    bsOut.Write(hmd);
    bsOut.Write(key);

    int32_t w = (int32_t)val;
    bsOut.Write(w);

    if (!GetRPC1()->Signal("SetIntValue_1", &bsOut, GetSession()->GetConnectionAtIndex(0)))
    {
        OVR_ASSERT(false);
    }
}
void NetClient::SetNumberValue(VirtualHmdId hmd, const char* key, double val)
{
    if (!IsConnected(true))
        return;

    OVR::Net::BitStream bsOut;
    bsOut.Write(hmd);
    bsOut.Write(key);

    bsOut.Write(val);

    if (!GetRPC1()->Signal("SetNumberValue_1", &bsOut, GetSession()->GetConnectionAtIndex(0)))
    {
        OVR_ASSERT(false);
    }
}
void NetClient::SetNumberValues(VirtualHmdId hmd, const char* key, const double* vals, int num_vals)
{
    if (!IsConnected(true))
    {
        return;
    }

    OVR::Net::BitStream bsOut;
    bsOut.Write(hmd);
    bsOut.Write(key);

    int32_t w_count = (int32_t)num_vals;
    bsOut.Write(w_count);

    for (int i = 0; i < num_vals; i++)
    {
        bsOut.Write(vals[i]);
    }

    if (!GetRPC1()->Signal("SetNumberValues_1", &bsOut, GetSession()->GetConnectionAtIndex(0)))
    {
        OVR_ASSERT(false);
    }
}

int NetClient::Hmd_Detect()
{
    if (!IsConnected(true))
    {
        return 0;
    }

	OVR::Net::BitStream bsOut, returnData;

	if (!GetRPC1()->CallBlocking("Hmd_Detect_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
	{
		OVR_ASSERT(false);
		return 0;
	}

    int32_t out = 0;
    if (!returnData.Read(out))
    {
        OVR_ASSERT(false);
    }
	return out;
}

bool NetClient::Hmd_Create(int index, HMDNetworkInfo* netInfo)
{
    if (!IsConnected(true))
    {
        return false;
    }

	OVR::Net::BitStream bsOut, returnData;

    int32_t w = (int32_t)index;
	bsOut.Write(w);

#ifdef OVR_OS_WIN32
    // Need the Pid for driver mode
    DWORD pid = GetCurrentProcessId();
    bsOut.Write(pid);
#endif

	if (!GetRPC1()->CallBlocking("Hmd_Create_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
	{
		OVR_ASSERT(false);
		return false;
	}

	return netInfo->Deserialize(&returnData);
}

void NetClient::Hmd_AttachToWindow(VirtualHmdId hmd, void* hWindow)
{
    if (!IsConnected())
        return;

    OVR::Net::BitStream bsOut;
    bsOut.Write(hmd);

    UInt64 hWinWord = (UPInt)hWindow;
    bsOut.Write(hWinWord);

    if (!GetRPC1()->CallBlocking("Hmd_AttachToWindow_1", &bsOut, GetSession()->GetConnectionAtIndex(0)))
    {
        OVR_ASSERT(false);
    }
}

void NetClient::Hmd_Release(VirtualHmdId hmd)
{
	if (!IsConnected())
		return;

	OVR::Net::BitStream bsOut;
	bsOut.Write(hmd);
	if (!GetRPC1()->CallBlocking("Hmd_Release_1", &bsOut, GetSession()->GetConnectionAtIndex(0)))
	{
		OVR_ASSERT(false);
	}
}

// Last string is cached locally.
const char* NetClient::Hmd_GetLastError(VirtualHmdId hmd)
{
    if (!IsConnected())
    {
        return Hmd_GetLastError_Str.ToCStr();
    }

	OVR::Net::BitStream bsOut;
	bsOut.Write(hmd);
	if (!GetRPC1()->CallBlocking("Hmd_GetLastError_1", &bsOut, GetSession()->GetConnectionAtIndex(0)))
	{
		OVR_ASSERT(false);
		return Hmd_GetLastError_Str.ToCStr();
	}
    if (!bsOut.Read(Hmd_GetLastError_Str))
    {
        OVR_ASSERT(false);
    }
	return Hmd_GetLastError_Str.ToCStr();
}


// Fills in description about HMD; this is the same as filled in by ovrHmd_Create.
// The actual descriptor is a par
bool NetClient::Hmd_GetHmdInfo(VirtualHmdId hmd, HMDInfo* hmdInfo)
{
    if (!IsConnected())
    {
        return false;
    }

	OVR::Net::BitStream bsOut, returnData;
	bsOut.Write(hmd);
	if (!GetRPC1()->CallBlocking("Hmd_GetHmdInfo_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
	{
		OVR_ASSERT(false);
		return false;
	}

    return NetSessionCommon::DeserializeHMDInfo(&returnData, hmdInfo);
}


//-------------------------------------------------------------------------------------
unsigned int NetClient::Hmd_GetEnabledCaps(VirtualHmdId hmd)
{
	if (!IsConnected())
    {
        return 0;
    }

	OVR::Net::BitStream bsOut, returnData;
	bsOut.Write(hmd);
	if (!GetRPC1()->CallBlocking("Hmd_GetEnabledCaps_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
	{
		OVR_ASSERT(false);
		return 0;
	}

    uint32_t c = 0;
    if (!returnData.Read(c))
    {
        OVR_ASSERT(false);
    }
	return c;
}

// Returns new caps after modification
unsigned int NetClient::Hmd_SetEnabledCaps(VirtualHmdId hmd, unsigned int hmdCaps)
{
	if (!IsConnected())
    {
        return 0;
    }

	OVR::Net::BitStream bsOut, returnData;
	bsOut.Write(hmd);

    uint32_t c = (uint32_t)hmdCaps;
	bsOut.Write(c);

	if (!GetRPC1()->CallBlocking("Hmd_SetEnabledCaps_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
	{
		OVR_ASSERT(false);
		return 0;
	}

    c = 0;
    if (!returnData.Read(c))
    {
        OVR_ASSERT(false);
    }
    return c;
}


//-------------------------------------------------------------------------------------
// *** Tracking Setup

bool NetClient::Hmd_ConfigureTracking(VirtualHmdId hmd, unsigned supportedCaps, unsigned requiredCaps)
{
	if (!IsConnected())
    {
        return false;
    }

	OVR::Net::BitStream bsOut, returnData;
	bsOut.Write(hmd);

    uint32_t w_sc = supportedCaps;
    bsOut.Write(w_sc);
    uint32_t w_rc = requiredCaps;
    bsOut.Write(w_rc);

	if (!GetRPC1()->CallBlocking("Hmd_ConfigureTracking_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
	{
		OVR_ASSERT(false);
		return false;
	}

    uint8_t b;
    if (!returnData.Read(b))
    {
        OVR_ASSERT(false);
    }

	return b != 0;
}


void NetClient::Hmd_ResetTracking(VirtualHmdId hmd)
{
	if (!IsConnected())
    {
        return;
    }

	OVR::Net::BitStream bsOut;
	bsOut.Write(hmd);
	if (!GetRPC1()->CallBlocking("Hmd_ResetTracking_1", &bsOut, GetSession()->GetConnectionAtIndex(0)))
	{
		OVR_ASSERT(false);
		return;
	}
}

bool NetClient::LatencyUtil_ProcessInputs(double startTestSeconds, unsigned char rgbColorOut[3])
{
    if (!IsConnected())
    {
        return false;
    }

    if (!LatencyTesterAvailable)
    {
        return false;
    }

    OVR::Net::BitStream bsOut, returnData;
    bsOut.Write(startTestSeconds);
    if (!GetRPC1()->CallBlocking("LatencyUtil_ProcessInputs_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
    {
        OVR_ASSERT(false);
        return false;
    }

    uint8_t u;
    returnData.Read(u);
    rgbColorOut[0] = u;
    returnData.Read(u);
    rgbColorOut[1] = u;
    if (!returnData.Read(u))
    {
        return false;
    }
    rgbColorOut[2] = u;

    return true;
}

const char* NetClient::LatencyUtil_GetResultsString()
{
    if (!IsConnected())
    {
        return NULL;
    }

    OVR::Net::BitStream bsOut, returnData;
    if (!GetRPC1()->CallBlocking("LatencyUtil_GetResultsString_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
    {
        OVR_ASSERT(false);
        return NULL;
    }

    if (!returnData.Read(LatencyUtil_GetResultsString_Str))
    {
        OVR_ASSERT(false);
    }

    return LatencyUtil_GetResultsString_Str.ToCStr();
}

bool NetClient::ShutdownServer()
{
    if (!IsConnected())
    {
        return false;
    }

    OVR::Net::BitStream bsOut;
    GetRPC1()->BroadcastSignal("Shutdown_1", &bsOut);

    return true;
}


//// Push Notifications:

void NetClient::registerRPC()
{
#define RPC_REGISTER_SLOT(observerScope, functionName) \
    observerScope.SetHandler(OVR::Net::Plugins::RPCSlot::FromMember<NetClient, &NetClient::functionName>(this)); pRPC->RegisterSlot(OVR_STRINGIZE(functionName), observerScope);

    // Register RPC functions:
    RPC_REGISTER_SLOT(InitialServerStateScope, InitialServerState_1);
    RPC_REGISTER_SLOT(LatencyTesterAvailableScope, LatencyTesterAvailable_1);

}

void NetClient::InitialServerState_1(BitStream* userData, ReceivePayload* pPayload)
{
    LatencyTesterAvailable_1(userData, pPayload);
}

void NetClient::LatencyTesterAvailable_1(BitStream* userData, ReceivePayload* pPayload)
{
    OVR_UNUSED(pPayload);

    uint8_t b = 0;
    if (!userData->Read(b))
    {
        OVR_ASSERT(false);
        return;
    }

    LatencyTesterAvailable = (b != 0);
}



}} // namespace OVR::Service
