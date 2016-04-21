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
/// This is a sample to largely show what NOT to do.  Its tempting to 
/// attempt to resolve the judder issue of frames that miss frame-rate,
/// by rendering them as blank.  This sample shows that this doesn't work
/// terribly well!
/// Press any of keys '1' to '4' to see a few preset examples.

#define   OVR_D3D_VERSION 11
#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"  // Basic VR

struct BlankFrames : BasicVR
{
    BlankFrames(HINSTANCE hinst) : BasicVR(hinst, L"Blank Frames") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);

	    while (HandleMessages())
	    {
		    ActionFromInput();
		    Layer[0]->GetEyePoses();

            // Have a clock going
            static int clock = 0;
            ++clock;

		    for (int eye = 0; eye < 2; ++eye)
		    {
                // Press '1-4' to simulate if, instead of rendering frames, exhibit blank frames
                // in order to guarantee frame rate.   Not recommended at all, but useful to see,
                // just in case some might consider it a viable alternative to juddering frames.
                int timesToRenderScene = 1;
                if (   ((DIRECTX.Key['1']) && ((clock % ( 1 * 2)) == (eye *  1)))  // Every 1 frame
                    || ((DIRECTX.Key['2']) && ((clock % ( 2 * 2)) == (eye *  2)))  // Every 2 frames
                    || ((DIRECTX.Key['3']) && ((clock % (10 * 2)) == (eye * 10)))  // Every 10 frames
                    || ((DIRECTX.Key['4']) && ((clock % (50 * 2)) == (eye * 50)))) // Every 50 frames
                    timesToRenderScene = 0;

                Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye, 0, 0, timesToRenderScene);
		    }

		    Layer[0]->PrepareLayerHeader();
		    DistortAndPresent(1);
	    }
    }
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	BlankFrames app(hinst);
    return app.Run();
}
