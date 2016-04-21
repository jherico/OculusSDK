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
/// This sample shows a very simple technique to mitigate motion sickness
/// from user-defined yaw.  In essence, it allows yaw to move in discreet 
/// increments, thus breaking immersion sufficiently for the usual possible
/// nausea to be reduced/eliminated.  Its somewhat overkill, and it 
/// takes away the desirable immersion in the process by introducing such
/// a jarring event.

#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"  // Basic VR

struct QuantizedYaw : BasicVR
{
    QuantizedYaw(HINSTANCE hinst) : BasicVR(hinst, L"Quantized Yaw") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);

	    while (HandleMessages())
	    {
		    ActionFromInput(1, false);
		    Layer[0]->GetEyePoses();

            // We only allow yaw in certain jumps.
            // Note we disabled yaw above
            static float Yaw = 3.141f;
            if (DIRECTX.Key[VK_LEFT])  Yaw += 0.02f;
            if (DIRECTX.Key[VK_RIGHT]) Yaw -= 0.02f;

            // Now adjust if too far away
            static float VisibleYaw = Yaw;
            const float JUMP_IN_RADIANS = 20/*DEGREES*/ / 57.0f;
            if (VisibleYaw > (Yaw + 0.5f * JUMP_IN_RADIANS)) VisibleYaw -= JUMP_IN_RADIANS;
            if (VisibleYaw < (Yaw - 0.5f * JUMP_IN_RADIANS)) VisibleYaw += JUMP_IN_RADIANS;

            // Set into camera
            MainCam->Rot = XMQuaternionRotationRollPitchYaw(0, VisibleYaw, 0);

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
	QuantizedYaw app(hinst);
    return app.Run();
}
