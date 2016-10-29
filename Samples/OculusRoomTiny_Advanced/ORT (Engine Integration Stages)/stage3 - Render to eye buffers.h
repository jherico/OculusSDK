
// STAGE 3
// ======
// Now we create two render buffers, according to the SDK specifications
// and render our scene into both of those.

#define STAGE3_CreateEyeBuffers    Texture     * pEyeRenderTexture[2];                                                                         \
                                   DepthBuffer * pEyeDepthBuffer[2];                                                                           \
                                   ovrRecti      eyeRenderViewport[2];                                                                         \
                                   for (int eye = 0; eye < 2; eye++)                                                                           \
                                   {                                                                                                           \
                                       ovrSizei idealSize = ovr_GetFovTextureSize(session, (ovrEyeType)eye, HMDInfo.DefaultEyeFov[eye], 1.0f); \
                                       pEyeRenderTexture[eye] = new Texture(true, idealSize.w, idealSize.h);                                   \
                                       pEyeDepthBuffer[eye]   = new DepthBuffer(DIRECTX.Device, idealSize.w, idealSize.h);                     \
									   eyeRenderViewport[eye].Pos.x = 0;                                                                       \
									   eyeRenderViewport[eye].Pos.y = 0;                                                                       \
									   eyeRenderViewport[eye].Size = idealSize;                                                                \
                                   }                                                                                                           
                                                                                                                                               
#define STAGE3_ModelsToViewBuffers Model renderLeftEyeTexture(new Material(pEyeRenderTexture[0]),-0.9f,-0.8f,-0.1f,+0.8f);                     \
                                   Model renderRightEyeTexture(new Material(pEyeRenderTexture[1]),+0.1f,-0.8f,+0.9f,+0.8f);                    
                                                                                                                                               
#define STAGE3_ForEachEye          for (int eye=0; eye<2 && isVisible;eye++)                                                                   
                                                                                                                                               
#define STAGE3_SetEyeRenderTarget  DIRECTX.SetAndClearRenderTarget(pEyeRenderTexture[eye]->TexRtv, pEyeDepthBuffer[eye]);                      \
	                               DIRECTX.SetViewport((float)eyeRenderViewport[eye].Pos.x, (float)eyeRenderViewport[eye].Pos.y,               \
                                                       (float)eyeRenderViewport[eye].Size.w, (float)eyeRenderViewport[eye].Size.h);            
                                                                                                                                               
#define STAGE3_RenderEyeBuffers    renderLeftEyeTexture.Render (XMMatrixIdentity(),1,1,1,1,true);                                             \
	                               renderRightEyeTexture.Render(XMMatrixIdentity(), 1, 1, 1, 1, true);


// Actual code
//============
{
    STAGE2_InitSDK
    STAGE1_InitEngine(L"Stage3", &luid);
    STAGE3_CreateEyeBuffers             /*NEW*/
    STAGE3_ModelsToViewBuffers          /*NEW*/
    STAGE1_InitModelsAndCamera
    STAGE1_MainLoopReadingInput
    {
        STAGE1_MoveCameraFromInputs
        STAGE3_ForEachEye               /*NEW*/
        {
            STAGE3_SetEyeRenderTarget   /*NEW*/
            STAGE1_GetMatrices
            STAGE1_RenderModels
        }
        STAGE1_SetScreenRenderTarget
        STAGE3_RenderEyeBuffers         /*NEW*/
        STAGE1_Present
    }
    STAGE2_ReleaseSDK
    STAGE1_ReleaseEngine;
}