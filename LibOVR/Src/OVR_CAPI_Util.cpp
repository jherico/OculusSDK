/************************************************************************************

PublicHeader:   OVR_CAPI_Util.c
Copyright   :   Copyright 2014-2016 Oculus VR, LLC All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.3 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.3 

Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*************************************************************************************/


#include <Extras/OVR_CAPI_Util.h>
#include <Extras/OVR_StereoProjection.h>


#if defined(_MSC_VER)
    #include <emmintrin.h>
    #pragma intrinsic(_mm_pause)
#endif

#if defined(_WIN32)
    // Prevents <Windows.h> from defining min() and max() macro symbols.
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif

    #include <windows.h>
#endif



// Used to generate projection from ovrEyeDesc::Fov
OVR_PUBLIC_FUNCTION(ovrMatrix4f) ovrMatrix4f_Projection(
    ovrFovPort fov, float znear, float zfar, unsigned int projectionModFlags)
{
    bool leftHanded     = (projectionModFlags & ovrProjection_LeftHanded) > 0;
    bool flipZ          = (projectionModFlags & ovrProjection_FarLessThanNear) > 0;
    bool farAtInfinity  = (projectionModFlags & ovrProjection_FarClipAtInfinity) > 0;
    bool isOpenGL       = (projectionModFlags & ovrProjection_ClipRangeOpenGL) > 0;

    // TODO: Pass in correct eye to CreateProjection if we want to support canted displays from CAPI
    return OVR::CreateProjection(leftHanded , isOpenGL, fov, OVR::StereoEye_Center, znear, zfar, flipZ, farAtInfinity);
}

OVR_PUBLIC_FUNCTION(ovrTimewarpProjectionDesc) ovrTimewarpProjectionDesc_FromProjection(
    ovrMatrix4f Projection, unsigned int projectionModFlags)
{
    ovrTimewarpProjectionDesc res;
    res.Projection22 = Projection.M[2][2];
    res.Projection23 = Projection.M[2][3];
    res.Projection32 = Projection.M[3][2];

    if ((res.Projection32 != 1.0f) && (res.Projection32 != -1.0f))
    {
        // This is a very strange projection matrix, and probably won't work.
        // If you need it to work, please contact Oculus and let us know your usage scenario.
    }

    if ( ( projectionModFlags & ovrProjection_ClipRangeOpenGL ) != 0 )
    {
        // Internally we use the D3D range of [0,+w] not the OGL one of [-w,+w], so we need to convert one to the other.
        // Note that the values in the depth buffer, and the actual linear depth we want is the same for both APIs,
        // the difference is purely in the values inside the projection matrix.

        // D3D does this:
        // depthBuffer =             ( ProjD3D.M[2][2] * linearDepth + ProjD3D.M[2][3] ) / ( linearDepth * ProjD3D.M[3][2] );
        // OGL does this:
        // depthBuffer = 0.5 + 0.5 * ( ProjOGL.M[2][2] * linearDepth + ProjOGL.M[2][3] ) / ( linearDepth * ProjOGL.M[3][2] );

        // Therefore:
        // ProjD3D.M[2][2] = 0.5 * ( ProjOGL.M[2][2] + ProjOGL.M[3][2] );
        // ProjD3D.M[2][3] = 0.5 *   ProjOGL.M[2][3];
        // ProjD3D.M[3][2] =         ProjOGL.M[3][2];

        res.Projection22 = 0.5f * ( Projection.M[2][2] + Projection.M[3][2] );
        res.Projection23 = 0.5f *   Projection.M[2][3];
        res.Projection32 =          Projection.M[3][2];
    }
    return res;
}

