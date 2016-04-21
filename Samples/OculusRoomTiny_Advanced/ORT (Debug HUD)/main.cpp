/************************************************************************************
Content     :   First-person view test application for Oculus Rift
Created     :   243rd July 2015
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
/// This sample shows the built in debug HUD, with two modes.

/// The first mode, enabled by pressing '1', shows a centred-
/// crosshairs at infinity.  This is very useful particularly 
/// for confirming that 3D objects within the scene are offset
/// left and right for each eye - and not accidentally the same
/// which would yield a monoscopic view.  Also, to see that
/// the amount of offset for objects varies consistently
/// with their distance from the viewpoint.
/// Such offsets are most easily viewed in the mirror window.

/// The second mode, enabled by pressing '2', shows a 3D
/// quad planted in the 3D world.  It provides a very useful 
/// reference point of confirmed 'good-VR', to 
/// compare and refer-to in your scene.  
/// Additionally, the range of the debug quad is given onscreen, 
/// in order for applications to confirm their settings, and 
/// confirm their graphics are also at the correct perceived distance.

/// These can both be scaled, rotated, translated and recolored.
/// Simple examples of how to adjust are given by
/// holding keys '3','4','5' or '6'.
/// Press the '0' key to disable.


#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"        // Basic VR

struct DebugHUD : BasicVR
{
    DebugHUD(HINSTANCE hinst) : BasicVR(hinst, L"Debug HUD") {}

    void MainLoop()
    {
	    Layer[0] = new VRLayer(Session);

	    while (HandleMessages())
	    {
		    static float clock = 0;
            ++clock;

            // Toggle the debug HUD on and off, and which mode
            if (DIRECTX.Key['0']) ovr_SetInt(Session, OVR_DEBUG_HUD_STEREO_MODE, int(ovrDebugHudStereo_Off));
            if (DIRECTX.Key['1']) ovr_SetInt(Session, OVR_DEBUG_HUD_STEREO_MODE, int(ovrDebugHudStereo_CrosshairAtInfinity));
            if (DIRECTX.Key['2']) ovr_SetInt(Session, OVR_DEBUG_HUD_STEREO_MODE, int(ovrDebugHudStereo_Quad));

		    // Vary some of the attributes of the DebugHUD, when number keys are pressed.
		    float guideSize[2]      = {1, 1};
		    float guidePosition[3]  = {0, 0, -1.50f};
		    float guideRotation[3]  = {0, 0, 0};
		    float guideColorRGBA[4] = {1, 0.5f, 0, 1};
		    if (DIRECTX.Key['3']) guideSize[0]      = 1 + 0.5f * sin(0.02f * clock);   // Vary width
		    if (DIRECTX.Key['4']) guidePosition[0]  = 0.5f * sin(0.02f * clock);       // Vary X position
		    if (DIRECTX.Key['5']) guideRotation[0]  = 0.5f * sin(0.02f * clock);       // Vary yaw
		    if (DIRECTX.Key['6']) guideColorRGBA[1] = 0.5f + 0.5f * sin(0.1f * clock); // Vary green

		    // Write in the new attributes into the SDK
		    ovr_SetFloatArray(Session, OVR_DEBUG_HUD_STEREO_GUIDE_SIZE,         guideSize,      2);
		    ovr_SetFloatArray(Session, OVR_DEBUG_HUD_STEREO_GUIDE_POSITION,     guidePosition,  3);
		    ovr_SetFloatArray(Session, OVR_DEBUG_HUD_STEREO_GUIDE_YAWPITCHROLL, guideRotation,  3);
		    ovr_SetFloatArray(Session, OVR_DEBUG_HUD_STEREO_GUIDE_COLOR,        guideColorRGBA, 4);

		    ActionFromInput();
		    Layer[0]->GetEyePoses();

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
	DebugHUD app(hinst);
    return app.Run();
}
