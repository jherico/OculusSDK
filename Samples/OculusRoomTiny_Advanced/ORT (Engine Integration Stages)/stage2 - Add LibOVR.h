
// STAGE 2
// =======
// Now we incorporate the Oculus SDK, and incorporate init and release
// functionality in the Init and release stages of the engine.

#define STAGE2_InitSDK     ovrResult result = ovr_Initialize(nullptr);                                     \
                           VALIDATE(result == ovrSuccess, "Failed to initialize libOVR.");                 \
                           ovrHmd HMD;                                                                     \
                           result = ovrHmd_Create(0, &HMD);                                                \
                           if (result != ovrSuccess) result = ovrHmd_CreateDebug(ovrHmd_DK2, &HMD);        \
                           VALIDATE(result == ovrSuccess, "Oculus Rift not detected.");                    \
                           VALIDATE(HMD->ProductName[0] != '\0', "Rift detected, display not enabled.");


#define STAGE2_ReleaseSDK  ovrHmd_Destroy(HMD);                                                            \
                           ovr_Shutdown();

// Actual code
//============
{
    STAGE1_InitEngine;
    STAGE2_InitSDK                 /*NEW*/
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