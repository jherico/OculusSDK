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
/// This is a sample showing blind remapping of the strafe keys, for a user who is
/// inside an Session and thus not able to see the keyboard directly. 
/// At any time, a keyboard control mapping may be made by 
/// holding space bar and selecting the 'down' button, thus
/// mapping the left, right and up keys to the left,right and above respectively.

#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"  // Basic VR

struct RemappedKeys : BasicVR
{
    RemappedKeys(HINSTANCE hinst) : BasicVR(hinst, L"Remapped Keys") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);

	    while (HandleMessages())
	    {
            // Doing a custom update of position, hence zero speed.
            ActionFromInput(0);
		    Layer[0]->GetEyePoses();

            // If space is down, then we will deciding out 4-key strafe set,
            // with the user pressing the down key (presumably with their longest, middle finger)
            // with the keys north, west and east of that comprising the other strafe keys, up, left
            // right respectively.
            static char ahead, altAhead, back, left, right;
            if (DIRECTX.Key[' '])
            {
                if (DIRECTX.Key['W']) { back = 'W'; left = 'Q'; ahead = '2'; altAhead = '3'; right = 'E'; }
                if (DIRECTX.Key['E']) { back = 'E'; left = 'W'; ahead = '3'; altAhead = '4'; right = 'R'; }
                if (DIRECTX.Key['R']) { back = 'R'; left = 'E'; ahead = '4'; altAhead = '5'; right = 'T'; }
                if (DIRECTX.Key['T']) { back = 'T'; left = 'R'; ahead = '5'; altAhead = '6'; right = 'Y'; }
                if (DIRECTX.Key['Y']) { back = 'Y'; left = 'T'; ahead = '6'; altAhead = '7'; right = 'U'; }
                if (DIRECTX.Key['U']) { back = 'U'; left = 'Y'; ahead = '7'; altAhead = '8'; right = 'I'; }
                if (DIRECTX.Key['I']) { back = 'I'; left = 'U'; ahead = '8'; altAhead = '9'; right = 'O'; }
                if (DIRECTX.Key['O']) { back = 'O'; left = 'I'; ahead = '9'; altAhead = '0'; right = 'P'; }

                if (DIRECTX.Key['S']) { back = 'S'; left = 'A'; ahead = 'W'; altAhead = 'E'; right = 'D'; }
                if (DIRECTX.Key['D']) { back = 'D'; left = 'S'; ahead = 'E'; altAhead = 'R'; right = 'F'; }
                if (DIRECTX.Key['F']) { back = 'F'; left = 'D'; ahead = 'R'; altAhead = 'T'; right = 'G'; }
                if (DIRECTX.Key['G']) { back = 'G'; left = 'F'; ahead = 'R'; altAhead = 'T'; right = 'H'; }
                if (DIRECTX.Key['H']) { back = 'H'; left = 'G'; ahead = 'Y'; altAhead = 'U'; right = 'J'; }
                if (DIRECTX.Key['J']) { back = 'J'; left = 'H'; ahead = 'U'; altAhead = 'I'; right = 'K'; }
                if (DIRECTX.Key['K']) { back = 'K'; left = 'J'; ahead = 'I'; altAhead = 'O'; right = 'L'; }

                if (DIRECTX.Key['X']) { back = 'X'; left = 'Z'; ahead = 'S'; altAhead = 'D'; right = 'C'; }
                if (DIRECTX.Key['C']) { back = 'C'; left = 'X'; ahead = 'D'; altAhead = 'F'; right = 'V'; }
                if (DIRECTX.Key['V']) { back = 'V'; left = 'C'; ahead = 'F'; altAhead = 'G'; right = 'B'; }
                if (DIRECTX.Key['B']) { back = 'B'; left = 'V'; ahead = 'G'; altAhead = 'H'; right = 'N'; }
                if (DIRECTX.Key['N']) { back = 'N'; left = 'B'; ahead = 'H'; altAhead = 'J'; right = 'M'; }
            }

            // Lets interrogate the keys for movement
		    if ((DIRECTX.Key[altAhead])|| (DIRECTX.Key[ahead]))
			                        MainCam->Pos = XMVectorAdd(MainCam->Pos, XMVector3Rotate(XMVectorSet(0, 0, -0.05f, 0), MainCam->Rot));
		    if (DIRECTX.Key[back])  MainCam->Pos = XMVectorAdd(MainCam->Pos, XMVector3Rotate(XMVectorSet(0, 0, +0.05f, 0), MainCam->Rot)); 
		    if (DIRECTX.Key[right]) MainCam->Pos = XMVectorAdd(MainCam->Pos, XMVector3Rotate(XMVectorSet(+0.05f, 0, 0, 0), MainCam->Rot)); 
		    if (DIRECTX.Key[left])  MainCam->Pos = XMVectorAdd(MainCam->Pos, XMVector3Rotate(XMVectorSet(-0.05f, 0, 0, 0), MainCam->Rot)); 

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
	RemappedKeys app(hinst);
    return app.Run();
}
