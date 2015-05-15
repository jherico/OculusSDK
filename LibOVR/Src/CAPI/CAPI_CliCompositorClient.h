/************************************************************************************

Filename    :   CAPI_CliCompositorClient.h
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

#ifndef OVR_CAPI_CliCompositorClient_h
#define OVR_CAPI_CliCompositorClient_h

#include "OVR_CAPI.h"
#include "OVR_Stereo.h"
#include "Kernel/OVR_System.h"
#include "Service/Service_Common_Compositor.h"
#include <vector>

#if defined(OVR_OS_MAC)
#include <mach/mach.h>
#elif defined(OVR_OS_LINUX)
#endif

namespace OVR { namespace CAPI {

class HMDState;
struct HMDRenderState;

//-------------------------------------------------------------------------------------
// ***** CAPI::CliCompositorClient

// Client connection to the compositor service. The texture sets being shared
// with the compositor as well as connection and synchronization are all tracked
// here. This is the base class that API-specific versions derive from.

class CliCompositorClient : public RefCountBase<CliCompositorClient>
{
public:
    static const uint32_t InvalidTextureSetID = (uint32_t)-1;

    virtual ~CliCompositorClient();

    // CreateTextureSet has a different signature per derived class, so there is no virtual function for it

    // Destroy a texture set, freeing all the resources
    virtual OVRError DestroyTextureSet(ovrSwapTextureSet* textureSet) = 0;

    // Support shared mirror texture
    virtual OVRError DestroyMirrorTexture(ovrTexture* mirrorTexture) = 0;

    // Layer manipulation
    virtual OVRError SubmitLayer(int layerNum, LayerDesc const *layerDesc) = 0;
    virtual OVRError DisableLayer(int layerNum) = 0;

    // Complete the frame, finalize submissions, synchronize with compositor service.
    virtual OVRError EndFrame(uint32_t appFrameIndex, ovrViewScaleDesc const *viewScaleDesc) = 0;

    virtual OVRError SetQueueAheadSeconds(float queueAheadSeconds) = 0;
    virtual float GetQueueAheadSeconds() const = 0;

    // Timing
    Service::OutputLatencyTimings const& GetLatencyTimings() const { return LatencyTimings; }

protected:
    CliCompositorClient(HMDState const* hmdState);

    // Calls to the compositor service
#if defined (OVR_OS_MS)
    OVRError    compConnect(const GraphicsAdapterID& adapterID, HANDLE fenceHandle, HANDLE frameQueueSemaphoreHandle);
#else
    OVRError    compConnect(const GraphicsAdapterID& adapterID, HMDRenderState const * renderState);
#endif
    OVRError    compDisconnect();

#if defined(OVR_OS_MS)
    OVRError    compCreateTextureSet(const std::vector<HANDLE>& shareHandles, uint32_t* textureSetID);
#elif defined(OVR_OS_MAC)
    OVRError    compCreateTextureSet(const std::vector<mach_port_t>& shareHandles, uint32_t* textureSetID);
#elif defined(OVR_OS_LINUX)
    // TODO: Implement for Linux (steaphan)
    OVRError    compCreateTextureSet(const std::vector<uint32_t>& shareHandles, uint32_t* textureSetID);
#else
#error Implement for Other
#endif
    OVRError    compDestroyTextureSet(uint32_t id);

    OVRError    compCreateMirrorTexture(handle64_t textureHandle);
    OVRError    compDestroyMirrorTexture();

    OVRError    compSubmitLayers(const ArrayPOD<Service::CompositorLayerDesc>& layers);
    OVRError    compEndFrame(uint32_t appFrameIndex, ovrViewScaleDesc const *viewScaleDesc);

    bool                    Connected;
    HMDState const*         HmdState;
    Service::OutputLatencyTimings LatencyTimings;  // Latency timing results from last frame
};


}} // namespace OVR::CAPI

#endif // OVR_CAPI_CliCompositorClient_h
