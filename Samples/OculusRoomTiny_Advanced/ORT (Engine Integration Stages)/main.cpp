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
/// This sample is a suggested procedure to follow for integrating
/// VR into your engine or application.  Starting from stage 1, which is
/// a non-VR sample application, in a sample engine, you can then follow
/// through the stages, in order, to 6.  At each stage, there are clearly
/// defined goals to achieve in a bite-sized way, so you can build up
/// in a clear, debugable way. 
/// Press ESCAPE to go sequentially from one stage to the next.
/// (Lately, it seems you have to press it pretty promptly, or the code doesn't
/// run once you reach stage 4)
/// The code is laid out as #defines, rather than functions, to avoid
/// the additional overhead of passing variables around which might
/// clutter the code, in order that you can most clearly see the code
/// added at each stage.

// Include DirectX 11 'engine'
#include "../Common/Win32_DirectXAppUtil.h"

// Include the Oculus SDK
#define   OVR_D3D_VERSION 11
#include "OVR_CAPI_D3D.h"

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
    // Press ESCAPE to go sequentially from one stage to the next.
	// Might need to do it quite promptly, to avoid timeouts.
    #include "stage1 - Start with nonVR application.h"
    #include "stage2 - Add LibOVR.h"
    #include "stage3 - Render to eye buffers.h"
    #include "stage4 - Hook into sensors.h"
    #include "stage5 - Output to headset.h"
    #include "stage6 - Add mirror.h"
    return(0);
}
