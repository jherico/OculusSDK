/************************************************************************************

Filename    :   CAPI_DistortionRenderer.h
Content     :   Abstract interface for platform-specific rendering of distortion
Created     :   February 2, 2014
Authors     :   Michael Antonov

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

#ifndef OVR_CAPI_DistortionRenderer_h
#define OVR_CAPI_DistortionRenderer_h

#include "CAPI_HMDRenderState.h"
#include "CAPI_FrameLatencyTracker.h"
#include "CAPI_FrameTimeManager3.h"
#include "CAPI_DistortionTiming.h"
#include "../Vision/SensorFusion/Vision_SensorStateReader.h"

typedef void (*PostDistortionCallback)(void* pRenderContext);

namespace OVR { namespace CAPI {


//-------------------------------------------------------------------------------------
// ***** CAPI::DistortionRenderer

// DistortionRenderer implements rendering of distortion and other overlay elements
// in platform-independent way.
// Platform-specific renderer back ends for CAPI are derived from this class.

class DistortionRenderer : public RefCountBase<DistortionRenderer>
{
    // Quiet assignment compiler warning.
    void operator = (const DistortionRenderer&) { }
public:
    DistortionRenderer();
    virtual ~DistortionRenderer();

    // Configures the Renderer based on externally passed API settings. Must be
    // called before use.
    // Under D3D, apiConfig includes D3D Device pointer, back buffer and other
    // needed structures.
    bool Initialize(ovrRenderAPIConfig const * apiConfig,
                    Vision::TrackingStateReader* stateReader,
                    DistortionTimer* distortionTiming,
                    HMDRenderState const * renderState);

    // Submits one eye texture for rendering. This is in the separate method to
    // allow "submit as you render" scenarios on horizontal screens where one
    // eye can be scanned out before the other.
    virtual void SubmitEye(int eyeId, const ovrTexture* eyeTexture) = 0;
    virtual void SubmitEyeWithDepth(int eyeId, const ovrTexture* eyeColorTexture, const ovrTexture* eyeDepthTexture) = 0;

    // Finish the frame, optionally swapping buffers.
    // Many implementations may actually apply the distortion here.
    virtual void EndFrame(uint32_t frameIndex, bool swapBuffers) = 0;

    void RegisterPostDistortionCallback(PostDistortionCallback postDistortionCallback)
    {
        RegisteredPostDistortionCallback = postDistortionCallback;
    }

	// Stores the current graphics pipeline state so it can be restored later.
	void SaveGraphicsState() { if (GfxState && !(RenderState->DistortionCaps & ovrDistortionCap_NoRestore)) GfxState->Save(); }

	// Restores the saved graphics pipeline state.
	void RestoreGraphicsState() { if (GfxState && !(RenderState->DistortionCaps & ovrDistortionCap_NoRestore)) GfxState->Restore(); }

    // *** Creation Factory logic

    ovrRenderAPIType GetRenderAPI() const { return RenderAPI; }

    // Creation function for this interface, registered for API.
    typedef DistortionRenderer* (*CreateFunc)();

    static CreateFunc APICreateRegistry[ovrRenderAPI_Count];

    // Color is expected to be 3 byte RGB
    void SetLatencyTestColor(unsigned char* color);
    void SetLatencyTest2Color(unsigned char* color);

    void SetPositionTimewarpDesc(const ovrPositionTimewarpDesc& posTimewarpDesc) { PositionTimewarpDesc = posTimewarpDesc; }

protected:
    virtual bool initializeRenderer(const ovrRenderAPIConfig* apiConfig) = 0;

	// Used for pixel luminance overdrive on DK2 displays
	// A copy of back buffer images will be ping ponged
	// TODO: figure out 0 dynamically based on DK2 latency?
	static const int	NumOverdriveTextures = 2;
	int					LastUsedOverdriveTextureIndex;

    bool                LatencyTestActive;
    unsigned char       LatencyTestDrawColor[3];
    bool                LatencyTest2Active;
    unsigned char       LatencyTest2DrawColor[3];

    bool IsOverdriveActive()
	{
		// doesn't make sense to use overdrive when vsync is disabled as we cannot guarantee
		// when the rendered frame will be displayed
		return LastUsedOverdriveTextureIndex >= 0 && (RenderState->EnabledHmdCaps & ovrHmdCap_NoVSync) == 0;
	}

    void GetOverdriveScales(float& outRiseScale, float& outFallScale);

    double WaitTillTime(double absTime);

#ifdef OVR_OS_WIN32
    HANDLE timer;
    LARGE_INTEGER waitableTimerInterval;
#endif

    class GraphicsState : public RefCountBase<GraphicsState>
    {
    public:
        GraphicsState() : IsValid(false) {}
        virtual ~GraphicsState() {}
        virtual void Save() = 0;
        virtual void Restore() = 0;
        
    protected:
        bool IsValid;
    };

    ovrRenderAPIType        RenderAPI;
    Vision::TrackingStateReader* SensorReader; // For reading head pose for timewarp
    DistortionTimer*        Timing;
    HMDRenderState const *  RenderState;

    Ptr<GraphicsState>      GfxState;
    ovrPositionTimewarpDesc PositionTimewarpDesc;
    PostDistortionCallback  RegisteredPostDistortionCallback;
};


}} // namespace OVR::CAPI


#endif // OVR_CAPI_DistortionRenderer_h
