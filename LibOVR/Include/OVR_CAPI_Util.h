/********************************************************************************//**

\file  OVR_CAPI_Util.h
\brief This header provides LibOVR utility function declarations

\copyright Copyright 2015 Oculus VR, LLC All Rights reserved.
\n
Licensed under the Oculus VR Rift SDK License Version 3.2 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.
\n
You may obtain a copy of the License at
\n
http://www.oculusvr.com/licenses/LICENSE-3.2 
\n
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


/// Enumerates modifications to the projection matrix based on the application's needs.
///
/// \see ovrMatrix4f_Projection
///
typedef enum ovrProjectionModifier_
{
    /// Use for generating a default projection matrix that is:
    /// * Left-handed.
    /// * Near depth values stored in the depth buffer are smaller than far depth values.
    /// * Both near and far are explicitly defined.
    /// * With a clipping range that is (0 to w).
    ovrProjection_None = 0x00,

    /// Enable if using right-handed transformations in your application.
    ovrProjection_RightHanded = 0x01,

    /// After the projection transform is applied, far values stored in the depth buffer will be less than closer depth values.
    /// NOTE: Enable only if the application is using a floating-point depth buffer for proper precision.
    ovrProjection_FarLessThanNear = 0x02,

    /// When this flag is used, the zfar value pushed into ovrMatrix4f_Projection() will be ignored
    /// NOTE: Enable only if ovrProjection_FarLessThanNear is also enabled where the far clipping plane will be pushed to infinity.
    ovrProjection_FarClipAtInfinity = 0x04,

    /// Enable if the application is rendering with OpenGL and expects a projection matrix with a clipping range of (-w to w).
    /// Ignore this flag if your application already handles the conversion from D3D range (0 to w) to OpenGL.
    ovrProjection_ClipRangeOpenGL = 0x08,
} ovrProjectionModifier;


/// Used to generate projection from ovrEyeDesc::Fov.
///
/// \param[in] fov Specifies the ovrFovPort to use.
/// \param[in] znear Distance to near Z limit.
/// \param[in] zfar Distance to far Z limit.
/// \param[in] projectionModFlags A combination of the ovrProjectionModifier flags.
///
/// \return Returns the calculated projection matrix.
/// 
/// \see ovrProjectionModifier
///
OVR_PUBLIC_FUNCTION(ovrMatrix4f) ovrMatrix4f_Projection(ovrFovPort fov, float znear, float zfar, unsigned int projectionModFlags);


/// Extracts the required data from the result of ovrMatrix4f_Projection.
///
/// \param[in] projection Specifies the project matrix from which to extract ovrTimewarpProjectionDesc.
/// \return Returns the extracted ovrTimewarpProjectionDesc.
/// \see ovrTimewarpProjectionDesc
///
OVR_PUBLIC_FUNCTION(ovrTimewarpProjectionDesc) ovrTimewarpProjectionDesc_FromProjection(ovrMatrix4f projection);


/// Generates an orthographic sub-projection.
///
/// Used for 2D rendering, Y is down.
///
/// \param[in] projection The perspective matrix that the orthographic matrix is derived from.
/// \param[in] orthoScale Equal to 1.0f / pixelsPerTanAngleAtCenter.
/// \param[in] orthoDistance Equal to the distance from the camera in meters, such as 0.8m.
/// \param[in] hmdToEyeViewOffsetX Specifies the offset of the eye from the center.
///
/// \return Returns the calculated projection matrix.
///
OVR_PUBLIC_FUNCTION(ovrMatrix4f) ovrMatrix4f_OrthoSubProjection(ovrMatrix4f projection, ovrVector2f orthoScale,
                                                                float orthoDistance, float hmdToEyeViewOffsetX);



/// Computes offset eye poses based on headPose returned by ovrTrackingState.
///
/// \param[in] headPose Indicates the HMD position and orientation to use for the calculation.
/// \param[in] hmdToEyeViewOffset Can be ovrEyeRenderDesc.HmdToEyeViewOffset returned from 
///            ovrHmd_GetRenderDesc. For monoscopic rendering, use a vector that is the average 
///            of the two vectors for both eyes.
/// \param[out] outEyePoses If outEyePoses are used for rendering, they should be passed to 
///             ovrHmd_SubmitFrame in ovrLayerEyeFov::RenderPose or ovrLayerEyeFovDepth::RenderPose.
///
OVR_PUBLIC_FUNCTION(void) ovr_CalcEyePoses(ovrPosef headPose,
                                           const ovrVector3f hmdToEyeViewOffset[2],
                                           ovrPosef outEyePoses[2]);


/// Returns the predicted head pose in outHmdTrackingState and offset eye poses in outEyePoses. 
///
/// This is a thread-safe function where caller should increment frameIndex with every frame
/// and pass that index where applicable to functions called on the rendering thread.
/// Assuming outEyePoses are used for rendering, it should be passed as a part of ovrLayerEyeFov.
/// The caller does not need to worry about applying HmdToEyeViewOffset to the returned outEyePoses variables.
///
/// \param[in]  hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in]  frameIndex Specifies the targeted frame index, or 0 to refer to one frame after 
///             the last time ovrHmd_SubmitFrame was called.
/// \param[in]  hmdToEyeViewOffset Can be ovrEyeRenderDesc.HmdToEyeViewOffset returned from 
///             ovrHmd_GetRenderDesc. For monoscopic rendering, use a vector that is the average 
///             of the two vectors for both eyes.
/// \param[out] outEyePoses The predicted eye poses.
/// \param[out] outHmdTrackingState The predicted ovrTrackingState. May be NULL, in which case it is ignored.
///
OVR_PUBLIC_FUNCTION(void) ovrHmd_GetEyePoses(ovrHmd hmd, unsigned int frameIndex,
                                             const ovrVector3f hmdToEyeViewOffset[2],
                                             ovrPosef outEyePoses[2],
                                             ovrTrackingState* outHmdTrackingState);




/// Waits until the specified absolute time.
///
/// \deprecated This function may be removed in a future version.
///
/// \param[in] absTime Specifies the absolute future time to wait until.
///
/// \see ovr_GetTimeInSeconds
/// 
OVR_PUBLIC_FUNCTION(double) ovr_WaitTillTime(double absTime);


#ifdef __cplusplus
} /* extern "C" */
#endif


#endif // Header include guard
