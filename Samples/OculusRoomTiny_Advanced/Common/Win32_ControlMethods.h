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

//-------------------------------------------
XMFLOAT3 GetEulerAngles(XMVECTOR q)
{
	XMFLOAT3 euler;
	XMFLOAT3 forward; XMStoreFloat3(&forward, XMVector3Rotate(XMVectorSet(0, 0, 1, 0), q));
	euler.y = atan2(forward.x , forward.z);
	float horizLength = sqrt(forward.x*forward.x + forward.z*forward.z);
	euler.x = atan2(forward.y, horizLength);
	XMFLOAT3 right; XMStoreFloat3(&right, XMVector3Rotate(XMVectorSet(1, 0, 0, 0), q));
	horizLength = sqrt(right.x*right.x + right.z*right.z);
	euler.z = atan2(right.y, horizLength);
	//Util.Output("Pitch = %0.0f, Yaw = %0.0f, Roll = %0.0f\n", XMConvertToDegrees(euler.x), XMConvertToDegrees(euler.y), XMConvertToDegrees(euler.z));
	return(euler);
}

//--------------------------------------------
XMVECTOR GetAutoYawRotation(VRLayer * vrLayer)
{
    // Increments in yaw are proportional to Rift yaw
    static float Yaw = 3.141f;
    XMVECTOR orientQuat= ConvertToXM(vrLayer->EyeRenderPose[0].Orientation);
	float yaw = GetEulerAngles(orientQuat).y;
    Yaw += yaw*0.02f;
    return (XMQuaternionRotationRollPitchYaw(0,Yaw,0));
}

//----------------------------------------------------------------------
bool WasItTapped(ovrVector3f linearAcc)
{
    const float thresholdForTap = 10.0f; 
    const float thresholdForReset = 2.0f;
	float magOfAccel = XMVectorGetX(XMVector3Length(ConvertToXM(linearAcc)));
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
    static float yPos = ovr_GetFloat(pBasicVR->Session, OVR_KEY_EYE_HEIGHT, 0);
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
    if (yPos < 1.6f) ///ovr_GetFloat(pBasicVR->Session, OVR_KEY_EYE_HEIGHT, 0))  No longer works
    {
		yPos = 1.6f;/// ovr_GetFloat(pBasicVR->Session, OVR_KEY_EYE_HEIGHT, 0);
        yVel = 0;
    }

    return(yPos);
}

//-----------------------------------------------------------------------
XMVECTOR FindVelocityFromTilt(BasicVR * pBasicVR, VRLayer * vrLayer, ovrTrackingState * pTrackingState)
{
    // Find the orthogonal vectors resulting from combined rift and user yaw
	XMVECTOR orientQuat = ConvertToXM(vrLayer->EyeRenderPose[0].Orientation);
	XMFLOAT3 eulerFromRift = GetEulerAngles(orientQuat);

	XMVECTOR totalHorizRot = XMQuaternionMultiply(pBasicVR->MainCam->Rot,XMQuaternionRotationRollPitchYaw(0,eulerFromRift.y,0));
    XMVECTOR unitForwardVector = XMVector3Rotate(XMVectorSet(0,0,-1,0),totalHorizRot);
    XMVECTOR unitRightVector   = XMVector3Rotate(XMVectorSet(1,0,0,0),totalHorizRot);

    // Now feed into a static velocity.
    static XMVECTOR vel = XMVectorSet(0,0,0,0); 

    // Hold down space if you want to look around, instead of move
    if (!DIRECTX.Key[' '])
    {
        const float tiltResponse = 0.0075f; 
        vel = XMVectorAdd(vel, XMVectorScale(unitForwardVector,tiltResponse * tan( eulerFromRift.x ))); // Pitch
        vel = XMVectorSubtract(vel, XMVectorScale(unitRightVector,  tiltResponse * tan( eulerFromRift.z ))); // Roll
    }

    // We always have damping, to cap top speeds
    // and to damp to zero when space is released.
    vel = XMVectorScale(vel,0.98f);

    // Limit velocity
    const float maxSpeed = 0.1f;
    float speed = XMVectorGetX(XMVector3Length(vel));
	if (speed > maxSpeed) vel = XMVectorScale(vel, maxSpeed / speed);;

    return(vel);
}


#endif // OVR_Win32_ControlMethods_h
