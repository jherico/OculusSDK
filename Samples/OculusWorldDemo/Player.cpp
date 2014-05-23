/************************************************************************************

Filename    :   Player.cpp
Content     :   Player location and hit-testing logic source
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

#include "Player.h"
#include <Kernel/OVR_Alg.h>

Player::Player()
    : UserEyeHeight(1.76f - 0.15f),        // 1.76 meters height (ave US male, Wikipedia), less 15 centimeters (TomF's top-of-head-to-eye distance).
    BodyPos(7.7f, 1.76f - 0.15f, -1.0f),
    BodyYaw(YawInitial)
{
	MoveForward = MoveBack = MoveLeft = MoveRight = 0;
    GamepadMove = Vector3f(0);
    GamepadRotate = Vector3f(0);
}

Player::~Player()
{
}

Vector3f Player::GetPosition()
{
    return BodyPos + Quatf(Vector3f(0,1,0), BodyYaw.Get()).Rotate(HeadPose.Translation);
}

Quatf Player::GetOrientation(bool baseOnly)
{
    Quatf baseQ = Quatf(Vector3f(0,1,0), BodyYaw.Get());
    return baseOnly ? baseQ : baseQ * HeadPose.Rotation;
}

Transformf Player::VirtualWorldTransformfromRealPose(const Transformf &sensorHeadPose)
{
    Quatf baseQ = Quatf(Vector3f(0,1,0), BodyYaw.Get());

    return Transformf(baseQ * sensorHeadPose.Rotation,
                 BodyPos + baseQ.Rotate(sensorHeadPose.Translation));
}


void Player::HandleMovement(double dt, Array<Ptr<CollisionModel> >* collisionModels,
	                        Array<Ptr<CollisionModel> >* groundCollisionModels, bool shiftDown)
{
    // Handle keyboard movement.
    // This translates BasePos based on the orientation and keys pressed.
    // Note that Pitch and Roll do not affect movement (they only affect view).
    Vector3f controllerMove;
    if(MoveForward || MoveBack || MoveLeft || MoveRight)
    {
        if (MoveForward)
        {
            controllerMove += ForwardVector;
        }
        else if (MoveBack)
        {
            controllerMove -= ForwardVector;
        }

        if (MoveRight)
        {
            controllerMove += RightVector;
        }
        else if (MoveLeft)
        {
            controllerMove -= RightVector;
        }
    }
    else if (GamepadMove.LengthSq() > 0)
    {
        controllerMove = GamepadMove;
    }
    controllerMove = GetOrientation(bMotionRelativeToBody).Rotate(controllerMove);    
    controllerMove.y = 0; // Project to the horizontal plane
    if (controllerMove.LengthSq() > 0)
    {
        // Normalize vector so we don't move faster diagonally.
        controllerMove.Normalize();
        controllerMove *= OVR::Alg::Min<float>(MoveSpeed * (float)dt * (shiftDown ? 3.0f : 1.0f), 1.0f);
    }

    // Compute total move direction vector and move length
    Vector3f orientationVector = controllerMove;
    float moveLength = orientationVector.Length();
    if (moveLength > 0)
        orientationVector.Normalize();
        
    float   checkLengthForward = moveLength;
    Planef  collisionPlaneForward;
    bool    gotCollision = false;

    for(unsigned int i = 0; i < collisionModels->GetSize(); ++i)
    {
        // Checks for collisions at model base level, which should prevent us from
		// slipping under walls
        if (collisionModels->At(i)->TestRay(BodyPos, orientationVector, checkLengthForward,
				                            &collisionPlaneForward))
        {
            gotCollision = true;
            break;
        }
    }

    if (gotCollision)
    {
        // Project orientationVector onto the plane
        Vector3f slideVector = orientationVector - collisionPlaneForward.N
			* (orientationVector.Dot(collisionPlaneForward.N));

        // Make sure we aren't in a corner
        for(unsigned int j = 0; j < collisionModels->GetSize(); ++j)
        {
            if (collisionModels->At(j)->TestPoint(BodyPos - Vector3f(0.0f, RailHeight, 0.0f) +
					                                (slideVector * (moveLength))) )
            {
                moveLength = 0;
                break;
            }
        }
        if (moveLength != 0)
        {
            orientationVector = slideVector;
        }
    }
    // Checks for collisions at foot level, which allows us to follow terrain
    orientationVector *= moveLength;
    BodyPos += orientationVector;

    Planef collisionPlaneDown;
    float finalDistanceDown = 10;

    // Only apply down if there is collision model (otherwise we get jitter).
    if (groundCollisionModels->GetSize())
    {
        for(unsigned int i = 0; i < groundCollisionModels->GetSize(); ++i)
        {
            float checkLengthDown = 10;
            if (groundCollisionModels->At(i)->TestRay(BodyPos, Vector3f(0.0f, -1.0f, 0.0f),
                checkLengthDown, &collisionPlaneDown))
            {
                finalDistanceDown = Alg::Min(finalDistanceDown, checkLengthDown);
            }
        }

        // Maintain the minimum camera height
        if (UserEyeHeight - finalDistanceDown < 1.0f)
        {
            BodyPos.y += UserEyeHeight - finalDistanceDown;
        }
    }

}



// Handle directional movement. Returns 'true' if movement was processed.
bool Player::HandleMoveKey(OVR::KeyCode key, bool down)
{
    switch(key)
    {
        // Handle player movement keys.
        // We just update movement state here, while the actual translation is done in OnIdle()
        // based on time.
    case OVR::Key_W:     MoveForward = down ? (MoveForward | 1) : (MoveForward & ~1); return true;
    case OVR::Key_S:     MoveBack    = down ? (MoveBack    | 1) : (MoveBack    & ~1); return true;
    case OVR::Key_A:     MoveLeft    = down ? (MoveLeft    | 1) : (MoveLeft    & ~1); return true;
    case OVR::Key_D:     MoveRight   = down ? (MoveRight   | 1) : (MoveRight   & ~1); return true;
    case OVR::Key_Up:    MoveForward = down ? (MoveForward | 2) : (MoveForward & ~2); return true;
    case OVR::Key_Down:  MoveBack    = down ? (MoveBack    | 2) : (MoveBack    & ~2); return true;
    case OVR::Key_Left:  MoveLeft    = down ? (MoveLeft    | 2) : (MoveLeft    & ~2); return true;
    case OVR::Key_Right: MoveRight   = down ? (MoveRight   | 2) : (MoveRight   & ~2); return true;
    default: return false;
    }
}


