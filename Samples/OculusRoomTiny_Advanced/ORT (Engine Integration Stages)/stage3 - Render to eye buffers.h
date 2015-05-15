
// STAGE 3
// ======
// Now we create two render buffers, according to the SDK specifications
// and render our scene into both of those.

#define STAGE3_CreateEyeBuffers    Texture     * pEyeRenderTexture[2];                                                                  \
                                   DepthBuffer * pEyeDepthBuffer[2];                                                                    \
                                   ovrRecti      eyeRenderViewport[2];                                                                  \
                                   for (int eye = 0; eye < 2; eye++)                                                                    \
                                   {                                                                                                    \
                                       Sizei idealSize = ovrHmd_GetFovTextureSize(HMD, (ovrEyeType)eye, HMD->DefaultEyeFov[eye], 1.0f); \
                                       pEyeRenderTexture[eye] = new Texture(true, idealSize);                                           \
                                       pEyeDepthBuffer[eye]   = new DepthBuffer(DIRECTX.Device, idealSize);                             \
                                       eyeRenderViewport[eye].Pos  = Vector2i(0, 0);                                                    \
                                       eyeRenderViewport[eye].Size = idealSize;                                                         \
                                   }

#define STAGE3_ModelsToViewBuffers Model renderLeftEyeTexture(pEyeRenderTexture[0],-0.9f,-0.8f,-0.1f,+0.8f);                            \
                                   Model renderRightEyeTexture(pEyeRenderTexture[1],+0.1f,-0.8f,+0.9f,+0.8f);  

#define STAGE3_ForEachEye          for (int eye=0; eye<2 && isVisible;eye++)

#define STAGE3_SetEyeRenderTarget  DIRECTX.SetAndClearRenderTarget(pEyeRenderTexture[eye]->TexRtv, pEyeDepthBuffer[eye]);               \
                                   DIRECTX.SetViewport(Recti(eyeRenderViewport[eye]));

#define STAGE3_RenderEyeBuffers    renderLeftEyeTexture.Render (Matrix4f(),1,1,1,1,true);                                                           \
                                   renderRightEyeTexture.Render(Matrix4f(),1,1,1,1,true);


// Actual code
//============
{
    STAGE1_InitEngine;
    STAGE2_InitSDK
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