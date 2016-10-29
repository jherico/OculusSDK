
// STAGE 2
// =======
// Now we incorporate the Oculus SDK, and incorporate init and release
// functionality in the Init and release stages of the engine.

#define STAGE2_InitSDK     ovrResult result = ovr_Initialize(nullptr);                                     \
                           VALIDATE(result == ovrSuccess, "Failed to initialize libOVR.");                 \
                           ovrSession session;                                                             \
						   ovrGraphicsLuid luid;                                                           \
                           result = ovr_Create(&session, &luid);                                           \
                           VALIDATE(result == ovrSuccess, "Oculus Rift not detected.");                    \
                           ovrHmdDesc HMDInfo = ovr_GetHmdDesc(session);                                   \
                           UNREFERENCED_PARAMETER(HMDInfo);

#define STAGE2_ReleaseSDK  ovr_Destroy(session);                                                           \
                           ovr_Shutdown();

// Actual code
//============
{
    STAGE2_InitSDK                 /*NEW*/
    STAGE1_InitEngine(L"Stage2", &luid);
    STAGE1_InitModelsAndCamera;
    STAGE1_MainLoopReadingInput
    {
        STAGE1_MoveCameraFromInputs
        STAGE1_SetScreenRenderTarget
        STAGE1_GetMatrices
        STAGE1_RenderModels
        STAGE1_Present
    }
    STAGE2_ReleaseSDK               /*NEW*/
    STAGE1_ReleaseEngine;          
}