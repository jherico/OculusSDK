/************************************************************************************

Filename    :   Win32_Gamepad.cpp
Content     :   Win32 implementation of Platform app infrastructure
Created     :   May 6, 2013
Authors     :   Lee Cooper

Copyright   :   Copyright 2012 Oculus VR, LLC. All Rights reserved.

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

#include "Win32_Gamepad.h"


OVR_DISABLE_MSVC_WARNING(28159) // C28159: GetTickCount: consider using another function instead.


namespace OVR { namespace OvrPlatform { namespace Win32 {

GamepadManager::GamepadManager() : 
  //hXInputModule(NULL),
    pXInputGetState(NULL),
    LastPadPacketNo(0xffffffff),
    NextTryTime(0)
{
    hXInputModule = ::LoadLibraryA("Xinput9_1_0.dll");
    if (hXInputModule)
    {
        pXInputGetState = (PFn_XInputGetState)
            ::GetProcAddress(hXInputModule, "XInputGetState");        
    }
}

GamepadManager::~GamepadManager()
{
    if (hXInputModule)
        ::FreeLibrary(hXInputModule);
}

static inline float GamepadStick(short in)
{
    float v;
    if (abs(in) < 9000)
        return 0;
    else if (in > 9000)
        v = (float) in - 9000;
    else
        v = (float) in + 9000;
    return v / (32767 - 9000);
}

static inline float GamepadTrigger(BYTE in)
{
    if (in < 30)
        return 0;
    else
        return float(in-30) / 225;
}

uint32_t GamepadManager::GetGamepadCount()
{
    return 1;
}

bool GamepadManager::GetGamepadState(uint32_t index, GamepadState* pState)
{
    // For now we just support one gamepad.
    OVR_UNUSED(index);

    if (pXInputGetState)
    {
        if((NextTryTime == 0) || (GetTickCount() >= NextTryTime)) // If the device is known to be present or if it's time to try testing for it again...
        {
            XINPUT_STATE xis;
            DWORD dwResult = pXInputGetState(0, &xis); // This function is expensive, including if there is no connected device.

            if(dwResult == ERROR_SUCCESS)
            {
                if (xis.dwPacketNumber != LastPadPacketNo)
                {
                    // State changed.
                    pState->Buttons = xis.Gamepad.wButtons; // Currently matches Xinput
                    pState->LT = GamepadTrigger(xis.Gamepad.bLeftTrigger);
                    pState->RT = GamepadTrigger(xis.Gamepad.bRightTrigger);
                    pState->LX = GamepadStick(xis.Gamepad.sThumbLX);
                    pState->LY = GamepadStick(xis.Gamepad.sThumbLY);
                    pState->RX = GamepadStick(xis.Gamepad.sThumbRX);
                    pState->RY = GamepadStick(xis.Gamepad.sThumbRY);

                    LastPadPacketNo = xis.dwPacketNumber;
                    NextTryTime = 0;

                    return true;
                }
            }
            else if(dwResult == ERROR_DEVICE_NOT_CONNECTED)
            {
                // Don't bother wasting time on XInputGetState if one isn't connected, as it's very slow when one isn't connected.
                // GetTickCount64 is available with Windows Vista+ and doesn't wrap around.
                // GetTickCount wraps around every 49.7 days since the system started, but we don't need absolute time and it's OK 
                // if we have a false positive which would occur if NextTryTime is set to a value that has wrapped around to zero.
                NextTryTime = GetTickCount() + 5000;
            }
        }
    }

    return false;
}

}}} // OVR::OvrPlatform::Win32

OVR_RESTORE_MSVC_WARNING()

