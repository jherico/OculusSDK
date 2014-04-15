/************************************************************************************

Filename    :   CAPI_DistortionRenderer.h
Content     :   Abstract interface for platform-specific rendering of distortion
Created     :   February 2, 2014
Authors     :   Michael Antonov

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

#ifndef OVR_CAPI_DistortionRenderer_h
#define OVR_CAPI_DistortionRenderer_h

#include "CAPI_HMDRenderState.h"
#include "CAPI_FrameTimeManager.h"


namespace OVR { namespace CAPI {

//-------------------------------------------------------------------------------------
// ***** CAPI::DistortionRenderer

// DistortionRenderer implements rendering of distortion and other overlay elements
// in platform-independent way.
// Platform-specific renderer back ends for CAPI are derived from this class.

class  DistortionRenderer : public RefCountBase<DistortionRenderer>
{
    // Quiet assignment compiler warning.
    void operator = (const DistortionRenderer&) { }
public:
    
    DistortionRenderer(ovrRenderAPIType api, ovrHmd hmd,
                       FrameTimeManager& timeManager,              
                       const HMDRenderState& renderState)
        : RenderAPI(api), HMD(hmd), TimeManager(timeManager), RState(renderState)
    { }
    virtual ~DistortionRenderer()
    { }
    

    // Configures the Renderer based on externally passed API settings. Must be
    // called before use.
    // Under D3D, apiConfig includes D3D Device pointer, back buffer and other
    // needed structures.
    virtual bool Initialize(const ovrRenderAPIConfig* apiConfig,
                            unsigned hmdCaps, unsigned distortionCaps) = 0;

    // Submits one eye texture for rendering. This is in the separate method to
    // allow "submit as you render" scenarios on horizontal screens where one
    // eye can be scanned out before the other.
    virtual void SubmitEye(int eyeId, ovrTexture* eyeTexture) = 0;

    // Finish the frame, optionally swapping buffers.
    // Many implementations may actually apply the distortion here.
    virtual void EndFrame(bool swapBuffers, unsigned char* latencyTesterDrawColor,
                                            unsigned char* latencyTester2DrawColor) = 0;
    


    // *** Creation Factory logic
    
    ovrRenderAPIType GetRenderAPI() const { return RenderAPI; }

    // Creation function for this interface, registered for API.
    typedef DistortionRenderer* (*CreateFunc)(ovrHmd hmd,
                                              FrameTimeManager &timeManager,
                                              const HMDRenderState& renderState);

    static CreateFunc APICreateRegistry[ovrRenderAPI_Count];

protected:
    const ovrRenderAPIType  RenderAPI;
    const ovrHmd            HMD;
    FrameTimeManager&       TimeManager;
    const HMDRenderState&   RState;    
};

}} // namespace OVR::CAPI


#endif // OVR_CAPI_DistortionRenderer_h


