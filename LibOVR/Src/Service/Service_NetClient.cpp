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
#include "OVR_Error.h"

OVR_DEFINE_SINGLETON(OVR::Service::NetClient);

namespace OVR { namespace Service {

using namespace OVR::Net;


// Default connection timeout in milliseconds.
static const int kDefaultConnectionTimeoutMS = 5000; // Timeout in Milliseconds


//// NetClient

NetClient::NetClient() :
    LatencyTesterAvailable(false),
    HMDCount(-1),
    EdgeTriggeredHMDCount(false),
    ServerProcessId(OVR_INVALID_PID),
    LastOVRError(),
    LastOVRErrorString(),
    LatencyUtil_GetResultsString_Str(),
    ProfileGetValue1_Str(),
    ProfileGetValue3_Str(),
    ServerOptional(false),
    ExtraDebugging(false),
    ConnectionTimeoutMS(0)
{
    SetDefaultParameters();

    GetSession()->AddSessionListener(this);

    // Register RPC functions
    registerRPC();

    if(!Start()) // Start the socket communication thread.
    {
        // To do: This is a critical failure. We need to find a way to relay it to the user of this class.
        OVR_ASSERT_M(false, "NetClient: Unable to start socket thread.");
        LogError("[NetClient] Unable to start socket thread.");
    }

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
        // To do: Find a way to avoid sleep-polling here, as this is executed within client application process.

        GetSession()->Poll(false);

        if (GetSession()->GetActiveSocketsCount() == 0)
        {
            Thread::MSleep(100);
        }
    }

