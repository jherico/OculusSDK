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

#ifndef OVR_Player_h
#define OVR_Player_h

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_Allocator.h"
#include "Kernel/OVR_RefCount.h"
#include "Kernel/OVR_System.h"
#include "Kernel/OVR_Nullptr.h"
#include "Kernel/OVR_Timer.h"
#include "Kernel/OVR_SysFile.h"
#include "Extras/OVR_Math.h"

#include "Kernel/OVR_KeyCodes.h"
#include "../CommonSrc/Render/Render_Device.h"

#include <vector>
#include <string>

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
    // Where the avatar head is positioned and oriented in the virtual world
    Vector3f GetHeadPosition(ovrTrackingOrigin trackingOrigin);
    Quatf   GetOrientation(bool baseOnly = false);

    // Returns virtual world position based on a real world head pose.
    // Allows predicting eyes separately based on scanout time.
    Posef   VirtualWorldTransformfromRealPose(const Posef &sensorHeadPose, ovrTrackingOrigin trackingOrigin);

    // Handle directional movement. Returns 'true' if movement was processed.
    bool    HandleMoveKey(OVR::KeyCode key, bool down);

    void    HandleMovement(double dt, std::vector<Ptr<CollisionModel> >* collisionModels,
                                      std::vector<Ptr<CollisionModel> >* groundCollisionModels, bool shiftDown);

    // Accounts for ComfortTurn setting.
    Anglef  GetApparentBodyYaw();

    float GetFloorDistanceFromTrackingOrigin(ovrTrackingOrigin trackingOrigin)
    {
        float floorDistance = (trackingOrigin == ovrTrackingOrigin_EyeLevel) ?
                                UserStandingEyeHeight : 
                                UserStandingEyeHeight - ProfileStandingEyeHeight;
        return floorDistance * HeightScale;
    }

    float   GetHeadDistanceFromTrackingOrigin(ovrTrackingOrigin trackingOrigin)
    {
        float headDistance = (trackingOrigin == ovrTrackingOrigin_EyeLevel) ?
                                UserStandingEyeHeight - ProfileStandingEyeHeight :
                                UserStandingEyeHeight;
        return headDistance * HeightScale;
    }

    void    SetBodyPos(Vector3f newBodyPos, bool addUserStandingEyeHeight)
    {
        BodyPos = newBodyPos;
        if (addUserStandingEyeHeight)
        {
            BodyPos.y += GetFloorDistanceFromTrackingOrigin(ovrTrackingOrigin_EyeLevel);
        }
        BodyPoseFloorLevel = BodyPos;
        // floor level height is *always* ProfileStandingEyeHeight from BodyPos.y
        BodyPoseFloorLevel.y = BodyPos.y - (ProfileStandingEyeHeight * HeightScale);
    }

    float GetScaledProfileEyeHeight()
    {
        return ProfileStandingEyeHeight * HeightScale;
    }

    float GetScaledUserEyeHeight()
    {
        return UserStandingEyeHeight * HeightScale;
    }

    void SetUserStandingEyeHeight(float eyeHeight, float heightScale)
    {
        // First subtract old eye height and scale
        BodyPos.y -= GetFloorDistanceFromTrackingOrigin(ovrTrackingOrigin_EyeLevel);

        HeightScale = heightScale;
        UserStandingEyeHeight = eyeHeight;

        // Add new eye height
        SetBodyPos(BodyPos, true);
    }

    Vector3f GetBodyPos(ovrTrackingOrigin trackingOrigin)
    {
        return  (trackingOrigin == ovrTrackingOrigin_EyeLevel) ? BodyPos : BodyPoseFloorLevel;
    }

    Player();
    ~Player();

public:
    // User parameters
    float       ProfileStandingEyeHeight;
    float       UserStandingEyeHeight;

    // Where the avatar coordinate system (and body) is positioned and oriented in the virtual world
    // Modified by gamepad/mouse input
    Anglef      BodyYaw;    // Probably call GetApparentBodyYaw() instead.

    // Where the player head is positioned and oriented in the real world
    Posef       HeadPose;

    // Movement state; different bits may be set based on the state of keys.
    uint8_t     MoveForward;
    uint8_t     MoveBack;
    uint8_t     MoveLeft;
    uint8_t     MoveRight;
    Vector3f    GamepadMove, GamepadRotate;
    bool        bMotionRelativeToBody;
    float       ComfortTurnSnap;

private:
    // Where the avatar coordinate system (and body) is positioned and oriented in the virtual world
    // Modified by gamepad/mouse input
    Vector3f    BodyPos;    // this one is used for both collision testing and rendering from eye-level origin
    Vector3f    BodyPoseFloorLevel;

    float       HeightScale;
};

#endif // OVR_Player_h
