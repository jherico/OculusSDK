/************************************************************************************
Filename    :   Win32_RoomTiny_Main.cpp
Content     :   First-person view test application for Oculus Rift
Created     :   18th Dec 2014
Authors     :   Tom Heath
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
*************************************************************************************/
/// This sample shows a simple process to reduce the processing 
/// burden of your application by just rendering one eye each
/// frame, and using the old image for the other eye, albeit fixed
/// up by timewarp to appear rotationally correct.   Note that there
/// are downsides to this, the animating cube has double images, 
/// close objects, particularly the floor, do not stereoscopically match
/// as you move, your IPD will appear to expand and contract with 
/// sideways movement. And as you manually yaw around with cursors, it is 
/// not smooth.  However, we show how to mitigate this last item,
/// by folding the user's yaw into the timewarp calculation.
/// By default, the effect will be active.
/// Hold '1' to enable the alternate eye effect.
/// Hold '2' to deactivate folding user-yaw into timewarp

#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h"  // DirectX
#include "../Common/Win32_BasicVR.h"         // Basic VR

struct SimpleAlternateEye : BasicVR
{
    SimpleAlternateEye(HINSTANCE hinst) : BasicVR(hinst, L"Simple Alternate Eye") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);

	    while (HandleMessages())
	    {
		    ActionFromInput();

            // Get Eye poses, but into a temporary buffer,
            ovrPosef tempEyeRenderPose[2];
            Layer[0]->GetEyePoses(tempEyeRenderPose);

            // Decide which eye will be drawn this frame
            static int eyeThisFrame = 0;
            eyeThisFrame = 1 - eyeThisFrame;

            // We're going to note the player orientation
            // and store it if used to render an eye
            XMVECTOR playerOrientation = MainCam->Rot;
		    static XMVECTOR playerOrientationAtRender[2];

            for (int eye = 0; eye < 2; eye++)
            {
                // If required, update EyeRenderPose and corresponding eye buffer
                if ((DIRECTX.Key['1']) && (eye != eyeThisFrame)) continue;
            
                // Record the user yaw orientation for this eye image
                if (!DIRECTX.Key['2']) playerOrientationAtRender[eye] = playerOrientation;

                Layer[0]->EyeRenderPose[eye] = tempEyeRenderPose[eye];
                Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye);
            }
        
		    XMVECTOR diffQuat[2] = { XMQuaternionIdentity(), XMQuaternionIdentity() };
            if (!DIRECTX.Key['2']) diffQuat[0] = XMQuaternionMultiply(XMQuaternionInverse(playerOrientationAtRender[0]), playerOrientation);
		    if (!DIRECTX.Key['2']) diffQuat[1] = XMQuaternionMultiply(XMQuaternionInverse(playerOrientationAtRender[1]), playerOrientation);

            Layer[0]->PrepareLayerHeader(0, 0, diffQuat);
		    DistortAndPresent(1);
	    }
    }
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	SimpleAlternateEye app(hinst);
    return app.Run();
}
