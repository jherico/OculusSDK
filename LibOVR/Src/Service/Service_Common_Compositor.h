/************************************************************************************

Filename    :   Service_Common_Compositor.h
Content     :   Common code shared between compositor client/server
Created     :   Sept 25, 2014
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

#ifndef OVR_Service_Common_Compositor_h
#define OVR_Service_Common_Compositor_h

#include "Net/OVR_BitStream.h"
#include "OVR_CAPI.h"
#include "OVR_Stereo.h"
#include "Service_NetSessionCommon.h"

#if defined(OVR_OS_WIN32) // Desktop Windows only.
    #include "Util/Util_Direct3D.h"
    #include "Service_Win32_FastIPC_Client.h"
    typedef LUID GraphicsAdapterID;
#elif defined(OVR_OS_MAC)
    typedef uint32_t GraphicsAdapterID;
#elif defined(OVR_OS_LINUX)
    //#include <GL/gl.h>
    typedef uint32_t GraphicsAdapterID;
#endif

typedef uint32_t handle32_t;
typedef uint64_t handle64_t;

namespace OVR { namespace Service {


// Note that since we're using a special IPC path for low latency this
// needs to be versioned separately from socket RPC.
#define OVR_IPC_PROTOCOL_VERSION 0
#define OVR_IPC_TIMEOUT 1000 /* msec for client to wait for IPC server */


//-------------------------------------------------------------------------------------
// ***** OutputLatencyTimings

// Latency timings returned to the application.

struct OutputLatencyTimings
{
    double LatencyRender;       // (seconds) Last time between render IMU sample and scanout
    double LatencyTimewarp;     // (seconds) Last time between timewarp IMU sample and scanout
    double LatencyPostPresent;  // (seconds) Average time between Vsync and scanout
    double ErrorRender;         // (seconds) Last error in render predicted scanout time
    double ErrorTimewarp;       // (seconds) Last error in timewarp predicted scanout time

    void Clear()
    {
        LatencyRender      = 0.;
        LatencyTimewarp    = 0.;
        LatencyPostPresent = 0.;
        ErrorRender        = 0.;
        ErrorTimewarp      = 0.;
    }

    bool Serialize(bool write, Net::BitStream& bs);
};


//-----------------------------------------------------------------------------
// CompositorClientCreate_1

// This is the rift identifying information needed by the core compositor code
// to create a client connection.
struct RPCCompositorClientRiftInfo
{
    // HMD UUID to uniquely identify the headset, in display EDID
    String DisplayUUID;

    // Render information pulled from the user profile needed for distortion
    ProfileRenderInfo OurProfileRenderInfo;
};

struct RPCCompositorClientCreateParams
{
    // Client's processId
    pid_t ProcessId;

    // Virtual HMD corresponding to this request.
    VirtualHmdId HMD;

    // Rift info
    RPCCompositorClientRiftInfo RiftInfo;

    // LUID of adapter that application is using.
    GraphicsAdapterID AdapterID;

    // Synchronization primitives. Fence may be null if using
    // CPU spinwait on legacy clients.
    handle64_t FenceHandle;
    handle64_t FrameQueueSemaphoreHandle;

    bool Serialize(bool write, Net::BitStream& bs);
};

struct RPCCompositorClientCreateResult
{
#if defined(OVR_OS_WIN32)
    // Key to allow client access to IPC.
    Win32::FastIPCKey IPCKey;
#endif // OVR_OS_WIN32

    // Name of app timing shared memory region for the timing subsystem.
    String AppTimingName;

    int MaxNumLayers;

    bool Serialize(bool write, Net::BitStream& bs);
};


//-----------------------------------------------------------------------------
// CompositorTextureSetCreate_1

struct RPCCompositorTextureSetCreateParams
{
    // Virtual HMD corresponding to this request.
    VirtualHmdId HMD;

    // OS share handles so textures can be shared with Compositor
    Array<handle64_t> TextureShareHandles;

    bool Serialize(bool write, Net::BitStream& bs);
};

struct RPCCompositorTextureSetCreateResult
{
    // Unique ID assigned to this texture set by the compositor.
    // Used to reference this texture set in submit calls.
    uint32_t TextureSetID;

    bool Serialize(bool write, Net::BitStream& bs);
};


//-----------------------------------------------------------------------------
// CompositorTextureSetDestroy_1

