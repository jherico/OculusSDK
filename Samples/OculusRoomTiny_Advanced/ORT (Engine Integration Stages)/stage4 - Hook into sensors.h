
// STAGE 4
// ======
// Complete the configurarion of VR, 
// and hook Rift orientation and position sensors into our cameras.
            
#define STAGE4_ConfigureVR ovrHmd_SetEnabledCaps(HMD, ovrHmdCap_LowPersistence | ovrHmdCap_DynamicPrediction);                                  \
                           result = ovrHmd_ConfigureTracking(HMD, ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection                  \
                                                                | ovrTrackingCap_Position, 0);                                                  \
                           VALIDATE(result == ovrSuccess, "Failed to configure tracking.");                                                     \
                           ovrEyeRenderDesc eyeRenderDesc[2];                                                                                   \
                           eyeRenderDesc[0] = ovrHmd_GetRenderDesc(HMD, ovrEye_Left, HMD->DefaultEyeFov[0]);                                    \
                           eyeRenderDesc[1] = ovrHmd_GetRenderDesc(HMD, ovrEye_Right, HMD->DefaultEyeFov[1]);                                   \

#define STAGE4_GetEyePoses ovrPosef    EyeRenderPose[2];                                                                                        \
                           ovrVector3f HmdToEyeViewOffset[2] = {eyeRenderDesc[0].HmdToEyeViewOffset,eyeRenderDesc[1].HmdToEyeViewOffset};       \
                           ovrFrameTiming   ftiming = ovrHmd_GetFrameTiming(HMD, 0);                                                            \
                           ovrTrackingState hmdState = ovrHmd_GetTrackingState(HMD, ftiming.DisplayMidpointSeconds);                            \
                           ovr_CalcEyePoses(hmdState.HeadPose.ThePose, HmdToEyeViewOffset, EyeRenderPose);

#define STAGE4_GetMatrices Camera finalCam(mainCam.Pos + mainCam.Rot.Transform(EyeRenderPose[eye].Position),                                    \
                           mainCam.Rot * Matrix4f(EyeRenderPose[eye].Orientation));                                                             \
                           Matrix4f view = finalCam.GetViewMatrix();                                                                            \
                           Matrix4f proj = ovrMatrix4f_Projection(eyeRenderDesc[eye].Fov, 0.2f, 1000.0f, ovrProjection_RightHanded);

// Actual code
//============
{
    STAGE1_InitEngine;
    STAGE2_InitSDK
    STAGE3_CreateEyeBuffers
    STAGE3_ModelsToViewBuffers
    STAGE4_ConfigureVR                 /*NEW*/
    STAGE1_InitModelsAndCamera
    STAGE1_MainLoopReadingInput
    {
        STAGE1_MoveCameraFromInputs
        STAGE4_GetEyePoses             /*NEW*/
        STAGE3_ForEachEye
        {
            STAGE3_SetEyeRenderTarget
            STAGE4_GetMatrices         /*REPLACEMENT*/
            STAGE1_RenderModels
        }
        STAGE1_SetScreenRenderTarget
        STAGE3_RenderEyeBuffers
        STAGE1_Present
    }
    STAGE2_ReleaseSDK
    STAGE1_ReleaseEngine;
}