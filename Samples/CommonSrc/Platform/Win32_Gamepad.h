/************************************************************************************

Filename    :   Win32_Gamepad.h
Content     :   Win32 implementation of Gamepad functionality.
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

#ifndef OVR_Win32_Gamepad_h
#define OVR_Win32_Gamepad_h

#include "Gamepad.h"

#include <WinSock2.h>
#include <WS2tcpip.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <xinput.h>

namespace OVR { namespace OvrPlatform { namespace Win32 {

class GamepadManager : public OvrPlatform::GamepadManager
{
public:
    GamepadManager();
    ~GamepadManager();

    virtual uint32_t  GetGamepadCount();
    virtual bool    GetGamepadState(uint32_t index, GamepadState* pState);

private:
    // Dynamically ink to XInput to simplify projects.
    HMODULE             hXInputModule;
    typedef DWORD (WINAPI *PFn_XInputGetState)(DWORD dwUserIndex, XINPUT_STATE* pState);
    PFn_XInputGetState  pXInputGetState;

    uint32_t            LastPadPacketNo;    // Used to prevent reading the same packet twice.
    uint32_t            NextTryTime;        // If no device was found then we don't try to access it again until some later time. 
};

}}}

#endif // OVR_Win32_Gamepad_h
