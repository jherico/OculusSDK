/************************************************************************************

PublicHeader:   OVR_CAPI_Util.h
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

*************************************************************************************/

#ifndef OVR_CAPI_Util_h
#define OVR_CAPI_Util_h


#include "OVR_CAPI.h"


#ifdef __cplusplus
extern "C" {
#endif

/// Enumerates modifications to the projection matrix based on the application's needs
typedef enum
{
    /// Use for generating a default projection matrix that is:
    /// * Left-handed
    /// * Near depth values stored in the depth buffer are smaller than far depth values
    /// * Both near and far are explicitly defined
    /// * With a clipping range that is (0 to w)
    ovrProjection_None = 0x00,

    /// Enable if using right-handed transformations in your application
    ovrProjection_RightHanded = 0x01,

    /// After projection transform is applied, far values stored in the depth buffer will be less than closer depth values
    /// NOTE: Enable only if application is using a floating-point depth buffer for proper precision
    ovrProjection_FarLessThanNear = 0x02,

    /// When this flag is used, the zfar value pushed into ovrMatrix4f_Projection() will be ignored
    /// NOTE: Enable only if ovrProjection_FarLessThanNear is also enabled where the far clipping plane will be pushed to infinity
    ovrProjection_FarClipAtInfinity = 0x04,

    /// Enable if application is rendering with OpenGL and expects a projection matrix with a clipping range of (-w to w)
    /// Ignore this flag if your application already handles the conversion from D3D range (0 to w) to OpenGL
    ovrProjection_ClipRangeOpenGL = 0x08,
} ovrProjectionModifier;

/// Used to generate projection from ovrEyeDesc::Fov.
/// projectionFlags is a combination of the ovrProjectionModifier flags defined above
OVR_PUBLIC_FUNCTION(ovrMatrix4f) ovrMatrix4f_Projection(ovrFovPort fov, float znear, float zfar, unsigned int projectionModFlags);

/// Used for 2D rendering, Y is down
/// orthoScale = 1.0f / pixelsPerTanAngleAtCenter
/// orthoDistance = distance from camera, such as 0.8m
OVR_PUBLIC_FUNCTION(ovrMatrix4f) ovrMatrix4f_OrthoSubProjection(ovrMatrix4f projection, ovrVector2f orthoScale,
                                                                float orthoDistance, float hmdToEyeViewOffsetX);

/// Waits until the specified absolute time.
OVR_PUBLIC_FUNCTION(double) ovr_WaitTillTime(double absTime);


#ifdef __cplusplus
} /* extern "C" */
#endif


#endif // Header include guard
