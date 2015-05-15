
// STAGE 1
// ======
// We start with a basic, conventional, non-VR, PC application,
// running in a window, and built upon a basic DirectX11 'engine'.
// Move around with cursor keys.

#define STAGE1_InitEngine             bool initialized = DIRECTX.InitWindowAndDevice(hinst, Recti(0,0,1280,720), L"ORT Engine Integration Stage 1");        \
                                      VALIDATE(initialized, "Unable to initialize window and D3D11 device.");

#define STAGE1_InitModelsAndCamera    Scene roomScene;                                                                       \
                                      Camera mainCam(Vector3f(0.0f,1.6f,-5.0f),Matrix4f::RotationY(3.141f));                 \
                                      bool isVisible = true;

#define STAGE1_MainLoopReadingInput   while (DIRECTX.HandleMessages())

#define STAGE1_MoveCameraFromInputs   static float Yaw = 3.141f;                                                             \
                                      if (DIRECTX.Key[VK_LEFT])  mainCam.Rot = Matrix4f::RotationY(Yaw+=0.02f);              \
                                      if (DIRECTX.Key[VK_RIGHT]) mainCam.Rot = Matrix4f::RotationY(Yaw-=0.02f);              \
                                      if (DIRECTX.Key[VK_UP])    mainCam.Pos+=mainCam.Rot.Transform(Vector3f(0,0,-0.05f));   \
                                      if (DIRECTX.Key[VK_DOWN])  mainCam.Pos+=mainCam.Rot.Transform(Vector3f(0,0,+0.05f));   \

#define STAGE1_SetScreenRenderTarget  DIRECTX.SetAndClearRenderTarget(DIRECTX.BackBufferRT, DIRECTX.MainDepthBuffer);        \
                                      DIRECTX.SetViewport(Recti(0,0,DIRECTX.WinSize.w,DIRECTX.WinSize.h));

#define STAGE1_GetMatrices            Camera finalCam(mainCam.Pos, mainCam.Rot);                                             \
                                      Matrix4f view = finalCam.GetViewMatrix();                                              \
                                      Matrix4f proj(Matrix4d::PerspectiveRH(1.0f, 1.0f, 0.2f, 1000.0f));

#define STAGE1_RenderModels           roomScene.Render(proj*view,1,1,1,1,true);

#define STAGE1_Present                DIRECTX.SwapChain->Present(true, 0); 

#define STAGE1_ReleaseEngine          DIRECTX.ReleaseWindow(hinst);                                                                       



// Actual code
//============
{
    STAGE1_InitEngine                 /*NEW*/
    STAGE1_InitModelsAndCamera        /*NEW*/
    STAGE1_MainLoopReadingInput       /*NEW*/
    {                                  
        STAGE1_MoveCameraFromInputs   /*NEW*/
        STAGE1_SetScreenRenderTarget  /*NEW*/
        STAGE1_GetMatrices            /*NEW*/
        STAGE1_RenderModels           /*NEW*/
        STAGE1_Present                /*NEW*/
    }                                  
    STAGE1_ReleaseEngine              /*NEW*/
}
