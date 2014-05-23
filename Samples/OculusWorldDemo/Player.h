/************************************************************************************

Filename    :   Player.h
Content     :   Avatar movement and collision detection
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
#include "Kernel/OVR_KeyCodes.h"
#include "../CommonSrc/Render/Render_Device.h"

using namespace OVR;
using namespace OVR::Render;

//-------------------------------------------------------------------------------------
// The RHS coordinate system is assumed.  
const Vector3f	RightVector(1.0f, 0.0f, 0.0f);
const Vector3f	UpVector(0.0f, 1.0f, 0.0f);
const Vector3f	ForwardVector(0.0f, 0.0f, -1.0f); // -1 because HMD looks along -Z at identity orientation

const float		YawInitial	= 0.0f;
const float		Sensitivity	= 0.3f; // low sensitivity to ease people into it gently.
const float		MoveSpeed	= 3.0f; // m/s

// These are used for collision detection
const float		RailHeight	= 0.8f;


//-------------------------------------------------------------------------------------
// ***** Player

// Player class describes position and movement state of the player in the 3D world.
class Player
{
public:

	float				UserEyeHeight;

	// Where the avatar coordinate system (and body) is positioned and oriented in the virtual world
    // Modified by gamepad/mouse input
	Vector3f			BodyPos;
	Anglef				BodyYaw;

    // Where the player head is positioned and oriented in the real world
    Transformf          HeadPose;

    // Where the avatar head is positioned and oriented in the virtual world
    Vector3f            GetPosition();
    Quatf               GetOrientation(bool baseOnly = false);

    // Returns virtual world position based on a real world head pose.
    // Allows predicting eyes separately based on scanout time.
    Transformf          VirtualWorldTransformfromRealPose(const Transformf &sensorHeadPose);

    // Handle directional movement. Returns 'true' if movement was processed.
    bool                HandleMoveKey(OVR::KeyCode key, bool down);

    // Movement state; different bits may be set based on the state of keys.
    UByte               MoveForward;
    UByte               MoveBack;
    UByte               MoveLeft;
    UByte               MoveRight;
    Vector3f            GamepadMove, GamepadRotate;
    bool                bMotionRelativeToBody;

	Player();
	~Player();
	void HandleMovement(double dt, Array<Ptr<CollisionModel> >* collisionModels,
		                Array<Ptr<CollisionModel> >* groundCollisionModels, bool shiftDown);
};

#endif
