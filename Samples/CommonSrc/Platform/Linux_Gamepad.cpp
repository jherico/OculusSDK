/************************************************************************************

Filename    :   Linux_Gamepad.cpp
Content     :   Linux implementation of Platform app infrastructure
Created     :   May 6, 2013
Authors     :   Lee Cooper

Copyright   :   Copyright 2013 Oculus VR, Inc. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#include "Linux_Gamepad.h"

namespace OVR { namespace Platform { namespace Linux {

GamepadManager::GamepadManager()
{

}

GamepadManager::~GamepadManager()
{

}

UInt32 GamepadManager::GetGamepadCount()
{
    return 1;
}

bool GamepadManager::GetGamepadState(UInt32 index, GamepadState* pState)
{
    return false;
}

}}} // OVR::Platform::Linux
