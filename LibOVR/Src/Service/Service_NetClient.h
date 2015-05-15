/************************************************************************************

Filename    :   Service_NetClient.h
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

#ifndef OVR_Service_NetClient_h
#define OVR_Service_NetClient_h

#include "Net/OVR_NetworkTypes.h"
#include "Service_NetSessionCommon.h"
#include "Kernel/OVR_System.h"
#include "OVR_CAPI.h"
#include "OVR_ErrorCode.h"
#include "OVR_Error.h"
#include "Util/Util_Render_Stereo.h"
#include "Service_Common_Compositor.h"

#if defined(OVR_OS_WIN32)
    #include "Service/Service_Win32_FastIPC_Client.h"
#endif // OVR_OS_WIN32

namespace OVR { namespace Service {

using namespace OVR::Net;


//-------------------------------------------------------------------------------------
// NetClient

class NetClient : public NetSessionCommon,
                  public Plugins::NetworkPlugin,
                  public SystemSingletonBase<NetClient>
{
    OVR_DECLARE_SINGLETON(NetClient);
    virtual void OnThreadDestroy();

public:
    ovrResult    Connect(bool blocking);
    bool         IsConnected(bool attemptReconnect, bool blockOnReconnect);
    void         Disconnect();

    void         GetLocalProtocolVersion(int& major, int& minor, int& patch);
    void         GetLocalSDKVersion(SDKVersion& requestedSDKVersion);

    // These functions may return false if it is not connected. However, they don't generate an Error in this case, 
    // as it's up to the caller to determine if a false return value indicates a true error in its context.
    bool         GetRemoteProtocolVersion(int& major, int& minor, int& patch);
    bool         GetRemoteSDKVersion(SDKVersion& remoteSDKVersion);

    pid_t        GetServerProcessId() { return ServerProcessId; }

    // Sets the last server error for this client. There isn't an hmd argument because the only time this
    // should be called is if we are unable to contact the server to get an hmd.
    void         SetLastError(String str, ovrResult errorCode = ovrError_ServiceError); // For backward compatibility. Users should migrate to using the version below via SetLastError(OVR_MAKE_ERROR(...))
    void         SetLastError(const OVRError& ovrError);                                // Alternative to the above which can support more error information.

    void         ApplyParameters(ovrInitParams const* params);

public:
    // Persistent key-value storage. These functions don't fail, but rather return a default valid value if not connected.
    // It's up to the user of these functions to determine if the system is in an error state.
    const char*  GetStringValue(VirtualHmdId hmd, const char* key, const char* default_val);    // Returns an empty string if not connected. default_val may be nullptr, to indicate that "" is the default value.
    bool         GetBoolValue(VirtualHmdId hmd, const char* key, bool default_val);
    int          GetIntValue(VirtualHmdId hmd, const char* key, int default_val);
    double       GetNumberValue(VirtualHmdId hmd, const char* key, double default_val);
    int          GetNumberValues(VirtualHmdId hmd, const char* key, double* values, int num_vals);

    // These functions can fail but don't report an Error if so, as they are utility functions and 
    // are not part of the intrinsic flow logic of this class.
    bool         SetStringValue(VirtualHmdId hmd, const char* key, const char* val);
    bool         SetBoolValue(VirtualHmdId hmd, const char* key, bool val);
    bool         SetIntValue(VirtualHmdId hmd, const char* key, int val);
    bool         SetNumberValue(VirtualHmdId hmd, const char* key, double val);
    bool         SetNumberValues(VirtualHmdId hmd, const char* key, const double* vals, int num_vals);

    bool         GetDriverMode(bool& driverInstalled, bool& compatMode, bool& hideDK1Mode);
    bool         SetDriverMode(bool compatMode, bool hideDK1Mode);

    // Returns the count of HMDs present, or an error code if it is unknown due to a connection failure 
    // with the Service. 
	ovrResult    Hmd_Detect();

	bool         Hmd_Create(int index, HMDNetworkInfo* netInfo, OVRError* pOVRError);
	void         Hmd_Release(VirtualHmdId hmd);

    // Support debug rendering connection to the service
    bool         Hmd_CreateDebug(HmdTypeEnum hmdType, OVRError* pOVRError);
    
    // Gets the last server error for the the given hmd.
	void         Hmd_GetLastError(VirtualHmdId hmd, OVRError& ovrError);    // For 0.6+ clients.

	// TBD: Replace with a function to return internal, original HMDInfo?

	// Fills in description about HMD; this is the same as filled in by ovrHmd_Create.
	// The actual descriptor is a par
    bool         Hmd_GetHmdInfo(VirtualHmdId hmd, HMDInfo& hmdInfo);

	//-------------------------------------------------------------------------------------
	unsigned int Hmd_GetEnabledCaps(VirtualHmdId hmd);
	// Returns new caps after modification
	unsigned int Hmd_SetEnabledCaps(VirtualHmdId hmd, unsigned int hmdCaps);

	//-------------------------------------------------------------------------------------
	// *** Tracking Setup

	ovrResult    Hmd_ConfigureTracking(VirtualHmdId hmd, unsigned supportedCaps, unsigned requiredCaps);	
	void         Hmd_ResetTracking(VirtualHmdId hmd, bool backOfHeadOnly);

	// TBD: Camera frames
    bool         LatencyUtil_ProcessInputs(double startTestSeconds, unsigned char rgbColorOut[3]);
    const char*  LatencyUtil_GetResultsString();

    //-------------------------------------------------------------------------------------
    // Compositor
    OVRError     Compositor_Create_1(RPCCompositorClientCreateParams& request, RPCCompositorClientCreateResult& response);
    OVRError     Compositor_Destroy_1(RPCCompositorClientDestroyParams& request, RPCCompositorClientDestroyResult& response);

    OVRError     Compositor_TextureSet_Create_1(RPCCompositorTextureSetCreateParams& request, RPCCompositorTextureSetCreateResult& response);
    OVRError     Compositor_TextureSet_Destroy_1(RPCCompositorTextureSetDestroyParams& request, RPCCompositorTextureSetDestroyResult& response);

    OVRError     Compositor_CreateMirror_1(RPCCompositorClientCreateMirrorParams& request, RPCCompositorClientCreateMirrorResult& response);
    OVRError     Compositor_DestroyMirror_1(RPCCompositorClientDestroyMirrorParams& request, RPCCompositorClientDestroyMirrorResult& response);

    OVRError     Compositor_SubmitLayers_1(IPCCompositorSubmitLayersParams& request, IPCCompositorSubmitLayersResult& response);
    OVRError     Compositor_EndFrame_1(IPCCompositorEndFrameParams& request, IPCCompositorEndFrameResult& response);

    // Boilerplate code for compositor call wrappers.
    template<typename pType, typename rType>
    OVRError     CompositorCallWrapper(const char* functionName, pType& request, rType& response);

    bool         ShutdownServer();

#if defined(OVR_OS_WIN32)
    Service::Win32::FastIPCClient IPCClient;
#endif // OVR_OS_WIN32

protected:
    // Status
    bool         LatencyTesterAvailable;
    int          HMDCount;
    bool         EdgeTriggeredHMDCount;
    pid_t        ServerProcessId;

    OVRError     LastOVRError;
    String       LastOVRErrorString; // For temporary backward compatibility.
    String       LatencyUtil_GetResultsString_Str;
    String       ProfileGetValue1_Str, ProfileGetValue3_Str;

    // Parameters passed to ovr_Initialize()
    bool         ServerOptional;      // Server connection is optional?
    bool         ExtraDebugging;      // Extra debugging enabled?
    int          ConnectionTimeoutMS; // Connection timeout in milliseconds

    void         SetDefaultParameters();

protected:
    virtual void OnDisconnected(Connection* conn) OVR_OVERRIDE;
    virtual void OnConnected(Connection* conn) OVR_OVERRIDE;

    virtual int  Run();

    //// Push Notifications:

    void registerRPC();

    CallbackListener<Plugins::RPCSlot> InitialServerStateScope;
    void InitialServerState_1(BitStream& userData, ReceivePayload const& pPayload);

    CallbackListener<Plugins::RPCSlot> LatencyTesterAvailableScope;
    void LatencyTesterAvailable_1(BitStream& userData, ReceivePayload const& pPayload);

    CallbackListener<Plugins::RPCSlot> DefaultLogOutputScope;
    void DefaultLogOutput_1(BitStream& userData, ReceivePayload const& pPayload);

    CallbackListener<Plugins::RPCSlot> HMDCountUpdateScope;
    void HMDCountUpdate_1(BitStream& userData, ReceivePayload const& pPayload);
};


}} // namespace OVR::Service

#endif // OVR_Service_NetClient_h