    return 0;
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

ovrResult NetClient::Connect(bool blocking)
{
    // If server is optional,
    if (ServerOptional && !Session::IsSingleProcess())
    {
        blocking = false; // Poll: Do not block
    }

    // Set up bind parameters
    BerkleyBindParameters bbp;
    bbp.Address = "::1"; // Bind to localhost only!
    bbp.blockingTimeout = ConnectionTimeoutMS;
    SockAddr sa;
    sa.Set("::1", VRServicePort, SOCK_STREAM);

    // Attempt to connect
    SessionResult sessionResult = GetSession()->ConnectPTCP(&bbp, &sa, blocking);

    if (Session::GetSessionResultSuccess(sessionResult))
    {
        OVR_ASSERT(GetSession()->GetError().GetCode() == ovrSuccess);
        return ovrSuccess;
    }

    ovrResult errorCode = GetSession()->GetError().GetCode();

    if(errorCode == ovrSuccess) // If the session didn't report an error itself... then either its handling is incomplete or the error was at our level instead of the session.
    {
        // We don't currently call OVR_MAKE_ERROR because that logs the error and currently Hmd_Create is 
        // often called by the CAPI expecting failure. We may want to have a parameter to this function which
        // indicates that we should or shouldn't log the error and take any other drastic actions. 
        SetLastError(OVRError(ovrError_ServiceConnection, "Unable to connect to the OVR Server. SessionResult = %d", (int)sessionResult));

        errorCode = ovrError_ServiceConnection;
    }

    return errorCode;
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

// Returns bool but doesn't generate an Error if false.
bool NetClient::GetRemoteProtocolVersion(int& major, int& minor, int& patch)
{
    Ptr<Connection> conn = GetSession()->GetFirstConnection();

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

// Returns bool but doesn't generate an Error if false.
bool NetClient::GetRemoteSDKVersion(SDKVersion& remoteSDKVersion)
{
    Ptr<Connection> conn = GetSession()->GetFirstConnection();

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

    BitStream bsOut, returnData;
    bsOut.Write(hmd);
    bsOut.Write(key);
    bsOut.Write(default_val);

    OVRError err = GetRPC1()->CallBlocking("GetStringValue_1", bsOut, GetSession()->GetFirstConnection(), &returnData);
    if (!err.Succeeded())
    {
        SetLastError(err);
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

    BitStream bsOut, returnData;
    bsOut.Write(hmd);
    bsOut.Write(key);
    bsOut.Write(default_val);

    OVRError err = GetRPC1()->CallBlocking("GetBoolValue_1", bsOut, GetSession()->GetFirstConnection(), &returnData);
    if (!err.Succeeded())
    {
        SetLastError(err);
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

    BitStream bsOut, returnData;
    bsOut.Write(hmd);
    bsOut.Write(key);
    bsOut.Write(default_val);

    OVRError err = GetRPC1()->CallBlocking("GetIntValue_1", bsOut, GetSession()->GetFirstConnection(), &returnData);
    if (!err.Succeeded())
    {
        SetLastError(err);
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

    BitStream bsOut, returnData;
    bsOut.Write(hmd);
    bsOut.Write(key);
    bsOut.Write(default_val);

    OVRError err = GetRPC1()->CallBlocking("GetNumberValue_1", bsOut, GetSession()->GetFirstConnection(), &returnData);
    if (!err.Succeeded())
    {
        SetLastError(err);
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

    BitStream bsOut, returnData;
    bsOut.Write(hmd);
    bsOut.Write(key);

    int32_t w = (int32_t)num_vals;
    bsOut.Write(w);

    OVRError err = GetRPC1()->CallBlocking("GetNumberValues_1", bsOut, GetSession()->GetFirstConnection(), &returnData);
    if (!err.Succeeded())
    {
        SetLastError(err);
        return 0;
    }

    int32_t out = 0;
    if (!returnData.Read(out))
    {
        OVR_FAIL_M("NetClient::GetNumberValues: returnData read failure.");
    }

    OVR_ASSERT_F((out >= 0) && (out <= num_vals), "NetClient::GetNumberValues failure: out=%d, num_vals=%d", out, num_vals);

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

    BitStream bsOut;
    bsOut.Write(hmd);
    bsOut.Write(key);

    bsOut.Write(val);

    if (!GetRPC1()->Signal("SetStringValue_1", bsOut, GetSession()->GetFirstConnection()))
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

    BitStream bsOut;
    bsOut.Write(hmd);
    bsOut.Write(key);

    uint8_t b = val ? 1 : 0;
    bsOut.Write(b);

    if (!GetRPC1()->Signal("SetBoolValue_1", bsOut, GetSession()->GetFirstConnection()))
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

    BitStream bsOut;
    bsOut.Write(hmd);
    bsOut.Write(key);

    int32_t w = (int32_t)val;
    bsOut.Write(w);

    if (!GetRPC1()->Signal("SetIntValue_1", bsOut, GetSession()->GetFirstConnection()))
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

    BitStream bsOut;
    bsOut.Write(hmd);
    bsOut.Write(key);

    bsOut.Write(val);

    if (!GetRPC1()->Signal("SetNumberValue_1", bsOut, GetSession()->GetFirstConnection()))
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

    BitStream bsOut;
    bsOut.Write(hmd);
    bsOut.Write(key);

    int32_t w_count = (int32_t)num_vals;
    bsOut.Write(w_count);

    for (int i = 0; i < num_vals; i++)
    {
        bsOut.Write(vals[i]);
    }

    if (!GetRPC1()->Signal("SetNumberValues_1", bsOut, GetSession()->GetFirstConnection()))
    {
        return false;
    }

    return true;
}

ovrResult NetClient::Hmd_Detect()
{
    if (!IsConnected(true, false))
    {
        // Question: Should we generate an OVRError here? If we do then we may want to consider all the 
        // parallel cases to this IsConnected checking in other functions.
        return ovrError_ServiceConnection;
    }

    // If using edge-triggered HMD counting,
    if (EdgeTriggeredHMDCount)
    {
        // Return the last update from the server
        return HMDCount;
    }

    // Otherwise: We need to ask the first time

    BitStream bsOut, returnData;

    OVRError err = GetRPC1()->CallBlocking("Hmd_Detect_1", bsOut, GetSession()->GetFirstConnection(), &returnData);
    if (!err.Succeeded())
    {
        SetLastError(err);
        return err.GetCode();
    }

    int32_t out = 0;
    if (!returnData.Read(out))
    {
        SetLastError(OVR_MAKE_ERROR(ovrError_ServiceError, "Hmd_Detect return data read error."));
        return ovrError_ServiceError;
    }

    OVR_ASSERT(out >= 0);  // We expect that any errors (negative values) will already have been returned by CallBlocking.
    HMDCount = out;
    EdgeTriggeredHMDCount = true;

    return HMDCount;
}

bool NetClient::Hmd_Create(int index, HMDNetworkInfo* netInfo, OVRError* pOVRError)
{
    OVR_ASSERT(netInfo && pOVRError);

    if (netInfo)
    {
        netInfo->NetId = InvalidVirtualHmdId; // Invalid until made valid below.
    }

    if (!IsConnected(true, true))
    {
        // We don't currently call OVR_MAKE_ERROR because that logs the error and currently Hmd_Create is 
        // often called by the CAPI expecting failure. We may want to have a parameter to this function which
        // indicates that we should or shouldn't log the error and take any other drastic actions. 
        SetLastError(OVRError(ovrError_ServiceConnection, "NetClient Hmd_Create service not connected."));
        if (pOVRError)
        {
            *pOVRError = LastOVRError;
        }
        return false;
    }

    BitStream bsOut, returnData;

    int32_t w = (int32_t)index;
    bsOut.Write(w);

    // Need the Pid for driver mode
    pid_t pid = GetCurrentProcessId();
    bsOut.Write(pid);

    OVRError err = GetRPC1()->CallBlocking("Hmd_Create_1", bsOut, GetSession()->GetFirstConnection(), &returnData);
    if (!err.Succeeded())
    {
        // See above comments on why we don't call OVR_MAKE_ERROR. 
        SetLastError(err);
        if (pOVRError)
        {
            *pOVRError = LastOVRError;
        }
        return false;
    }

    if (netInfo && netInfo->Serialize(returnData, false))
    {
        return (netInfo->NetId != InvalidVirtualHmdId);
    }

    // See above comments on why we don't call OVR_MAKE_ERROR. 
    SetLastError(OVRError(ovrError_ServiceError, "NetClient Hmd_Create service call failed."));
    if (pOVRError)
    {
        *pOVRError = LastOVRError;
    }

    return false;
}

bool NetClient::Hmd_CreateDebug(HmdTypeEnum hmdType, OVRError* pOVRError)
{
    OVR_ASSERT(pOVRError);

    if (!IsConnected(true, true))
    {
        // We don't currently call OVR_MAKE_ERROR because that logs the error and currently Hmd_Create is 
        // often called by the CAPI expecting failure. We may want to have a parameter to this function which
        // indicates that we should or shouldn't log the error and take any other drastic actions. 
        SetLastError(OVRError(ovrError_ServiceConnection, "NetClient Hmd_CreateDebug service not connected."));
        if (pOVRError)
        {
            *pOVRError = LastOVRError;
        }
        return false;
    }

    BitStream bsOut, returnData;

    int32_t w = (int32_t)hmdType;
    bsOut.Write(w);

    // Need the Pid for driver mode
    pid_t pid = GetCurrentProcessId();
    bsOut.Write(pid);

    OVRError err = GetRPC1()->CallBlocking("Hmd_CreateDebug_1", bsOut, GetSession()->GetFirstConnection(), &returnData);
    if (!err.Succeeded())
    {
        // See above comments on why we don't call OVR_MAKE_ERROR. 
        SetLastError(err);
        if (pOVRError)
        {
            *pOVRError = LastOVRError;
        }
        return false;
    }

    return true;
}

bool NetClient::GetDriverMode(bool& driverInstalled, bool& compatMode, bool& hideDK1Mode)
{
    if (!IsConnected(true, true))
    {
        return false;
    }

    BitStream bsOut, returnData;

    bsOut.Write(InvalidVirtualHmdId);

    OVRError err = GetRPC1()->CallBlocking("GetDriverMode_1", bsOut, GetSession()->GetFirstConnection(), &returnData);
    if (!err.Succeeded())
    {
        SetLastError(err);
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

    BitStream bsOut, returnData;

    bsOut.Write(InvalidVirtualHmdId);

    int32_t w_compatMode, w_hideDK1Mode;
    w_compatMode = compatMode ? 1 : 0;
    w_hideDK1Mode = hideDK1Mode ? 1 : 0;
    bsOut.Write(w_compatMode);
    bsOut.Write(w_hideDK1Mode);

    OVRError err = GetRPC1()->CallBlocking("SetDriverMode_1", bsOut, GetSession()->GetFirstConnection(), &returnData);
    if (!err.Succeeded())
    {
        SetLastError(err);
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

void NetClient::Hmd_Release(VirtualHmdId hmd)
{
    if (!IsConnected(false, false))
    {
        return;
    }

    BitStream bsOut;
    bsOut.Write(hmd);

    GetRPC1()->CallBlocking("Hmd_Release_1", bsOut, GetSession()->GetFirstConnection());
}

void NetClient::SetLastError(String str, ovrResult errorCode)
{
    // We don't call OVR_MAKE_ERROR because that does a debug trace and sets the current thread's last error. The current 
    // usage of this function makes that not ideal, but rather we'll just store it as a member.
    LastOVRError = OVRError(errorCode, "%s", str.ToCStr());
}

void NetClient::SetLastError(const OVRError& ovrError)
{
    LastOVRError = ovrError;

    // Reflect the service's last error to our process' last error. We don't currently log the error, as it turns out that 
    // the ovrHmd_Create function (and potentially others) calls this NetClient repeatedly after receiving an error from 
    // it, as a way of waiting until some future success. Well if we logged errors here then the log output would get 
    // spammed with many error traces. Perhaps we can have a way for the CAPI to make calls to this NetClient with a parameter
    // that indicates that an error is "expected" and so not to log the error or do anything else drastic if failure occurs
    LastErrorTLS::GetInstance()->LastError() = ovrError;
}

void NetClient::Hmd_GetLastError(VirtualHmdId hmd, OVRError& ovrError)
{
    // If we aren't connected to the server then we return whatever LastOVRError is.
    // If we fail to be able to complete the RPC to the server then we create a new error state for ourselves and return that.
    // Otherwise we complete the RPC and set LastOVRError as returned from the server.

    if ((hmd != InvalidVirtualHmdId) && IsConnected(false, false))
    {
        BitStream bsOut, returnData;
        bsOut.Write(hmd);

        OVRError err = GetRPC1()->CallBlocking("Hmd_GetLastError_2", bsOut, GetSession()->GetFirstConnection(), &returnData);
        if (err.Succeeded())
        {
            if (!NetSessionCommon::SerializeOVRError(returnData, LastOVRError, false))
            {
                LastOVRError = OVR_MAKE_ERROR_F(ovrError_ServiceError, "Unable to RPC deserialize Hmd_GetLastError_2 for virtual Hmd %d", (int)hmd);
            } // Else LastOVRError was successfully set by the SerializeOVRError call.
        }
        else
        {
            LastOVRError = OVR_MAKE_ERROR_F(ovrError_ServiceError, "Unable to RPC call Hmd_GetLastError_2 for virtual Hmd %d", (int)hmd);
        }
    }

    ovrError = LastOVRError;
}


// Fills in description about HMD; this is the same as filled in by ovrHmd_Create.
// The actual descriptor is a par
bool NetClient::Hmd_GetHmdInfo(VirtualHmdId hmd, HMDInfo& hmdInfo)
{
    if (!IsConnected(false, false))
    {
        return false;
    }

    BitStream bsOut, returnData;
    bsOut.Write(hmd);

    OVRError err = GetRPC1()->CallBlocking("Hmd_GetHmdInfo_1", bsOut, GetSession()->GetFirstConnection(), &returnData);
    if (!err.Succeeded())
    {
        SetLastError(err);
        return false;
    }

    return NetSessionCommon::SerializeHMDInfo(returnData, hmdInfo, false);
}


//-------------------------------------------------------------------------------------
unsigned int NetClient::Hmd_GetEnabledCaps(VirtualHmdId hmd)
{
    if (!IsConnected(false, false))
    {
        return 0;
    }

    BitStream bsOut, returnData;
    bsOut.Write(hmd);

    OVRError err = GetRPC1()->CallBlocking("Hmd_GetEnabledCaps_1", bsOut, GetSession()->GetFirstConnection(), &returnData);
    if (!err.Succeeded())
    {
        SetLastError(err);
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

    BitStream bsOut, returnData;
    bsOut.Write(hmd);

    uint32_t c = (uint32_t)hmdCaps;
    bsOut.Write(c);

    OVRError err = GetRPC1()->CallBlocking("Hmd_SetEnabledCaps_1", bsOut, GetSession()->GetFirstConnection(), &returnData);
    if (!err.Succeeded())
    {
        SetLastError(err);
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

ovrResult NetClient::Hmd_ConfigureTracking(VirtualHmdId hmd, unsigned supportedCaps, unsigned requiredCaps)
{
    if (!IsConnected(false, false))
    {
        return ovrError_ServiceConnection;
    }

    // Make sure it's not a debug HMD
    if (hmd < 0)
    {
        return ovrError_InvalidHmd;
    }

    BitStream bsOut, returnData;
    bsOut.Write(hmd);

    uint32_t w_sc = supportedCaps;
    bsOut.Write(w_sc);
    uint32_t w_rc = requiredCaps;
    bsOut.Write(w_rc);

    OVRError err = GetRPC1()->CallBlocking("Hmd_ConfigureTracking_1", bsOut, GetSession()->GetFirstConnection(), &returnData);
    if (!err.Succeeded())
    {
        SetLastError(err);
        return err.GetCode();
    }

    return ovrSuccess;
}


void NetClient::Hmd_ResetTracking(VirtualHmdId hmd, bool backOfHeadOnly)
{
    if (!IsConnected(false, false))
    {
        return;
    }

    BitStream bsOut;
    bsOut.Write(hmd);

    bsOut.Write((int32_t)backOfHeadOnly);

    OVRError err = GetRPC1()->CallBlocking("Hmd_ResetTracking_1", bsOut, GetSession()->GetFirstConnection());
    if (!err.Succeeded())
    {
        SetLastError(err);
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

    BitStream bsOut, returnData;
    bsOut.Write(startTestSeconds);

    OVRError err = GetRPC1()->CallBlocking("LatencyUtil_ProcessInputs_1", bsOut, GetSession()->GetFirstConnection(), &returnData);
    if (!err.Succeeded())
    {
        SetLastError(err);
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
        return nullptr;
    }

    BitStream bsOut, returnData;

    OVRError err = GetRPC1()->CallBlocking("LatencyUtil_GetResultsString_1", bsOut, GetSession()->GetFirstConnection(), &returnData);
    if (!err.Succeeded())
    {
        SetLastError(err);
        return nullptr;
    }

    if (!returnData.Read(LatencyUtil_GetResultsString_Str))
    {
        OVR_ASSERT(false);
    }

    return LatencyUtil_GetResultsString_Str.ToCStr();
}

// Boilerplate code for compositor call wrappers.
template<typename pType, typename rType>
OVRError NetClient::CompositorCallWrapper(const char* functionName, pType& request, rType& response)
{
    if (!IsConnected(true, false))
    {
        return OVR_MAKE_ERROR(ovrError_ServiceError, "Disconnected");
    }

    OVR::Net::BitStream bsOut, returnData;
    request.Serialize(true, bsOut);

    OVRError err = GetRPC1()->CallBlocking(functionName, bsOut, GetSession()->GetFirstConnection(), &returnData);
    if (!err.Succeeded())
    {
        SetLastError(err);
        return err;
    }

    if (!response.Serialize(false, returnData))
    {
        return OVR_MAKE_ERROR(ovrError_ServiceError, "Truncated");
    }

    return err;
}

OVRError NetClient::Compositor_Create_1(RPCCompositorClientCreateParams& request, RPCCompositorClientCreateResult& response)
{
    return CompositorCallWrapper("Compositor_Create_1", request, response);
}

OVRError NetClient::Compositor_Destroy_1(RPCCompositorClientDestroyParams& request, RPCCompositorClientDestroyResult& response)
{
    return CompositorCallWrapper("Compositor_Destroy_1", request, response);
}

OVRError NetClient::Compositor_TextureSet_Create_1(RPCCompositorTextureSetCreateParams& request, RPCCompositorTextureSetCreateResult& response)
{
    return CompositorCallWrapper("Compositor_TextureSet_Create_1", request, response);
}

OVRError NetClient::Compositor_TextureSet_Destroy_1(RPCCompositorTextureSetDestroyParams& request, RPCCompositorTextureSetDestroyResult& response)
{
    return CompositorCallWrapper("Compositor_TextureSet_Destroy_1", request, response);
}

OVRError NetClient::Compositor_CreateMirror_1(RPCCompositorClientCreateMirrorParams& request, RPCCompositorClientCreateMirrorResult& response)
{
    return CompositorCallWrapper("Compositor_CreateMirror_1", request, response);
}

OVRError NetClient::Compositor_DestroyMirror_1(RPCCompositorClientDestroyMirrorParams& request, RPCCompositorClientDestroyMirrorResult& response)
{
    return CompositorCallWrapper("Compositor_DestroyMirror_1", request, response);
}

OVRError NetClient::Compositor_SubmitLayers_1(IPCCompositorSubmitLayersParams& request, IPCCompositorSubmitLayersResult& response)
{
#if defined(OVR_OS_WIN32)
    if (IPCClient.IsValid())
    {
        BitStream bsRequest, bsResponse;
        bsRequest.Write("Compositor_SubmitLayers_1");
        request.Serialize(true, bsRequest);

        OVRError err = IPCClient.Call(bsRequest, bsResponse);
        if (!err.Succeeded())
        {
            return err;
        }

        if (!response.Serialize(false, bsResponse))
        {
            return OVR_MAKE_ERROR(ovrError_ServiceError, "Corrupt response");
        }

        return OVRError::Success();
    }
#endif // OVR_OS_WIN32

    return CompositorCallWrapper("Compositor_SubmitLayers_1", request, response);
}

OVRError NetClient::Compositor_EndFrame_1(IPCCompositorEndFrameParams& request, IPCCompositorEndFrameResult& response)
{
#if defined(OVR_OS_WIN32)
    if (IPCClient.IsValid())
    {
        BitStream bsRequest, bsResponse;
        bsRequest.Write("Compositor_EndFrame_1");
        request.Serialize(true, bsRequest);

        OVRError err = IPCClient.Call(bsRequest, bsResponse);
        if (!err.Succeeded())
        {
            return err;
        }

        if (!response.Serialize(false, bsResponse))
        {
            return OVR_MAKE_ERROR(ovrError_ServiceError, "Corrupt response");
        }

        return OVRError::Success();
    }
#endif // OVR_OS_WIN32

    return CompositorCallWrapper("Compositor_EndFrame_1", request, response);
}

bool NetClient::ShutdownServer()
{
    if (!IsConnected(false, false))
    {
        return false;
    }

    BitStream bsOut;
    GetRPC1()->BroadcastSignal("Shutdown_1", bsOut);

    return true;
}


//// Push Notifications:

void NetClient::registerRPC()
{
#define RPC_REGISTER_SLOT(observerScope, functionName) \
    observerScope.SetHandler(Plugins::RPCSlot::FromMember<NetClient, &NetClient::functionName>(this)); \
    pRPC->RegisterSlot(OVR_STRINGIZE(functionName), &observerScope);

    // Register RPC functions:
    RPC_REGISTER_SLOT(InitialServerStateScope, InitialServerState_1);
    RPC_REGISTER_SLOT(LatencyTesterAvailableScope, LatencyTesterAvailable_1);
    RPC_REGISTER_SLOT(DefaultLogOutputScope, DefaultLogOutput_1);
    RPC_REGISTER_SLOT(HMDCountUpdateScope, HMDCountUpdate_1);
}

void NetClient::InitialServerState_1(BitStream& userData, ReceivePayload const& pPayload)
{
    LatencyTesterAvailable_1(userData, pPayload);

    if (!userData.Read(ServerProcessId))
    {
        OVR_ASSERT(false); // Server is out of date.
        return;
    }
}

void NetClient::LatencyTesterAvailable_1(BitStream& userData, ReceivePayload const& pPayload)
{
    OVR_UNUSED(pPayload);

    uint8_t b = 0;
    if (!userData.Read(b))
    {
        OVR_ASSERT(false);
        return;
    }

    LatencyTesterAvailable = (b != 0);
}

void NetClient::DefaultLogOutput_1(BitStream& userData, ReceivePayload const& pPayload)
{
    OVR_UNUSED(pPayload);

    String formattedText;
    LogMessageType messageType = Log_Text; // Will normally be overwritten below.
    userData.Read(messageType);
    if (userData.Read(formattedText))
    {
        if (Log::GetGlobalLog() /*&& (messageType != Log_Text) */)
        {
            String logStr = "[From Service] ";
            logStr.AppendString(formattedText);
            Log::GetGlobalLog()->LogMessage(messageType, "%s", logStr.ToCStr());
        }
    }
}

void NetClient::HMDCountUpdate_1(BitStream& userData, ReceivePayload const& pPayload)
{
    OVR_UNUSED(pPayload);

    int32_t hmdCount = 0;
    if (!userData.Read(hmdCount))
    {
        OVR_ASSERT(false);
        return;
    }

    HMDCount = hmdCount;
    EdgeTriggeredHMDCount = true;
}


}} // namespace OVR::Service
