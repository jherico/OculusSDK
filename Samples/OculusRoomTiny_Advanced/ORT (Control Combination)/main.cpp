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
/// A sample showing a combination of some of the other sample control
/// methods, to show how they can be combined, and to present an
/// interesting effect of them all together.  
/// This sample combines auto-yaw, jump from accelerometers, and 
/// tilt controlled movement.  Plus tap on the controller to fire a 
/// trivial bullet in the look direction.
/// Note, you can hold down SPACEBAR to temporarily disable tilt movement.

#define   OVR_D3D_VERSION 11
#include "..\Common\Win32_DirectXAppUtil.h"        // DirectX
#include "..\Common\Win32_BasicVR.h"         // Basic VR
#include "..\Common\Win32_ControlMethods.h"  // Control code

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
    BasicVR basicVR(hinst);
    basicVR.Layer[0] = new VRLayer(basicVR.HMD);

    // Main loop
    while (basicVR.HandleMessages())
    {
        // Take out manual yaw rotation (leaving button move for now)
        basicVR.ActionFromInput(1,false);
        ovrTrackingState trackingState = basicVR.Layer[0]->GetEyePoses();

        // Set various control methods into camera
        basicVR.MainCam->Pos  += FindVelocityFromTilt(&basicVR,basicVR.Layer[0],&trackingState);
        basicVR.MainCam->Pos.y = GetAccelJumpPosY(&basicVR,&trackingState);
        basicVR.MainCam->Rot   = GetAutoYawRotation(basicVR.Layer[0]);
 
        // If tap side of Rift, then fire a bullet
        bool singleTap = WasItTapped(trackingState.HeadPose.LinearAcceleration);

        static Vector3f bulletPos = Vector3f(0,0,0);
        static Vector3f bulletVel = Vector3f(0,0,0);
        if (singleTap)
        {
            Vector3f eye0 = (Vector3f) basicVR.Layer[0]->EyeRenderPose[0].Position;
            Vector3f eye1 = (Vector3f) basicVR.Layer[0]->EyeRenderPose[1].Position;
            Vector3f midEyePos = (eye0 + eye1)*0.5f;

            Matrix4f totalRot = basicVR.MainCam->Rot * Matrix4f(basicVR.Layer[0]->EyeRenderPose[0].Orientation);
            Vector3f posOfOrigin = basicVR.MainCam->Pos + basicVR.MainCam->Rot.Transform(midEyePos);

            Vector3f unitDirOfMainCamera = totalRot.Transform(Vector3f(0,0,-1));
            bulletPos = posOfOrigin + unitDirOfMainCamera*2.0f;
            bulletVel = unitDirOfMainCamera*0.3f;
        }

        // Move missile on, and set its position
        bulletPos += bulletVel;
        basicVR.pRoomScene->Models[1]->Pos = bulletPos; 


        for (int eye = 0; eye < 2; eye++)
        {
            basicVR.Layer[0]->RenderSceneToEyeBuffer(basicVR.MainCam, basicVR.pRoomScene, eye);
        }

        basicVR.Layer[0]->PrepareLayerHeader();
        basicVR.DistortAndPresent(1);
    }

    return (basicVR.Release(hinst));
}
