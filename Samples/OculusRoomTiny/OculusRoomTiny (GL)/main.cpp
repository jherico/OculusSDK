/*****************************************************************************

Filename    :   main.cpp
Content     :   Simple minimal VR demo
Created     :   December 1, 2014
Author      :   Tom Heath
Copyright   :   Copyright 2012 Oculus, Inc. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

/*****************************************************************************/
/// This sample has not yet been fully assimiliated into the framework
/// and also the GL support is not quite fully there yet, hence the VR
/// is not that great!

#include "../../OculusRoomTiny_Advanced/Common/Win32_GLAppUtil.h"
#include <Kernel/OVR_System.h>

// Include the Oculus SDK
#include "OVR_CAPI_GL.h"

using namespace OVR;

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
    OVR::System::Init();

     // Initialise rift
    if (ovr_Initialize(nullptr) != ovrSuccess) { MessageBoxA(NULL, "Unable to initialize libOVR.", "", MB_OK); return 0; }
    ovrHmd HMD;
    ovrResult result = ovrHmd_Create(0, &HMD);
    if (result != ovrSuccess)
    {
        result = ovrHmd_CreateDebug(ovrHmd_DK2, &HMD);
    }
 
    if (result != ovrSuccess) {    MessageBoxA(NULL,"Oculus Rift not detected.","", MB_OK); ovr_Shutdown(); return 0; }
    if (HMD->ProductName[0] == '\0') MessageBoxA(NULL,"Rift detected, display not enabled.","", MB_OK);

    // Setup Window and Graphics
    // Note: the mirror window can be any size, for this sample we use 1/2 the HMD resolution
    ovrSizei windowSize = { HMD->Resolution.w / 2, HMD->Resolution.h / 2 };
    if (!Platform.InitWindowAndDevice(hinst, Recti(Vector2i(0), windowSize), true, L"Oculus Room Tiny (GL)"))
        return 0;

    // Make eye render buffers
    TextureBuffer * eyeRenderTexture[2];
    DepthBuffer   * eyeDepthBuffer[2];
    for (int i=0; i<2; i++)
    {
        ovrSizei idealTextureSize = ovrHmd_GetFovTextureSize(HMD, (ovrEyeType)i, HMD->DefaultEyeFov[i], 1);
        eyeRenderTexture[i] = new TextureBuffer(HMD, true, true, idealTextureSize, 1, NULL, 1);
        eyeDepthBuffer[i]   = new DepthBuffer(eyeRenderTexture[i]->GetSize(), 0);
    }

    // Create mirror texture and an FBO used to copy mirror texture to back buffer
    ovrGLTexture* mirrorTexture;
    ovrHmd_CreateMirrorTextureGL(HMD, GL_RGBA, windowSize.w, windowSize.h, (ovrTexture**)&mirrorTexture);
    // Configure the mirror read buffer
    GLuint mirrorFBO = 0;
    glGenFramebuffers(1, &mirrorFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, mirrorFBO);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mirrorTexture->OGL.TexId, 0);
    glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    ovrEyeRenderDesc EyeRenderDesc[2]; 
    EyeRenderDesc[0] = ovrHmd_GetRenderDesc(HMD, ovrEye_Left, HMD->DefaultEyeFov[0]);
    EyeRenderDesc[1] = ovrHmd_GetRenderDesc(HMD, ovrEye_Right, HMD->DefaultEyeFov[1]);

    ovrHmd_SetEnabledCaps(HMD, ovrHmdCap_LowPersistence|ovrHmdCap_DynamicPrediction);

    // Start the sensor
    ovrHmd_ConfigureTracking(HMD, ovrTrackingCap_Orientation|ovrTrackingCap_MagYawCorrection|
                                  ovrTrackingCap_Position, 0);

    // Turn off vsync to let the compositor do its magic
    wglSwapIntervalEXT(0);

    // Make scene - can simplify further if needed
    Scene roomScene(false); 

    bool isVisible = true;

    // Main loop
    while (Platform.HandleMessages())
    {
        // Keyboard inputs to adjust player orientation
        static float Yaw(3.141592f);  
        if (Platform.Key[VK_LEFT])  Yaw += 0.02f;
        if (Platform.Key[VK_RIGHT]) Yaw -= 0.02f;

        // Keyboard inputs to adjust player position
        static Vector3f Pos2(0.0f,1.6f,-5.0f);
        if (Platform.Key['W']||Platform.Key[VK_UP])     Pos2+=Matrix4f::RotationY(Yaw).Transform(Vector3f(0,0,-0.05f));
        if (Platform.Key['S']||Platform.Key[VK_DOWN])   Pos2+=Matrix4f::RotationY(Yaw).Transform(Vector3f(0,0,+0.05f));
        if (Platform.Key['D'])                          Pos2+=Matrix4f::RotationY(Yaw).Transform(Vector3f(+0.05f,0,0));
        if (Platform.Key['A'])                          Pos2+=Matrix4f::RotationY(Yaw).Transform(Vector3f(-0.05f,0,0));
        Pos2.y = ovrHmd_GetFloat(HMD, OVR_KEY_EYE_HEIGHT, Pos2.y);

		// Animate the cube
        static float cubeClock = 0;
        roomScene.Models[0]->Pos = Vector3f(9 * sin(cubeClock), 3, 9 * cos(cubeClock += 0.015f));

        // Get eye poses, feeding in correct IPD offset
        ovrVector3f               ViewOffset[2] = { EyeRenderDesc[0].HmdToEyeViewOffset,
                                                    EyeRenderDesc[1].HmdToEyeViewOffset };
        ovrPosef                  EyeRenderPose[2];

        ovrFrameTiming   ftiming = ovrHmd_GetFrameTiming(HMD, 0);
        ovrTrackingState hmdState = ovrHmd_GetTrackingState(HMD, ftiming.DisplayMidpointSeconds);
        ovr_CalcEyePoses(hmdState.HeadPose.ThePose, ViewOffset, EyeRenderPose);

        if (isVisible)
        {
            for (int eye = 0; eye<2; eye++)
            {
                // Increment to use next texture, just before writing
                eyeRenderTexture[eye]->TextureSet->CurrentIndex = (eyeRenderTexture[eye]->TextureSet->CurrentIndex + 1) % eyeRenderTexture[eye]->TextureSet->TextureCount;

                // Switch to eye render target
                eyeRenderTexture[eye]->SetAndClearRenderSurface(eyeDepthBuffer[eye]);

                // Get view and projection matrices
                Matrix4f rollPitchYaw = Matrix4f::RotationY(Yaw);
                Matrix4f finalRollPitchYaw = rollPitchYaw * Matrix4f(EyeRenderPose[eye].Orientation);
                Vector3f finalUp = finalRollPitchYaw.Transform(Vector3f(0, 1, 0));
                Vector3f finalForward = finalRollPitchYaw.Transform(Vector3f(0, 0, -1));
                Vector3f shiftedEyePos = Pos2 + rollPitchYaw.Transform(EyeRenderPose[eye].Position);

                Matrix4f view = Matrix4f::LookAtRH(shiftedEyePos, shiftedEyePos + finalForward, finalUp);
                Matrix4f proj = ovrMatrix4f_Projection(HMD->DefaultEyeFov[eye], 0.2f, 1000.0f, ovrProjection_RightHanded);

            	// Render world
            	roomScene.Render(view,proj);

            	// Avoids an error when calling SetAndClearRenderSurface during next iteration.
            	// Without this, during the next while loop iteration SetAndClearRenderSurface
            	// would bind a framebuffer with an invalid COLOR_ATTACHMENT0 because the texture ID
            	// associated with COLOR_ATTACHMENT0 had been unlocked by calling wglDXUnlockObjectsNV.
            	eyeRenderTexture[eye]->UnsetRenderSurface();
            }
        }

        // Do distortion rendering, Present and flush/sync

        // Set up positional data.
        ovrViewScaleDesc viewScaleDesc;
        viewScaleDesc.HmdSpaceToWorldScaleInMeters = 1.0f;
        viewScaleDesc.HmdToEyeViewOffset[0] = ViewOffset[0];
        viewScaleDesc.HmdToEyeViewOffset[1] = ViewOffset[1];

        ovrLayerEyeFov ld;
        ld.Header.Type  = ovrLayerType_EyeFov;
        ld.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;   // Because OpenGL.

        for (int eye = 0; eye < 2; eye++)
        {
            ld.ColorTexture[eye] = eyeRenderTexture[eye]->TextureSet;
            ld.Viewport[eye]     = Recti(eyeRenderTexture[eye]->GetSize());
            ld.Fov[eye]          = HMD->DefaultEyeFov[eye];
            ld.RenderPose[eye]   = EyeRenderPose[eye];
        }

        ovrLayerHeader* layers = &ld.Header;
        ovrResult result = ovrHmd_SubmitFrame(HMD, 0, &viewScaleDesc, &layers, 1);
        isVisible = result == ovrSuccess;

        // Blit mirror texture to back buffer
        glBindFramebuffer(GL_READ_FRAMEBUFFER, mirrorFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        GLint w = mirrorTexture->OGL.Header.TextureSize.w;
        GLint h = mirrorTexture->OGL.Header.TextureSize.h;
        glBlitFramebuffer(0, h, w, 0,
                          0, 0, w, h,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        SwapBuffers(Platform.hDC);
    }

    glDeleteFramebuffers(1, &mirrorFBO);
    ovrHmd_DestroyMirrorTexture(HMD, (ovrTexture*)mirrorTexture);
    ovrHmd_DestroySwapTextureSet(HMD, eyeRenderTexture[0]->TextureSet);
    ovrHmd_DestroySwapTextureSet(HMD, eyeRenderTexture[1]->TextureSet);

    // Release
    ovrHmd_Destroy(HMD);
    ovr_Shutdown();
    Platform.ReleaseWindow(hinst);
    OVR::System::Destroy();

    return 0;
}



