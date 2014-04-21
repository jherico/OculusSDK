/************************************************************************************

Filename    :   OVR_CAPI_HMDRenderState.cpp
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




#include "CAPI_HMDRenderState.h"


namespace OVR { namespace CAPI {



//-------------------------------------------------------------------------------------
// ***** HMDRenderState


HMDRenderState::HMDRenderState(ovrHmd hmd, Profile* userProfile, const OVR::HMDInfo& hmdInfo)
    : HMD(hmd), HMDInfo(hmdInfo)
{
	RenderInfo = GenerateHmdRenderInfoFromHmdInfo( HMDInfo, userProfile );

    Distortion[0] = CalculateDistortionRenderDesc(StereoEye_Left,  RenderInfo, 0);
    Distortion[1] = CalculateDistortionRenderDesc(StereoEye_Right, RenderInfo, 0);

    ClearColor[0] = ClearColor[1] = ClearColor[2] = ClearColor[3] =0.0f;
}

HMDRenderState::~HMDRenderState()
{

}


ovrHmdDesc HMDRenderState::GetDesc()
{
    ovrHmdDesc d;
    memset(&d, 0, sizeof(d));
    
    d.Type = ovrHmd_Other;
     
    d.ProductName       = HMDInfo.ProductName;    
    d.Manufacturer      = HMDInfo.Manufacturer;
    d.Resolution.w      = HMDInfo.ResolutionInPixels.w;
    d.Resolution.h      = HMDInfo.ResolutionInPixels.h;
    d.WindowsPos.x      = HMDInfo.DesktopX;
    d.WindowsPos.y      = HMDInfo.DesktopY;
    d.DisplayDeviceName = HMDInfo.DisplayDeviceName;
    d.DisplayId         = HMDInfo.DisplayId;

    d.Caps              = ovrHmdCap_YawCorrection | ovrHmdCap_Orientation | ovrHmdCap_Present;

    if (strstr(HMDInfo.ProductName, "DK1"))
    {
        d.Type = ovrHmd_DK1;        
    }
    else if (strstr(HMDInfo.ProductName, "DK2"))
    {
        d.Type = ovrHmd_DK2;
        d.Caps |= ovrHmdCap_Position | ovrHmdCap_LowPersistence;
    }
        
    DistortionRenderDesc& leftDistortion  = Distortion[0];
    DistortionRenderDesc& rightDistortion = Distortion[1];
  
    // The suggested FOV (assuming eye rotation)
    d.DefaultEyeFov[0] = CalculateFovFromHmdInfo(StereoEye_Left, leftDistortion, RenderInfo, OVR_DEFAULT_EXTRA_EYE_ROTATION);
    d.DefaultEyeFov[1] = CalculateFovFromHmdInfo(StereoEye_Right, rightDistortion, RenderInfo, OVR_DEFAULT_EXTRA_EYE_ROTATION);

    // FOV extended across the entire screen
    d.MaxEyeFov[0] = GetPhysicalScreenFov(StereoEye_Left, leftDistortion);
    d.MaxEyeFov[1] = GetPhysicalScreenFov(StereoEye_Right, rightDistortion);

    if (HMDInfo.Shutter.Type == HmdShutter_RollingRightToLeft)
    {
        d.EyeRenderOrder[0] = ovrEye_Right;
        d.EyeRenderOrder[1] = ovrEye_Left;
    }
    else
    {
        d.EyeRenderOrder[0] = ovrEye_Left;
        d.EyeRenderOrder[1] = ovrEye_Right;
    }    

    return d;
}


ovrSizei HMDRenderState::GetFOVTextureSize(int eye, ovrFovPort fov, float pixelsPerDisplayPixel)
{
    OVR_ASSERT((unsigned)eye < 2);
    StereoEye seye = (eye == ovrEye_Left) ? StereoEye_Left : StereoEye_Right;
    return CalculateIdealPixelSize(seye, Distortion[eye], fov, pixelsPerDisplayPixel);
}

ovrEyeRenderDesc HMDRenderState::calcRenderDesc(const ovrEyeDesc& eyeDesc)
{    
    HmdRenderInfo&   hmdri = RenderInfo;
    StereoEye        eye   = (eyeDesc.Eye == ovrEye_Left) ? StereoEye_Left : StereoEye_Right;
    ovrEyeRenderDesc e0;

    e0.Desc                      = eyeDesc;
    e0.ViewAdjust                = CalculateEyeVirtualCameraOffset(hmdri, eye, false);
    e0.DistortedViewport         = GetFramebufferViewport(eye, hmdri);
    e0.PixelsPerTanAngleAtCenter = Distortion[0].PixelsPerTanAngleAtCenter;

    // If RenderViewport is uninitialized, set it to texture size.
    if (Sizei(e0.Desc.RenderViewport.Size) == Sizei(0))
        e0.Desc.RenderViewport.Size = e0.Desc.TextureSize;

    return e0;
}


void HMDRenderState::setupRenderDesc( ovrEyeRenderDesc eyeRenderDescOut[2],
                                      const ovrEyeDesc eyeDescIn[2] )
{
    eyeRenderDescOut[0] = EyeRenderDesc[0] = calcRenderDesc(eyeDescIn[0]);
    eyeRenderDescOut[1] = EyeRenderDesc[1] = calcRenderDesc(eyeDescIn[1]);    
}


}} // namespace OVR::CAPI

