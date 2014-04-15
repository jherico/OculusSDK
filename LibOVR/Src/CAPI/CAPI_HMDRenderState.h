/************************************************************************************

Filename    :   CAPI_HMDRenderState.h
Content     :   Combines all of the rendering state associated with the HMD
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

#ifndef OVR_CAPI_HMDRenderState_h
#define OVR_CAPI_HMDRenderState_h

#include "../OVR_CAPI.h"
#include "../Kernel/OVR_Math.h"
#include "../Util/Util_Render_Stereo.h"


namespace OVR { namespace CAPI {

using namespace OVR::Util::Render;

//-------------------------------------------------------------------------------------
// ***** HMDRenderState

// Combines all of the rendering setup information about one HMD.

class HMDRenderState : public NewOverrideBase 
{
    // Quiet assignment compiler warning.
    void operator = (const HMDRenderState&) { }
public:   

    HMDRenderState(ovrHmd hmd, Profile* userProfile, const OVR::HMDInfo& hmdInfo);
    virtual ~HMDRenderState();


    // *** Rendering Setup

    // Delegated access APIs
    ovrHmdDesc GetDesc();
    ovrSizei   GetFOVTextureSize(int eye, ovrFovPort fov, float pixelsPerDisplayPixel);

    ovrEyeRenderDesc calcRenderDesc(const ovrEyeDesc& eyeDesc);

    void       setupRenderDesc(ovrEyeRenderDesc eyeRenderDescOut[2],
                               const ovrEyeDesc eyeDescIn[2]);
public:
    
    // HMDInfo shouldn't change, as its string pointers are passed out.    
    ovrHmd                  HMD;
    const OVR::HMDInfo&     HMDInfo;

    //const char*             pLastError;

    HmdRenderInfo            RenderInfo;    
    DistortionRenderDesc     Distortion[2];
    ovrEyeRenderDesc         EyeRenderDesc[2];

    // Clear color used for distortion
    float                    ClearColor[4];

    // Pose at which last time the eye was rendered, as submitted by EndEyeRender.
    ovrPosef                 EyeRenderPoses[2];

    // Capabilities passed to Configure.
    unsigned                 HMDCaps;
    unsigned                 DistortionCaps;
};


}} // namespace OVR::CAPI


#endif // OVR_CAPI_HMDState_h


