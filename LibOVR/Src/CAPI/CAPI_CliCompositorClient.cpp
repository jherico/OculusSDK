/************************************************************************************

Filename    :   CAPI_CliCompositorClient.cpp
Content     :   Base class for client connection to the compositor service.
Created     :   December 16, 2014
Authors     :   Reza Nourai

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

#include "CAPI_CliCompositorClient.h"
#include "CAPI_HMDState.h"

#if !defined(OVR_OS_MS)
#include <unistd.h>
#endif

namespace OVR { namespace CAPI {

//-------------------------------------------------------------------------------------
// ***** CAPI::CliCompositorClient

CliCompositorClient::CliCompositorClient(HMDState const* hmdState)
    : HmdState(hmdState)
    , Connected(false)
{
    // Not valid to use compositor without service connection
    OVR_ASSERT(HmdState && HmdState->pClient);
}

CliCompositorClient::~CliCompositorClient()
{
    compDisconnect();
}

#if defined (OVR_OS_MS)
OVRError CliCompositorClient::compConnect(const GraphicsAdapterID& adapterID,
                                           HANDLE fenceHandle,
                                           HANDLE frameQueueSemaphoreHandle)
#else
OVRError CliCompositorClient::compConnect(const GraphicsAdapterID& adapterID)
#endif
{
    if (Connected)
    {
        return OVRError::Success();
    }

    RPCCompositorClientCreateParams params = RPCCompositorClientCreateParams();
    params.HMD = HmdState->NetId;
    params.AdapterID = adapterID;
    params.ProcessId = GetCurrentProcessId();
    params.RiftInfo.DisplayUUID = HmdState->OurHMDInfo.DisplayDeviceName;
    params.RiftInfo.OurProfileRenderInfo = HmdState->RenderState.OurProfileRenderInfo;
#if defined (OVR_OS_MS)
    params.FenceHandle = (handle64_t)fenceHandle;
    params.FrameQueueSemaphoreHandle = (handle64_t)frameQueueSemaphoreHandle;
#endif

    RPCCompositorClientCreateResult result;
    OVRError err = HmdState->pClient->Compositor_Create_1(params, result);
    if (!err.Succeeded())
    {
        return err;
    }

    // Success
    Connected = true;

    // Make sure that we call compDisconnect() if additional initialization fails below:

#if defined (OVR_OS_MS)
    err = HmdState->pClient->IPCClient.Initialize(result.IPCKey);
#endif

    // If not a debug device,
    if (err.Succeeded() && !HmdState->OurHMDInfo.DebugDevice)
    {
        // Open the app timing shared memory object to sync up with distortion timing.
        err = HmdState->RenderTimer.Open(result.AppTimingName.ToCStr());
    }

    if (!err.Succeeded())
    {
        // Immediately disconnect to notify the server we failed.
        // Ignore secondary errors from disconnecting.
        compDisconnect();

        return err;
    }

    return OVRError::Success();
}

OVRError CliCompositorClient::compDisconnect()
{
    if (!Connected)
    {
        // Called during teardown, even when connection isn't made
        return OVRError::Success();
    }

    RPCCompositorClientDestroyParams params = RPCCompositorClientDestroyParams();
    params.HMD = HmdState->NetId;

    RPCCompositorClientDestroyResult result;
    OVRError err = HmdState->pClient->Compositor_Destroy_1(params, result);
    if (!err.Succeeded())
    {
        return err;
    }

    // Success
    Connected = false;
    return err;
}

#if defined(OVR_OS_MS)
OVRError CliCompositorClient::compCreateTextureSet(const std::vector<HANDLE>& shareHandles, uint32_t* textureSetID)
#elif defined(OVR_OS_MAC)
OVRError CliCompositorClient::compCreateTextureSet(const std::vector<mach_port_t>& shareHandles, uint32_t* textureSetID)
#elif defined(OVR_OS_LINUX)
// TODO: Implement for Linux (steaphan)
OVRError CliCompositorClient::compCreateTextureSet(const std::vector<uint32_t>& shareHandles, uint32_t* textureSetID)
#else
#error Implement for Other
#endif
{
    if (!Connected)
    {
        return OVR_MAKE_ERROR(ovrError_ServiceConnection, "Not connected to the compositor.");
    }

    RPCCompositorTextureSetCreateParams params;
    params.HMD = HmdState->NetId;

    for (auto textureHandle : shareHandles)
    {
        params.TextureShareHandles.PushBack((handle64_t)textureHandle);
    }

    RPCCompositorTextureSetCreateResult result;
    OVRError err = HmdState->pClient->Compositor_TextureSet_Create_1(params, result);
    if (!err.Succeeded())
    {
        return err;
    }

    // Success
    if (textureSetID)
    {
        *textureSetID = result.TextureSetID;
    }
    return err;
}

OVRError CliCompositorClient::compDestroyTextureSet(uint32_t id)
{
    if (!Connected)
    {
        // Called during teardown, even when connection isn't made
        return OVRError::Success();
    }

    RPCCompositorTextureSetDestroyParams params;
    params.HMD = HmdState->NetId;
    params.TextureSetID = id;

    RPCCompositorTextureSetDestroyResult result;
    OVRError err = HmdState->pClient->Compositor_TextureSet_Destroy_1(params, result);
    if (!err.Succeeded())
    {
        return err;
    }

    // Success
    return err;
}

OVRError CliCompositorClient::compCreateMirrorTexture(handle64_t textureHandle)
{
    if (!Connected)
    {
        return OVR_MAKE_ERROR(ovrError_ServiceConnection, "Not connected to the compositor.");
    }

    RPCCompositorClientCreateMirrorParams params = RPCCompositorClientCreateMirrorParams();
    params.HMD = HmdState->NetId;
    params.TextureHandle = textureHandle;

    RPCCompositorClientCreateMirrorResult result;
    OVRError err = HmdState->pClient->Compositor_CreateMirror_1(params, result);
    if (!err.Succeeded())
    {
        return err;
    }

    // Success
    return err;
}

OVRError CliCompositorClient::compDestroyMirrorTexture()
{
    if (!Connected)
    {
        return OVRError::Success();
    }

    RPCCompositorClientDestroyMirrorParams params = RPCCompositorClientDestroyMirrorParams();
    params.HMD = HmdState->NetId;

    RPCCompositorClientDestroyMirrorResult result;
    OVRError err = HmdState->pClient->Compositor_DestroyMirror_1(params, result);
    if (!err.Succeeded())
    {
        return err;
    }

    // Success
    return err;
}

OVRError CliCompositorClient::compSubmitLayers(const ArrayPOD<CompositorLayerDesc>& layers)
{
    if (!Connected)
    {
        return OVR_MAKE_ERROR(ovrError_ServiceConnection, "Not connected to the compositor.");
    }

    IPCCompositorSubmitLayersParams params = IPCCompositorSubmitLayersParams();
    params.HMD             = HmdState->NetId;
    params.Layers          = layers;

    IPCCompositorSubmitLayersResult result;
    OVRError err = HmdState->pClient->Compositor_SubmitLayers_1(params, result);
    if (!err.Succeeded())
    {
        return err;
    }

    // Success
    return err;
}

OVRError CliCompositorClient::compEndFrame(uint32_t appFrameIndex, ovrViewScaleDesc const *viewScaleDesc)
{
    if (!Connected)
    {
        return OVR_MAKE_ERROR(ovrError_ServiceConnection, "Not connected to the compositor.");
    }

    IPCCompositorEndFrameParams params = IPCCompositorEndFrameParams();

    params.HMD = HmdState->NetId;
    params.CenteredFromWorld = HmdState->TheTrackingStateReader.GetCenteredFromWorld();
    params.EnabledDistortionCaps = ovrDistortionCap_Default;
    static_assert(OVR_ARRAY_COUNT(params.DistortionClearColor) == 4, "Update count");

    for (int i = 0; i < 4; ++i)
    {
        params.DistortionClearColor[i] = HmdState->RenderState.ClearColor[i];
    }
    // Look up the timing information for this app frame index
    AppTimingHistoryRecord appTimingRecord = HmdState->TimingHistory.Lookup(appFrameIndex);

    // Add client-side timing information for this app frame
    params.AppTiming.AppFrameIndex = appFrameIndex;
    params.AppTiming.AppRenderIMUTime = appTimingRecord.RenderIMUTime;
    params.AppTiming.AppScanoutStartTime = appTimingRecord.Timing.ScanoutStartTime;

    if (viewScaleDesc != nullptr)
    {
        params.ViewScaleDesc = *viewScaleDesc;
    }

    IPCCompositorEndFrameResult result;
    OVRError err = HmdState->pClient->Compositor_EndFrame_1(params, result);
    if (!err.Succeeded())
    {
        return err;
    }

    // Success
    LatencyTimings = result.LatencyTimings;
    return err;
}


}} // namespace OVR::CAPI
