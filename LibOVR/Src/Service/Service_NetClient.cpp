/************************************************************************************

Filename    :   Service_NetClient.cpp
Content     :   Client for service interface
Created     :   June 12, 2014
Authors     :   Michael Antonov, Kevin Jenkins, Chris Taylor

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

#include "Service_NetClient.h"
#include "Net/OVR_MessageIDTypes.h"

#if defined (OVR_OS_MAC) || defined(OVR_OS_LINUX)
#define GetCurrentProcessId getpid
#endif

OVR_DEFINE_SINGLETON(OVR::Service::NetClient);

namespace OVR { namespace Service {

using namespace OVR::Net;


// Default connection timeout in milliseconds.
static const int kDefaultConnectionTimeoutMS = 5000; // Timeout in Milliseconds


//// NetClient

NetClient::NetClient() :
    LatencyTesterAvailable(false), HMDCount(-1), EdgeTriggeredHMDCount(false)
{
    SetDefaultParameters();

    GetSession()->AddSessionListener(this);

    // Register RPC functions
    registerRPC();

    Start();

    // Must be at end of function
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

void NetClient::SetDefaultParameters()
{
    ServerOptional      = false;
    ExtraDebugging      = false;
    ConnectionTimeoutMS = kDefaultConnectionTimeoutMS;
}

void NetClient::ApplyParameters(ovrInitParams const* params)
{
    SetDefaultParameters();

    // If connection timeout is specified,
    if (params->ConnectionTimeoutMS > 0)
    {
        ConnectionTimeoutMS = params->ConnectionTimeoutMS;
    }

    ServerOptional = (params->Flags & ovrInit_ServerOptional) != 0;
    ExtraDebugging = (params->Flags & ovrInit_Debug) != 0;
}

int NetClient::Run()
{
    SetThreadName("NetClient");

    while (!Terminated.load(std::memory_order_relaxed))
    {
        // There is no watchdog here because the watchdog is part of the private code.

        GetSession()->Poll(false);

        if (GetSession()->GetActiveSocketsCount() == 0)
        {
            Thread::MSleep(100);
        }
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

    EdgeTriggeredHMDCount = false;
}

void NetClient::OnConnected(Connection* conn)
{
    OVR_UNUSED(conn);

    OVR_DEBUG_LOG(("[NetClient] Connected to the server running SDK version " \
        "(prod=%d).%d.%d(req=%d).%d(build=%d), RPC version %d.%d.%d. " \
        "Client SDK version (prod=%d).%d.%d(req=%d).%d.(build=%d), RPC version=%d.%d.%d",
        conn->RemoteCodeVersion.ProductVersion,
        conn->RemoteCodeVersion.MajorVersion,
        conn->RemoteCodeVersion.MinorVersion,
        conn->RemoteCodeVersion.RequestedMinorVersion,
        conn->RemoteCodeVersion.PatchVersion,
        conn->RemoteCodeVersion.BuildNumber,
        conn->RemoteMajorVersion, conn->RemoteMinorVersion, conn->RemotePatchVersion,
        OVR_PRODUCT_VERSION,
        OVR_MAJOR_VERSION,
        OVR_MINOR_VERSION,
        RuntimeSDKVersion.RequestedMinorVersion,
        OVR_PATCH_VERSION,
        OVR_BUILD_NUMBER,
        RPCVersion_Major, RPCVersion_Minor, RPCVersion_Patch));

    EdgeTriggeredHMDCount = false;
}

bool NetClient::Connect(bool blocking)
{
    // If server is optional,
    if (ServerOptional && !Session::IsSingleProcess())
    {
        blocking = false; // Poll: Do not block
    }

    // Set up bind parameters
    OVR::Net::BerkleyBindParameters bbp;
    bbp.Address = "::1"; // Bind to localhost only!
    bbp.blockingTimeout = ConnectionTimeoutMS;
    OVR::Net::SockAddr sa;
    sa.Set("::1", VRServicePort, SOCK_STREAM);

    // Attempt to connect
    OVR::Net::SessionResult result = GetSession()->ConnectPTCP(&bbp, &sa, blocking);

    // Already connected counts as success too
    return result == Net::SessionResult_OK ||
           result == Net::SessionResult_AlreadyConnected ||
           result == Net::SessionResult_ConnectInProgress;
}

void NetClient::Disconnect()
{
    GetSession()->Shutdown();
}

bool NetClient::IsConnected(bool attemptReconnect, bool blockOnReconnect)
{
    // If it was able to connect,
    if (GetSession()->ConnectionSuccessful())
    {
        return true;
    }
    else if (attemptReconnect)
    {
        // Attempt to connect here
        Connect(blockOnReconnect);

        // If it connected,
        if (GetSession()->ConnectionSuccessful())
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

void NetClient::GetLocalSDKVersion(SDKVersion& requestedSDKVersion)
{
    requestedSDKVersion = RuntimeSDKVersion;
}

bool NetClient::GetRemoteSDKVersion(SDKVersion& remoteSDKVersion)
{
    Ptr<Connection> conn = GetSession()->GetConnectionAtIndex(0);

    if (conn)
    {
        remoteSDKVersion = conn->RemoteCodeVersion;
        return true;
    }
    
    return false;
}


//// NetClient API

const char* NetClient::GetStringValue(VirtualHmdId hmd, const char* key, const char* default_val)
{
    // If a null value is provided,
    if (!default_val)
    {
        default_val = "";
    }

    if (!IsConnected(true, true))
    {
        return default_val;
    }

    ProfileGetValue1_Str = default_val;

    OVR::Net::BitStream bsOut, returnData;
    bsOut.Write(hmd);
    bsOut.Write(key);
    bsOut.Write(default_val);
    if (!GetRPC1()->CallBlocking("GetStringValue_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
    {
        return default_val;
    }
    if (!returnData.Read(ProfileGetValue1_Str))
    {
        OVR_ASSERT(false); //This assert will hit if you tamper or restart the service mid-call.
    }
    return ProfileGetValue1_Str.ToCStr();
}
bool NetClient::GetBoolValue(VirtualHmdId hmd, const char* key, bool default_val)
{
    if (!IsConnected(true, true))
    {
        return default_val;
    }

    OVR::Net::BitStream bsOut, returnData;
    bsOut.Write(hmd);
    bsOut.Write(key);
    bsOut.Write(default_val);
    if (!GetRPC1()->CallBlocking("GetBoolValue_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
    {
        return default_val;
    }
    uint8_t out = 0;
    if (!returnData.Read(out))
    {
        OVR_ASSERT(false); //This assert will hit if you tamper or restart the service mid-call.
    }
    return out != 0;
}
int NetClient::GetIntValue(VirtualHmdId hmd, const char* key, int default_val)
{
    if (!IsConnected(true, true))
    {
        return default_val;
    }

    OVR::Net::BitStream bsOut, returnData;
    bsOut.Write(hmd);
    bsOut.Write(key);
    bsOut.Write(default_val);
    if (!GetRPC1()->CallBlocking("GetIntValue_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
    {
        return default_val;
    }
    int32_t out = (int32_t)default_val;
    if (!returnData.Read(out))
    {
        OVR_ASSERT(false); //This assert will hit if you tamper or restart the service mid-call.
    }
    return out;
}
double NetClient::GetNumberValue(VirtualHmdId hmd, const char* key, double default_val)
{
    if (!IsConnected(true, true))
    {
        return default_val;
    }

    OVR::Net::BitStream bsOut, returnData;
    bsOut.Write(hmd);
    bsOut.Write(key);
    bsOut.Write(default_val);
    if (!GetRPC1()->CallBlocking("GetNumberValue_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
    {
        return default_val;
    }
    double out = 0.;
    if (!returnData.Read(out))
    {
        OVR_ASSERT(false); //This assert will hit if you tamper or restart the service mid-call.
    }
    return out;
}
int NetClient::GetNumberValues(VirtualHmdId hmd, const char* key, double* values, int num_vals)
{
    if (!IsConnected(true, true))
    {
        return 0;
    }

    OVR::Net::BitStream bsOut, returnData;
    bsOut.Write(hmd);
    bsOut.Write(key);

    int32_t w = (int32_t)num_vals;
    bsOut.Write(w);

    if (!GetRPC1()->CallBlocking("GetNumberValues_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
    {
        return 0;
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

bool NetClient::SetStringValue(VirtualHmdId hmd, const char* key, const char* val)
{
    if (!IsConnected(true, true))
    {
        return false;
    }

    OVR::Net::BitStream bsOut;
    bsOut.Write(hmd);
    bsOut.Write(key);

    bsOut.Write(val);

    if (!GetRPC1()->Signal("SetStringValue_1", &bsOut, GetSession()->GetConnectionAtIndex(0)))
    {
        return false;
    }

    return true;
}

bool NetClient::SetBoolValue(VirtualHmdId hmd, const char* key, bool val)
{
    if (!IsConnected(true, true))
    {
        return false;
    }

    OVR::Net::BitStream bsOut;
    bsOut.Write(hmd);
    bsOut.Write(key);

    uint8_t b = val ? 1 : 0;
    bsOut.Write(b);

    if (!GetRPC1()->Signal("SetBoolValue_1", &bsOut, GetSession()->GetConnectionAtIndex(0)))
    {
        return false;
    }

    return true;
}

bool NetClient::SetIntValue(VirtualHmdId hmd, const char* key, int val)
{
    if (!IsConnected(true, true))
    {
        return false;
    }

    OVR::Net::BitStream bsOut;
    bsOut.Write(hmd);
    bsOut.Write(key);

    int32_t w = (int32_t)val;
    bsOut.Write(w);

    if (!GetRPC1()->Signal("SetIntValue_1", &bsOut, GetSession()->GetConnectionAtIndex(0)))
    {
        return false;
    }

    return true;
}

bool NetClient::SetNumberValue(VirtualHmdId hmd, const char* key, double val)
{
    if (!IsConnected(true, true))
    {
        return false;
    }

    OVR::Net::BitStream bsOut;
    bsOut.Write(hmd);
    bsOut.Write(key);

    bsOut.Write(val);

    if (!GetRPC1()->Signal("SetNumberValue_1", &bsOut, GetSession()->GetConnectionAtIndex(0)))
    {
        return false;
    }

    return true;
}

bool NetClient::SetNumberValues(VirtualHmdId hmd, const char* key, const double* vals, int num_vals)
{
    if (!IsConnected(true, true))
    {
        return false;
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
        return false;
    }

    return true;
}

int NetClient::Hmd_Detect()
{
    if (!IsConnected(true, false))
    {
        return -1;
    }

    // If using edge-triggered HMD counting,
    if (EdgeTriggeredHMDCount)
    {
        // Return the last update from the server
        return HMDCount;
    }

    // Otherwise: We need to ask the first time

    OVR::Net::BitStream bsOut, returnData;

    if (!GetRPC1()->CallBlocking("Hmd_Detect_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
    {
        return -1;
    }

    int32_t out = 0;
    if (!returnData.Read(out))
    {
        OVR_ASSERT(false); //This assert will hit if you tamper or restart the service mid-call.
    }
    HMDCount = out;
    EdgeTriggeredHMDCount = true;
    return out;
}

bool NetClient::Hmd_Create(int index, HMDNetworkInfo* netInfo)
{
    if (!IsConnected(true, true))
    {
        return false;
    }

    OVR::Net::BitStream bsOut, returnData;

    int32_t w = (int32_t)index;
    bsOut.Write(w);

    // Need the Pid for driver mode
    pid_t pid = GetCurrentProcessId();
    bsOut.Write(pid);

    if (!GetRPC1()->CallBlocking("Hmd_Create_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
    {
        return false;
    }

    return netInfo->Serialize(&returnData, false);
}

bool NetClient::GetDriverMode(bool& driverInstalled, bool& compatMode, bool& hideDK1Mode)
{
    if (!IsConnected(true, true))
    {
        return false;
    }

    OVR::Net::BitStream bsOut, returnData;

    bsOut.Write(InvalidVirtualHmdId);

    if (!GetRPC1()->CallBlocking("GetDriverMode_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
    {
        return false;
    }

    int32_t w_driverInstalled = 0;
    int32_t w_compatMode = 0;
    int32_t w_hideDK1Mode = 0;
    returnData.Read(w_driverInstalled);
    returnData.Read(w_compatMode);
    if (!returnData.Read(w_hideDK1Mode))
    {
        return false;
    }

    driverInstalled = w_driverInstalled != 0;
    compatMode = w_compatMode != 0;
    hideDK1Mode = w_hideDK1Mode != 0;
    return true;
}

bool NetClient::SetDriverMode(bool compatMode, bool hideDK1Mode)
{
    if (!IsConnected(true, true))
    {
        return false;
    }

    OVR::Net::BitStream bsOut, returnData;

    bsOut.Write(InvalidVirtualHmdId);

    int32_t w_compatMode, w_hideDK1Mode;
    w_compatMode = compatMode ? 1 : 0;
    w_hideDK1Mode = hideDK1Mode ? 1 : 0;
    bsOut.Write(w_compatMode);
    bsOut.Write(w_hideDK1Mode);

    if (!GetRPC1()->CallBlocking("SetDriverMode_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
    {
        return false;
    }

    int32_t out = 0;
    if (!returnData.Read(out))
    {
        OVR_ASSERT(false);
        return false;
    }

    return out != 0;
}

bool NetClient::Hmd_AttachToWindow(VirtualHmdId hmd, void* hWindow)
{
    if (!IsConnected(false, false))
    {
        return false;
    }

    OVR::Net::BitStream bsOut;
    bsOut.Write(hmd);

    uint64_t hWinWord = 0;
    #ifdef OVR_OS_LINUX
        if (hWindow == NULL)
        {
            return false;
        }
        hWinWord = *(uint64_t *)&hWindow;
    #elif defined(OVR_OS_WIN32)
        hWinWord = (UPInt)hWindow;
    #else
        OVR_UNUSED(hWindow);
    #endif

    bsOut.Write(hWinWord);

    if (!GetRPC1()->CallBlocking("Hmd_AttachToWindow_1", &bsOut, GetSession()->GetConnectionAtIndex(0)))
    {
        return false;
    }

    return true;
}

void NetClient::Hmd_Release(VirtualHmdId hmd)
{
    if (!IsConnected(false, false))
    {
        return;
    }

    OVR::Net::BitStream bsOut;
    bsOut.Write(hmd);
    bool result = GetRPC1()->CallBlocking("Hmd_Release_1", &bsOut, GetSession()->GetConnectionAtIndex(0));
    OVR_ASSERT_AND_UNUSED(result, result);
}

void NetClient::SetLastError(String str)
{
    Hmd_GetLastError_Str = str;
}

// Last string is cached locally.
const char* NetClient::Hmd_GetLastError(VirtualHmdId hmd)
{
    if (hmd == InvalidVirtualHmdId || !IsConnected(false, false))
    {
        return Hmd_GetLastError_Str.ToCStr();
    }

    OVR::Net::BitStream bsOut, returnData;
    bsOut.Write(hmd);
    if (!GetRPC1()->CallBlocking("Hmd_GetLastError_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
    {
        return Hmd_GetLastError_Str.ToCStr();
    }
    if (!returnData.Read(Hmd_GetLastError_Str))
    {
        OVR_ASSERT(false);
    }
    return Hmd_GetLastError_Str.ToCStr();
}


// Fills in description about HMD; this is the same as filled in by ovrHmd_Create.
// The actual descriptor is a par
bool NetClient::Hmd_GetHmdInfo(VirtualHmdId hmd, HMDInfo* hmdInfo)
{
    if (!IsConnected(false, false))
    {
        return false;
    }

    OVR::Net::BitStream bsOut, returnData;
    bsOut.Write(hmd);
    if (!GetRPC1()->CallBlocking("Hmd_GetHmdInfo_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
    {
        return false;
    }

    return NetSessionCommon::SerializeHMDInfo(&returnData, hmdInfo, false);
}


//-------------------------------------------------------------------------------------
unsigned int NetClient::Hmd_GetEnabledCaps(VirtualHmdId hmd)
{
    if (!IsConnected(false, false))
    {
        return 0;
    }

    OVR::Net::BitStream bsOut, returnData;
    bsOut.Write(hmd);
    if (!GetRPC1()->CallBlocking("Hmd_GetEnabledCaps_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
    {
        return 0;
    }

    uint32_t c = 0;
    if (!returnData.Read(c))
    {
        OVR_ASSERT(false); //This assert will hit if you tamper or restart the service mid-call.
    }
    return c;
}

// Returns new caps after modification
unsigned int NetClient::Hmd_SetEnabledCaps(VirtualHmdId hmd, unsigned int hmdCaps)
{
    if (!IsConnected(false, false))
    {
        return 0;
    }

    OVR::Net::BitStream bsOut, returnData;
    bsOut.Write(hmd);

    uint32_t c = (uint32_t)hmdCaps;
    bsOut.Write(c);

    if (!GetRPC1()->CallBlocking("Hmd_SetEnabledCaps_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
    {
        return 0;
    }

    c = 0;
    if (!returnData.Read(c))
    {
        OVR_ASSERT(false); //This assert will hit if you tamper or restart the service mid-call.
    }
    return c;
}


//-------------------------------------------------------------------------------------
// *** Tracking Setup

bool NetClient::Hmd_ConfigureTracking(VirtualHmdId hmd, unsigned supportedCaps, unsigned requiredCaps)
{
    if (!IsConnected(false, false))
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
        return false;
    }

    uint8_t b;
    if (!returnData.Read(b))
    {
        OVR_ASSERT(false);
    }

    return b != 0;
}


void NetClient::Hmd_ResetTracking(VirtualHmdId hmd, bool visionReset)
{
    if (!IsConnected(false, false))
    {
        return;
    }

    OVR::Net::BitStream bsOut;
    bsOut.Write(hmd);

    int32_t w_visionReset;
    w_visionReset = visionReset ? 1 : 0;
    bsOut.Write(w_visionReset);

    if (!GetRPC1()->CallBlocking("Hmd_ResetTracking_1", &bsOut, GetSession()->GetConnectionAtIndex(0)))
    {
        return;
    }
}

bool NetClient::LatencyUtil_ProcessInputs(double startTestSeconds, unsigned char rgbColorOut[3])
{
    if (!IsConnected(false, false))
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
    if (!IsConnected(false, false))
    {
        return NULL;
    }

    OVR::Net::BitStream bsOut, returnData;
    if (!GetRPC1()->CallBlocking("LatencyUtil_GetResultsString_1", &bsOut, GetSession()->GetConnectionAtIndex(0), &returnData))
    {
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
    if (!IsConnected(false, false))
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
    observerScope.SetHandler(OVR::Net::Plugins::RPCSlot::FromMember<NetClient, &NetClient::functionName>(this)); \
    pRPC->RegisterSlot(OVR_STRINGIZE(functionName), &observerScope);

    // Register RPC functions:
    RPC_REGISTER_SLOT(InitialServerStateScope, InitialServerState_1);
    RPC_REGISTER_SLOT(LatencyTesterAvailableScope, LatencyTesterAvailable_1);
    RPC_REGISTER_SLOT(DefaultLogOutputScope, DefaultLogOutput_1);
    RPC_REGISTER_SLOT(HMDCountUpdateScope, HMDCountUpdate_1);
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

void NetClient::DefaultLogOutput_1(BitStream* userData, ReceivePayload* pPayload)
{
    OVR_UNUSED(pPayload);

    String formattedText;
    LogMessageType messageType = Log_Text; // Will normally be overwritten below.
    userData->Read(messageType);
    if (userData->Read(formattedText))
    {
        if (OVR::Log::GetGlobalLog())
        {
            OVR::String logStr = "[From Service] ";
            logStr.AppendString(formattedText);
            OVR::Log::GetGlobalLog()->LogMessage(messageType, "%s", logStr.ToCStr());
        }
    }
}

void NetClient::HMDCountUpdate_1(BitStream* userData, ReceivePayload* pPayload)
{
    OVR_UNUSED(pPayload);

    int32_t hmdCount = 0;
    if (!userData->Read(hmdCount))
    {
        OVR_ASSERT(false);
        return;
    }

    HMDCount = hmdCount;
    EdgeTriggeredHMDCount = true;
}


}} // namespace OVR::Service
