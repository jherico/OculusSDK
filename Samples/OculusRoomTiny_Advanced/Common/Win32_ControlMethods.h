/************************************************************************************
Filename    :   Win32_ControlMethods.h
Content     :   Shared functionality for the VR control methods
Created     :   October 20th, 2014
Author      :   Tom Heath
Copyright   :   Copyright 2014 Oculus, Inc. All Rights reserved.
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
#ifndef OVR_Win32_ControlMethods_h
#define OVR_Win32_ControlMethods_h

//--------------------------------------------
Matrix4f GetAutoYawRotation(VRLayer * vrLayer)
{
    // Increments in yaw are proportional to Rift yaw
    static float Yaw = 3.141f;
    float x,y,z;
    Quatf q = vrLayer->EyeRenderPose[0].Orientation;
    q.GetEulerAngles<Axis_X, Axis_Y, Axis_Z>(&x, &y, &z);
    Yaw += y*0.02f;
    return (Matrix4f::RotationY(Yaw));
}

//----------------------------------------------------------------------
bool WasItTapped(Vector3f linearAcc)
{
    const float thresholdForTap = 10.0f; 
    const float thresholdForReset = 2.0f;
    float magOfAccel = linearAcc.Length();
    static bool readyForNewSingleTap = false;
    if (magOfAccel < thresholdForReset)
        readyForNewSingleTap = true;
    if ((readyForNewSingleTap) && (magOfAccel > thresholdForTap))
    {
        readyForNewSingleTap = false;
        return(true);
    }
    return(false);
}

//---------------------------------------------------
float GetAccelJumpPosY(BasicVR * pBasicVR, ovrTrackingState * pTrackingState)
{
    // Change y position - note, we need to keep a version here
    // As the one in ActionFromInput will keep resetting us to height of the character
    static float yPos = ovrHmd_GetFloat(pBasicVR->HMD, OVR_KEY_EYE_HEIGHT, 0);
    static float yVel = 0;

    // Jump into air manually with '1' key
    if (DIRECTX.Key['1']) yVel += 0.01f;

    // Jump from head movement - note, its slightly counter intuitive - instead of responding to the 'up' accel,
    // it lets that one get absorbed by the ground, and acts on the deceleration from the upward movement.
	// Alternative : You could also try yVel += - min(0,jumpResponse*pTrackingState->HeadPose.LinearAcceleration.y);
	//               in the below code. This has facets that are interesting, in allowing multiple jumps,
	//               and eliminates any downward midair movement - but leaves the motion unbounded.
    const float jumpResponse = 0.0015f;
    yVel += - jumpResponse*pTrackingState->HeadPose.LinearAcceleration.y; 

    // Add pseudo gravity
    yVel += -0.002f;

    // Increment position
    yPos += yVel;

    // Hit floor;
    if (yPos < ovrHmd_GetFloat(pBasicVR->HMD, OVR_KEY_EYE_HEIGHT, 0))
    {
        yPos = ovrHmd_GetFloat(pBasicVR->HMD, OVR_KEY_EYE_HEIGHT, 0);
        yVel = 0;
    }

    return(yPos);
}

//-----------------------------------------------------------------------
Vector3f FindVelocityFromTilt(BasicVR * pBasicVR, VRLayer * vrLayer, ovrTrackingState * pTrackingState)
    {
    // Find the orthogonal vectors resulting from combined rift and user yaw
    Vector3f eulerFromRift;
    Quatf q = vrLayer->EyeRenderPose[0].Orientation;
    q.GetEulerAngles<Axis_X, Axis_Y, Axis_Z>(&eulerFromRift.x, &eulerFromRift.y, &eulerFromRift.z);
    Matrix4f totalHorizRot = pBasicVR->MainCam->Rot * Matrix4f::RotationY(eulerFromRift.y);
    Vector3f unitForwardVector = totalHorizRot.Transform(Vector3f(0,0,-1));
    Vector3f unitRightVector   = totalHorizRot.Transform(Vector3f(1,0,0));

    // Now feed into a static velocity.
    static Vector3f vel = Vector3f(0,0,0); 

    // Hold down space if you want to look around, instead of move
    if (!DIRECTX.Key[' '])
    {
        const float tiltResponse = 0.0075f; 
        vel -= unitForwardVector * tiltResponse * tan( eulerFromRift.x ); // Pitch
        vel -= unitRightVector   * tiltResponse * tan( eulerFromRift.z ); // Roll
    }

    // We always have damping, to cap top speeds
    // and to damp to zero when space is released.
    vel *= 0.98f;

    // Limit velocity
    const float maxSpeed = 0.1f;
    float speed = vel.Length();
    if (speed > maxSpeed) vel = (vel * maxSpeed)/speed;
    
    return(vel);
    }

#endif // OVR_Win32_ControlMethods_h
