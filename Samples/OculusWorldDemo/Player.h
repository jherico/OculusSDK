/************************************************************************************

Filename    :   Player.h
Content     :   Player location and hit-testing logic
Created     :   October 4, 2012

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

#ifndef OVR_WorldDemo_Player_h
#define OVR_WorldDemo_Player_h

#include "OVR.h"
#include "../CommonSrc/Render/Render_Device.h"

using namespace OVR;
using namespace OVR::Render;

//-------------------------------------------------------------------------------------
// The RHS coordinate system is defines as follows (as seen in perspective view):
//  Y - Up
//  Z - Back
//  X - Right
const Vector3f	UpVector(0.0f, 1.0f, 0.0f);
const Vector3f	ForwardVector(0.0f, 0.0f, -1.0f);
const Vector3f	RightVector(1.0f, 0.0f, 0.0f);

// We start out looking in the positive Z (180 degree rotation).
const float		YawInitial	= 3.141592f;
const float		Sensitivity	= 1.0f;
const float		MoveSpeed	= 3.0f; // m/s

// These are used for collision detection
const float		RailHeight	= 0.8f;


//-------------------------------------------------------------------------------------
// ***** Player

// Player class describes position and movement state of the player in the 3D world.
class Player
{
public:
	// Position and look. The following apply:
    Vector3f            EyePos;
	float				EyeHeight;
    float               EyeYaw;         // Rotation around Y, CCW positive when looking at RHS (X,Z) plane.
    float               EyePitch;       // Pitch. If sensor is plugged in, only read from sensor.
    float               EyeRoll;        // Roll, only accessible from Sensor.
    float               LastSensorYaw;  // Stores previous Yaw value from to support computing delta.

	// Movement state; different bits may be set based on the state of keys.
    UByte               MoveForward;
    UByte               MoveBack;
    UByte               MoveLeft;
    UByte               MoveRight;
    Vector3f            GamepadMove, GamepadRotate;

	Player();
	~Player();
	void HandleCollision(double dt, Array<Ptr<CollisionModel> >* collisionModels,
		                 Array<Ptr<CollisionModel> >* groundCollisionModels, bool shiftDown);
};

#endif
