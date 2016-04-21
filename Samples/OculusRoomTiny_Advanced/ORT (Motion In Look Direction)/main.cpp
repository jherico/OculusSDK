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
/// A simple demo to show the extra code needed to have the player's movement
/// (from WASD and cursors) in the direction of where the player is looking (including Rift
/// orientations), not just in the direction of the player (independent of Rift orientation)
/// as exhibited in most of these samples.

#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"  // Basic VR

struct MotionInLookDirection : BasicVR
{
    MotionInLookDirection(HINSTANCE hinst) : BasicVR(hinst, L"Motion In Look Direction") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);

	    while (HandleMessages())
	    {
            // We pass in zero, to make no positional movement 
            // but keep rest of the motion intact.
		    ActionFromInput(0);
		    Layer[0]->GetEyePoses();

            // Find the orthogonal vectors resulting from combined rift and user yaw
            XMVECTOR totalRot = XMQuaternionMultiply(ConvertToXM(Layer[0]->EyeRenderPose[0].Orientation), MainCam->Rot);
            XMVECTOR forward = XMVector3Rotate(XMVectorSet(0, 0, -1, 0), totalRot);
            XMVECTOR right   = XMVector3Rotate(XMVectorSet(1, 0, 0, 0), totalRot);

            // Keyboard inputs to adjust player position, using these orthogonal vectors
            float speed = 0.05f;
            if (DIRECTX.Key['W'] || DIRECTX.Key[VK_UP])   MainCam->Pos = XMVectorAdd(MainCam->Pos, XMVectorScale(forward, +speed));
            if (DIRECTX.Key['S'] || DIRECTX.Key[VK_DOWN]) MainCam->Pos = XMVectorAdd(MainCam->Pos, XMVectorScale(forward, -speed));
            if (DIRECTX.Key['D'])                         MainCam->Pos = XMVectorAdd(MainCam->Pos, XMVectorScale(right,   +speed));
            if (DIRECTX.Key['A'])                         MainCam->Pos = XMVectorAdd(MainCam->Pos, XMVectorScale(right,   -speed));

		    for (int eye = 0; eye < 2; ++eye)
		    {
			    Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye);
		    }

		    Layer[0]->PrepareLayerHeader();
		    DistortAndPresent(1);
	    }
    }
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	MotionInLookDirection app(hinst);
    return app.Run();
}