struct RPCCompositorTextureSetDestroyParams
{
    // Virtual HMD corresponding to this request.
    VirtualHmdId HMD;

    // ID of the texture set to destroy
    uint32_t TextureSetID;

    bool Serialize(bool write, Net::BitStream& bs);
};

struct RPCCompositorTextureSetDestroyResult
{
    // Result code from creating texture set.  NoError for success.
    uint32_t ResultCode;
    uint32_t DetailErrorCode; // Detailed error code when ResultCode is not NoError.

    bool Serialize(bool write, Net::BitStream& bs);
};


//-----------------------------------------------------------------------------
// CompositorSubmitLayers_1 : Sent over FastIPC or socket RPC

// Parameters describing a single layer in this submission
// If you change this, update IPCCompositorSubmitLayersParams::Serialize()
struct CompositorLayerDesc
{
    int LayerNum;

    // All the non-pointer data.
    LayerDesc Desc;

    // ID of the texture set to select the texture(s) from.
    // May be the same texture for both left and right.
    uint32_t    TextureSetIDColor[2];
    uint32_t    TextureSetIDDepth[2];

    // The index of the specific buffer in the texture set to use.
    // May be the same index for both left and right.
    uint32_t    TextureIndexColor[2];
    uint32_t    TextureIndexDepth[2];
};

struct IPCCompositorSubmitLayersParams
{
    // Virtual HMD corresponding to this request.
    VirtualHmdId HMD;

    // Layers to update.
    ArrayPOD<CompositorLayerDesc> Layers;

    bool Serialize(bool write, Net::BitStream& bs);
};

struct IPCCompositorSubmitLayersResult
{
    bool Serialize(bool write, Net::BitStream& bs);
};


//-----------------------------------------------------------------------------
// CompositorEndFrame_1 : Sent over FastIPC or socket RPC

struct CompositorEndFrameAppTiming
{
    // App frame index
    uint32_t AppFrameIndex;

    // App render pose IMU time
    double AppRenderIMUTime;

    // App predicted scanout start time
    double AppScanoutStartTime;

    bool Serialize(bool write, Net::BitStream& bs);
};

struct IPCCompositorEndFrameParams
{
    // Virtual HMD corresponding to this request.
    VirtualHmdId HMD;

    // Provide the centered from world transform matrix used to get the eye
    // poses, so that it can be used during timewarp extrapolation.
    Posed CenteredFromWorld;

    // Data for positional timewarp to use.
    ovrViewScaleDesc ViewScaleDesc;

    // DistortionCaps
    uint32_t EnabledDistortionCaps;

    // Distortion parameters
    float DistortionClearColor[4];

    // App frame timing
    CompositorEndFrameAppTiming AppTiming;

    bool Serialize(bool write, Net::BitStream& bs);
};

struct IPCCompositorEndFrameResult
{
    OutputLatencyTimings LatencyTimings;

    bool Serialize(bool write, Net::BitStream& bs);
};


//-----------------------------------------------------------------------------
// CompositorClientCreateMirror_1

// This is a request to create a shared mirror texture.
struct RPCCompositorClientCreateMirrorParams
{
    // Virtual HMD corresponding to this request.
    VirtualHmdId HMD;

    // Shared texture handle to the surface to copy mirror output into
    handle64_t TextureHandle;

    bool Serialize(bool write, Net::BitStream& bs);
};

struct RPCCompositorClientCreateMirrorResult
{
    bool Serialize(bool write, Net::BitStream& bs);
};


//-----------------------------------------------------------------------------
// CompositorClientDestroyMirror_1

// This is a request to destroy the shared mirror texture.
struct RPCCompositorClientDestroyMirrorParams
{
    // Virtual HMD corresponding to this request.
    VirtualHmdId HMD;

    bool Serialize(bool write, Net::BitStream& bs);
};

struct RPCCompositorClientDestroyMirrorResult
{
    bool Serialize(bool write, Net::BitStream& bs);
};


//-----------------------------------------------------------------------------
// CompositorClientDestroy_1

// This is a request to destroy the compositor client connection.
struct RPCCompositorClientDestroyParams
{
    // Virtual HMD corresponding to this request.
    VirtualHmdId HMD;

    bool Serialize(bool write, Net::BitStream& bs);
};

struct RPCCompositorClientDestroyResult
{
    bool Serialize(bool write, Net::BitStream& bs);
};


}} // namespace OVR::Service

#endif // OVR_Service_Common_Compositor_h
