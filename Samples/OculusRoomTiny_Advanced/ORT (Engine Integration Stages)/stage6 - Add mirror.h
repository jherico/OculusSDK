
// STAGE 6
// =======
// Finally, we provide the means for the output to be mirrored onto
// the desktop monitor. 

#define STAGE6_CreateMirrorForMonitor   ovrMirrorTexture mirrorTexture = nullptr;                                   \
                                        ovrMirrorTextureDesc td = {};                                               \
                                        td.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;                                 \
                                        td.Width = DIRECTX.WinSizeW;                                                \
                                        td.Height = DIRECTX.WinSizeH;                                               \
                                        ovr_CreateMirrorTextureDX(session, DIRECTX.Device, &td, &mirrorTexture);    \

#define STAGE6_RenderMirror             ID3D11Resource* resource = nullptr;                                         \
                                        ovr_GetMirrorTextureBufferDX(session, mirrorTexture, IID_PPV_ARGS(&resource));  \
                                        DIRECTX.Context->CopyResource(DIRECTX.BackBuffer, resource);                \
                                        DIRECTX.SwapChain->Present(0, 0);

#define STAGE6_ReleaseMirror            ovr_DestroyMirrorTexture(session, mirrorTexture);


// Actual code
//============
{
    STAGE5_DeclareOculusTexture
    STAGE2_InitSDK
    STAGE1_InitEngine(L"Stage6", &luid);
    STAGE5_CreateEyeBuffers
    STAGE4_ConfigureVR
    STAGE6_CreateMirrorForMonitor   /*NEW*/
    STAGE1_InitModelsAndCamera
    STAGE1_MainLoopReadingInput
    {
        STAGE1_MoveCameraFromInputs
        STAGE4_GetEyePoses
        STAGE3_ForEachEye
        {
            STAGE5_SetEyeRenderTarget
            STAGE4_GetMatrices
            STAGE1_RenderModels
        }
        STAGE5_DistortAndPresent
        STAGE6_RenderMirror         /*NEW*/
    }
	STAGE6_ReleaseMirror            /*NEW*/
    STAGE5_ReleaseOculusTextures
    STAGE2_ReleaseSDK
    STAGE1_ReleaseEngine;
}