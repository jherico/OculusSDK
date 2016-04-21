
// STAGE 5
// =======
// This is a big stage.   We create special Oculus textures, for feeding into the SDK.
// We render to these, and then we pass these into the 'layer' system of the SDK.
// For this simple example, there is just a simple basic layer.
// This carries out the distortion, and outputs the results to the Rift.

#define STAGE5_DeclareOculusTexture  struct OculusTexture                                                                                 \
                                     {                                                                                                    \
                                         ovrTextureSwapChain  TextureChain;                                                               \
                                         ID3D11RenderTargetView * TexRtv[3];                                                              \
										 OculusTexture(ovrSession session, int sizeW, int sizeH)                                          \
                                         {                                                                                                \
                                             ovrTextureSwapChainDesc dsDesc = {};                                                         \
                                             dsDesc.Width = sizeW;                                                                        \
                                             dsDesc.Height = sizeH;                                                                       \
                                             dsDesc.MipLevels = 1;                                                                        \
                                             dsDesc.ArraySize = 1;                                                                        \
                                             dsDesc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;                                              \
                                             dsDesc.SampleCount = 1;                                                                      \
                                             dsDesc.MiscFlags = ovrTextureMisc_DX_Typeless;                                               \
                                             dsDesc.BindFlags = ovrTextureBind_DX_RenderTarget;                                           \
											 ovr_CreateTextureSwapChainDX(session, DIRECTX.Device, &dsDesc, &TextureChain);               \
                                             int count = 0;                                                                               \
                                             ovr_GetTextureSwapChainLength(session, TextureChain, &count);                                \
                                             for (int i = 0; i < count; ++i)                                                              \
                                             {                                                                                            \
                                                 ID3D11Texture2D* tex = nullptr;                                                          \
											     ovr_GetTextureSwapChainBufferDX(session, TextureChain, i, IID_PPV_ARGS(&tex));           \
												 D3D11_RENDER_TARGET_VIEW_DESC rtvd = {};												  \
												 rtvd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;												  \
												 rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;									  \
												 DIRECTX.Device->CreateRenderTargetView(tex, &rtvd, &TexRtv[i]);		                  \
                                                 tex->Release();                                                                          \
											 }                                                                                            \
                                         }                                                                                                \
                                         ID3D11RenderTargetView* GetRTV(ovrSession session)                                               \
                                         {                                                                                                \
                                             int index = 0;                                                                               \
                                             ovr_GetTextureSwapChainCurrentIndex(session, TextureChain, &index);                          \
                                             return TexRtv[index];                                                                        \
                                         }                                                                                                \
                                         void Commit(ovrSession session)                                                                  \
                                         {                                                                                                \
                                             ovr_CommitTextureSwapChain(session, TextureChain);                                           \
                                         }                                                                                                \
                                         void Release(ovrSession session)                                                                 \
                                         {                                                                                                \
                                             ovr_DestroyTextureSwapChain(session, TextureChain);                                          \
                                         }                                                                                                \
                                     };



#define STAGE5_CreateEyeBuffers      OculusTexture  * pEyeRenderTexture[2];                                                                         \
                                     DepthBuffer    * pEyeDepthBuffer[2];                                                                           \
                                     ovrRecti         eyeRenderViewport[2];                                                                         \
                                     for (int eye = 0; eye < 2; eye++)                                                                              \
                                     {                                                                                                              \
                                         ovrSizei idealSize = ovr_GetFovTextureSize(session, (ovrEyeType)eye, HMDInfo.DefaultEyeFov[eye], 1.0f);    \
                                         pEyeRenderTexture[eye] = new OculusTexture(session, idealSize.w, idealSize.h);                             \
                                         pEyeDepthBuffer[eye] = new DepthBuffer(DIRECTX.Device, idealSize.w, idealSize.h);                          \
										 eyeRenderViewport[eye].Pos.x = 0;                                                                          \
										 eyeRenderViewport[eye].Pos.y = 0;                                                                          \
										 eyeRenderViewport[eye].Size = idealSize;                                                                   \
                                     }


#define STAGE5_SetEyeRenderTarget    DIRECTX.SetAndClearRenderTarget(pEyeRenderTexture[eye]->GetRTV(session), pEyeDepthBuffer[eye]);       \
									 DIRECTX.SetViewport((float)eyeRenderViewport[eye].Pos.x, (float)eyeRenderViewport[eye].Pos.y,         \
									                     (float)eyeRenderViewport[eye].Size.w, (float)eyeRenderViewport[eye].Size.h);

#define STAGE5_DistortAndPresent     ovrViewScaleDesc viewScaleDesc;                                                                       \
                                     viewScaleDesc.HmdSpaceToWorldScaleInMeters = 1.0f;                                                    \
                                     viewScaleDesc.HmdToEyeOffset[0] = HmdToEyeOffset[0];                                                  \
                                     viewScaleDesc.HmdToEyeOffset[1] = HmdToEyeOffset[1];                                                  \
                                     ovrLayerEyeFov ld;                                                                                    \
                                     ld.Header.Type = ovrLayerType_EyeFov;                                                                 \
                                     ld.Header.Flags = 0;                                                                                  \
                                     for (int eye = 0; eye < 2; eye++)                                                                     \
                                     {                                                                                                     \
                                         pEyeRenderTexture[eye]->Commit(session);                                                          \
                                         ld.ColorTexture[eye] = pEyeRenderTexture[eye]->TextureChain;                                      \
                                         ld.Viewport[eye] = eyeRenderViewport[eye];                                                        \
                                         ld.Fov[eye] = HMDInfo.DefaultEyeFov[eye];                                                         \
                                         ld.RenderPose[eye] = EyeRenderPose[eye];                                                          \
                                     }                                                                                                     \
                                     ovrLayerHeader* layers = &ld.Header;                                                                  \
                                     isVisible = ovr_SubmitFrame(session, 0, &viewScaleDesc, &layers, 1) == ovrSuccess;                                           
                                                                                                                                       
#define STAGE5_ReleaseOculusTextures pEyeRenderTexture[0]->Release(session);                                                                   \
                                     pEyeRenderTexture[1]->Release(session);                                                               



// Actual code
//============
{
    STAGE5_DeclareOculusTexture         /*NEW*/
    STAGE2_InitSDK
    STAGE1_InitEngine(L"Stage5", &luid);
    STAGE5_CreateEyeBuffers            /*REPLACEMENT*/
    STAGE4_ConfigureVR
    STAGE1_InitModelsAndCamera
    STAGE1_MainLoopReadingInput
    {
        STAGE1_MoveCameraFromInputs
        STAGE4_GetEyePoses
        STAGE3_ForEachEye
        {
            STAGE5_SetEyeRenderTarget   /*REPLACEMENT*/
            STAGE4_GetMatrices
            STAGE1_RenderModels
        }
        STAGE5_DistortAndPresent        /*REPLACEMENT*/
    }
    STAGE5_ReleaseOculusTextures        /*NEW*/
    STAGE2_ReleaseSDK
    STAGE1_ReleaseEngine;
}
