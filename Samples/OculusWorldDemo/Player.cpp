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

Player::Player(void)
	: EyeHeight(1.8f),
	  EyePos(7.7f, 1.8f, -1.0f),
      EyeYaw(YawInitial), EyePitch(0), EyeRoll(0),
      LastSensorYaw(0)
{
	MoveForward = MoveBack = MoveLeft = MoveRight = 0;
    GamepadMove = Vector3f(0);
    GamepadRotate = Vector3f(0);
}


Player::~Player(void)
{
}

void Player::HandleCollision(double dt, Array<Ptr<CollisionModel> >* collisionModels,
	                         Array<Ptr<CollisionModel> >* groundCollisionModels, bool shiftDown)
{
	if(MoveForward || MoveBack || MoveLeft || MoveRight || GamepadMove.LengthSq() > 0)
    {
        Vector3f orientationVector;
        // Handle keyboard movement.
        // This translates EyePos based on Yaw vector direction and keys pressed.
        // Note that Pitch and Roll do not affect movement (they only affect view).
        if(MoveForward || MoveBack || MoveLeft || MoveRight)
        {
            Vector3f localMoveVector(0, 0, 0);
            Matrix4f yawRotate = Matrix4f::RotationY(EyeYaw);
            
            if (MoveForward)
            {
                localMoveVector = ForwardVector;
            }
            else if (MoveBack)
            {
                localMoveVector = -ForwardVector;
            }

            if (MoveRight)
            {
                localMoveVector += RightVector;
            }
            else if (MoveLeft)
            {
                localMoveVector -= RightVector;
            }

            // Normalize vector so we don't move faster diagonally.
            localMoveVector.Normalize();
            orientationVector = yawRotate.Transform(localMoveVector);
        }
        else if (GamepadMove.LengthSq() > 0)
        {
            Matrix4f yawRotate = Matrix4f::RotationY(EyeYaw);
            GamepadMove.Normalize();
            orientationVector = yawRotate.Transform(GamepadMove);
        }

        float moveLength = OVR::Alg::Min<float>(MoveSpeed * (float)dt * (shiftDown ? 3.0f : 1.0f), 1.0f);

        float   checkLengthForward = moveLength;
        Planef  collisionPlaneForward;
        float   checkLengthLeft = moveLength;
        Planef  collisionPlaneLeft;
        float   checkLengthRight = moveLength;
        Planef  collisionPlaneRight;
        bool    gotCollision = false;
        bool    gotCollisionLeft = false;
        bool    gotCollisionRight = false;

        for(unsigned int i = 0; i < collisionModels->GetSize(); ++i)
        {
            // Checks for collisions at eye level, which should prevent us from
			// slipping under walls
            if (collisionModels->At(i)->TestRay(EyePos, orientationVector, checkLengthForward,
				                                &collisionPlaneForward))
            {
                gotCollision = true;
            }

            Matrix4f leftRotation = Matrix4f::RotationY(45 * (Math<float>::Pi / 180.0f));
            Vector3f leftVector   = leftRotation.Transform(orientationVector);
            if (collisionModels->At(i)->TestRay(EyePos, leftVector, checkLengthLeft,
				                                &collisionPlaneLeft))
            {
                gotCollisionLeft = true;
            }
            Matrix4f rightRotation = Matrix4f::RotationY(-45 * (Math<float>::Pi / 180.0f));
            Vector3f rightVector   = rightRotation.Transform(orientationVector);
            if (collisionModels->At(i)->TestRay(EyePos, rightVector, checkLengthRight,
				                                &collisionPlaneRight))
            {
                gotCollisionRight = true;
            }
        }

        if (gotCollision)
        {
            // Project orientationVector onto the plane
            Vector3f slideVector = orientationVector - collisionPlaneForward.N
				* (orientationVector * collisionPlaneForward.N);

            // Make sure we aren't in a corner
            for(unsigned int j = 0; j < collisionModels->GetSize(); ++j)
            {
                if (collisionModels->At(j)->TestPoint(EyePos - Vector3f(0.0f, RailHeight, 0.0f) +
					                                  (slideVector * (moveLength))) )
                {
                    moveLength = 0;
                }
            }
            if (moveLength != 0)
            {
                orientationVector = slideVector;
            }
        }
        // Checks for collisions at foot level, which allows us to follow terrain
        orientationVector *= moveLength;
        EyePos += orientationVector;

        Planef collisionPlaneDown;
        float finalDistanceDown = 10;

        for(unsigned int i = 0; i < groundCollisionModels->GetSize(); ++i)
        {
            float checkLengthDown = 10;
            if (groundCollisionModels->At(i)->TestRay(EyePos, Vector3f(0.0f, -1.0f, 0.0f),
				                                      checkLengthDown, &collisionPlaneDown))
            {
                finalDistanceDown = Alg::Min(finalDistanceDown, checkLengthDown);
            }
        }

        // Maintain the minimum camera height
        if (EyeHeight - finalDistanceDown < 1.0f)
        {
            EyePos.y += EyeHeight - finalDistanceDown;
        }
    }
}