OVR_PUBLIC_FUNCTION(ovrMatrix4f) ovrMatrix4f_OrthoSubProjection(
    ovrMatrix4f projection, ovrVector2f orthoScale,
    float orthoDistance, float hmdToEyeOffsetX)
{
    ovrMatrix4f ortho;
    // Negative sign is correct!
    // If the eye is offset to the left, then the ortho view needs to be offset to the right relative to the camera.
    float orthoHorizontalOffset = -hmdToEyeOffsetX / orthoDistance;

    /*
    // Current projection maps real-world vector (x,y,1) to the RT.
    // We want to find the projection that maps the range [-FovPixels/2,FovPixels/2] to
    // the physical [-orthoHalfFov,orthoHalfFov]
    // Note moving the offset from M[0][2]+M[1][2] to M[0][3]+M[1][3] - this means
    // we don't have to feed in Z=1 all the time.
    // The horizontal offset math is a little hinky because the destination is
    // actually [-orthoHalfFov+orthoHorizontalOffset,orthoHalfFov+orthoHorizontalOffset]
    // So we need to first map [-FovPixels/2,FovPixels/2] to
    //                         [-orthoHalfFov+orthoHorizontalOffset,orthoHalfFov+orthoHorizontalOffset]:
    // x1 = x0 * orthoHalfFov/(FovPixels/2) + orthoHorizontalOffset;
    //    = x0 * 2*orthoHalfFov/FovPixels + orthoHorizontalOffset;
    // But then we need the same mapping as the existing projection matrix, i.e.
    // x2 = x1 * Projection.M[0][0] + Projection.M[0][2];
    //    = x0 * (2*orthoHalfFov/FovPixels + orthoHorizontalOffset) * Projection.M[0][0] + Projection.M[0][2];
    //    = x0 * Projection.M[0][0]*2*orthoHalfFov/FovPixels +
    //      orthoHorizontalOffset*Projection.M[0][0] + Projection.M[0][2];
    // So in the new projection matrix we need to scale by Projection.M[0][0]*2*orthoHalfFov/FovPixels and
    // offset by orthoHorizontalOffset*Projection.M[0][0] + Projection.M[0][2].
    */

    ortho.M[0][0] = projection.M[0][0] * orthoScale.x;
    ortho.M[0][1] = 0.0f;
    ortho.M[0][2] = 0.0f;
    ortho.M[0][3] = -projection.M[0][2] + ( orthoHorizontalOffset * projection.M[0][0] );

    ortho.M[1][0] = 0.0f;
    ortho.M[1][1] = -projection.M[1][1] * orthoScale.y;       /* Note sign flip (text rendering uses Y=down). */
    ortho.M[1][2] = 0.0f;
    ortho.M[1][3] = -projection.M[1][2];

    ortho.M[2][0] = 0.0f;
    ortho.M[2][1] = 0.0f;
    ortho.M[2][2] = 0.0f;
    ortho.M[2][3] = 0.0f;

    /* No perspective correction for ortho. */
    ortho.M[3][0] = 0.0f;
    ortho.M[3][1] = 0.0f;
    ortho.M[3][2] = 0.0f;
    ortho.M[3][3] = 1.0f;

    return ortho;
}


OVR_PUBLIC_FUNCTION(void) ovr_CalcEyePoses(ovrPosef headPose,
    const ovrVector3f hmdToEyeOffset[2],
    ovrPosef outEyePoses[2])
{
    if (!hmdToEyeOffset || !outEyePoses)
    {
        return;
    }

    using OVR::Posef;
    using OVR::Vector3f;

    // Currently hmdToEyeOffset is only a 3D vector
    outEyePoses[0] = Posef(headPose.Orientation, ((Posef)headPose).Apply((Vector3f)hmdToEyeOffset[0]));
    outEyePoses[1] = Posef(headPose.Orientation, ((Posef)headPose).Apply((Vector3f)hmdToEyeOffset[1]));
}


OVR_PUBLIC_FUNCTION(void) ovr_GetEyePoses(ovrSession session, long long frameIndex, ovrBool latencyMarker,
    const ovrVector3f hmdToEyeOffset[2],    
    ovrPosef outEyePoses[2],
    double* outSensorSampleTime)
{
    double frameTime = ovr_GetPredictedDisplayTime(session, frameIndex);
    ovrTrackingState trackingState = ovr_GetTrackingState(session, frameTime, latencyMarker);
    ovr_CalcEyePoses(trackingState.HeadPose.ThePose, hmdToEyeOffset, outEyePoses);

    if (outSensorSampleTime != nullptr)
    {
        *outSensorSampleTime = ovr_GetTimeInSeconds();
    }
}

OVR_PUBLIC_FUNCTION(ovrDetectResult) ovr_Detect(int timeoutMilliseconds)
{
    // Initially we assume everything is not running.
    ovrDetectResult result;
    result.IsOculusHMDConnected = ovrFalse;
    result.IsOculusServiceRunning = ovrFalse;

#if defined(_WIN32)
    // Attempt to open the named event.
    HANDLE hServiceEvent = ::OpenEventW(SYNCHRONIZE, FALSE, OVR_HMD_CONNECTED_EVENT_NAME);

    // If event exists,
    if (hServiceEvent != nullptr)
    {
        // This indicates that the Oculus Runtime is installed and running.
        result.IsOculusServiceRunning = ovrTrue;

        // Poll for event state.
        DWORD objectResult = ::WaitForSingleObject(hServiceEvent, timeoutMilliseconds);

        // If the event is signaled,
        if (objectResult == WAIT_OBJECT_0)
        {
            // This indicates that the Oculus HMD is connected.
            result.IsOculusHMDConnected = ovrTrue;
        }

        ::CloseHandle(hServiceEvent);
    }
#endif // _WIN32


    return result;
}

OVR_PUBLIC_FUNCTION(void) ovrPosef_FlipHandedness(const ovrPosef* inPose, ovrPosef* outPose)
{
    outPose->Orientation.x = -inPose->Orientation.x;
    outPose->Orientation.y = inPose->Orientation.y;
    outPose->Orientation.z = inPose->Orientation.z;
    outPose->Orientation.w = -inPose->Orientation.w;

    outPose->Position.x = -inPose->Position.x;
    outPose->Position.y = inPose->Position.y;
    outPose->Position.z = inPose->Position.z;
}
