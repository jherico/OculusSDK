
// STAGE 4
// ======
// Complete the configurarion of VR, 
// and hook Rift orientation and position sensors into our cameras.
            
#define STAGE4_ConfigureVR ovrEyeRenderDesc eyeRenderDesc[2];                                                                                               \
                           eyeRenderDesc[0] = ovr_GetRenderDesc(session, ovrEye_Left, HMDInfo.DefaultEyeFov[0]);                                            \
                           eyeRenderDesc[1] = ovr_GetRenderDesc(session, ovrEye_Right, HMDInfo.DefaultEyeFov[1]);                                           \

#define STAGE4_GetEyePoses ovrPosef    EyeRenderPose[2];                                                                                                    \
                           ovrVector3f HmdToEyeOffset[2] = {eyeRenderDesc[0].HmdToEyeOffset,eyeRenderDesc[1].HmdToEyeOffset};                   \
                           double           ftiming = ovr_GetPredictedDisplayTime(session, 0);                                                              \
                           ovrTrackingState hmdState = ovr_GetTrackingState(session, ftiming, ovrTrue);                                                     \
                           ovr_CalcEyePoses(hmdState.HeadPose.ThePose, HmdToEyeOffset, EyeRenderPose);

#define STAGE4_GetMatrices XMVECTOR eyeQuat = XMVectorSet(EyeRenderPose[eye].Orientation.x, EyeRenderPose[eye].Orientation.y,								  \
	                                                      EyeRenderPose[eye].Orientation.z, EyeRenderPose[eye].Orientation.w);								  \
                           XMVECTOR eyePos = XMVectorSet(EyeRenderPose[eye].Position.x, EyeRenderPose[eye].Position.y, EyeRenderPose[eye].Position.z, 0);	  \
                           XMVECTOR CombinedPos = XMVectorAdd(mainCam.Pos, XMVector3Rotate(eyePos, mainCam.Rot));											  \
                           Camera finalCam(CombinedPos, (XMQuaternionMultiply(eyeQuat, mainCam.Rot)));													  \
                           XMMATRIX view = finalCam.GetViewMatrix();																						  \
                           ovrMatrix4f p = ovrMatrix4f_Projection(eyeRenderDesc[eye].Fov, 0.2f, 1000.0f, ovrProjection_None);						          \
                           XMMATRIX proj = XMMatrixSet(p.M[0][0], p.M[1][0], p.M[2][0], p.M[3][0],															  \
                           	                           p.M[0][1], p.M[1][1], p.M[2][1], p.M[3][1],															  \
                           	                           p.M[0][2], p.M[1][2], p.M[2][2], p.M[3][2],															  \
                           	                           p.M[0][3], p.M[1][3], p.M[2][3], p.M[3][3]);															  




// Actual code
//============
{
    STAGE2_InitSDK
    STAGE1_InitEngine(L"Stage4", &luid);
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