
// STAGE 1
// ======
// We start with a basic, conventional, non-VR, PC application,
// running in a window, and built upon a basic DirectX11 'engine'.
// Move around with cursor keys.

#define STAGE1_InitEngine(x, pLuid)   DIRECTX.InitWindow(hinst, x); \
                                      DIRECTX.InitDevice(1280, 720, reinterpret_cast<LUID*>(pLuid));

#define STAGE1_InitModelsAndCamera    Scene roomScene(false); \
                                      Camera mainCam(XMVectorSet(0.0f, 1.6f, +5.0f, 0), XMQuaternionIdentity()); \
                                      bool isVisible = true; \
                                      UNREFERENCED_PARAMETER(isVisible);

#define STAGE1_MainLoopReadingInput   while (DIRECTX.HandleMessages())

#define STAGE1_MoveCameraFromInputs   static float Yaw = 0; \
                                      if (DIRECTX.Key[VK_LEFT])  mainCam.Rot = XMQuaternionRotationRollPitchYaw(0, Yaw += 0.02f, 0); \
                                      if (DIRECTX.Key[VK_RIGHT]) mainCam.Rot = XMQuaternionRotationRollPitchYaw(0, Yaw -= 0.02f, 0); \
                                      XMVECTOR forward = XMVector3Rotate(XMVectorSet(0, 0, -0.05f, 0), mainCam.Rot); \
                                      if (DIRECTX.Key[VK_UP])	 mainCam.Pos = XMVectorAdd(mainCam.Pos, forward); \
                                      if (DIRECTX.Key[VK_DOWN])  mainCam.Pos = XMVectorSubtract(mainCam.Pos, forward);

#define STAGE1_SetScreenRenderTarget  DIRECTX.SetAndClearRenderTarget(DIRECTX.BackBufferRT, DIRECTX.MainDepthBuffer); \
                                      DIRECTX.SetViewport(0,0,(float)DIRECTX.WinSizeW,(float)DIRECTX.WinSizeH);

#define STAGE1_GetMatrices            Camera finalCam(&mainCam.Pos, &mainCam.Rot); \
                                      XMMATRIX view = finalCam.GetViewMatrix(); \
                                      XMMATRIX proj = XMMatrixPerspectiveFovRH(1.0f, 1.0f, 0.2f, 1000.0f);

#define STAGE1_RenderModels           XMMATRIX viewProj = XMMatrixMultiply(view, proj); \
	                                  roomScene.Render(&viewProj,1,1,1,1,true); 

#define STAGE1_Present                DIRECTX.SwapChain->Present(true, 0);

#define STAGE1_ReleaseEngine          DIRECTX.ReleaseDevice(); \
                                      DIRECTX.CloseWindow();

// Actual code
//============
{
    STAGE1_InitEngine(L"Stage1", nullptr)   /*NEW*/
    STAGE1_InitModelsAndCamera              /*NEW*/
    STAGE1_MainLoopReadingInput             /*NEW*/
    {                                  
        STAGE1_MoveCameraFromInputs         /*NEW*/
        STAGE1_SetScreenRenderTarget        /*NEW*/
        STAGE1_GetMatrices                  /*NEW*/
        STAGE1_RenderModels                 /*NEW*/
        STAGE1_Present                      /*NEW*/
    }                                  
    STAGE1_ReleaseEngine                    /*NEW*/
}
