/************************************************************************************

Filename    :   GUIConsole.h
Content     :   A stdout console window that runs alongside Windows GUI applications
Created     :   Feb 4, 2013
Authors     :   Brant Lewis

Copyright   :   Copyright 2014 Oculus VR, LLC All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.2 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.2 

Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*************************************************************************************/

#ifndef OVR_Util_GuiConsole_h
#define OVR_Util_GuiConsole_h

#include "../../Include/OVR_Kernel.h"

#ifdef OVR_INTERNAL_USE

#include "../Kernel/OVR_Win32_IncludeWindows.h"

class GUIConsole
{
public:
    // constructors
    GUIConsole();
    ~GUIConsole();

    // member variables
    HANDLE hStdIn, hStdOut, hStdError;
};
#endif // #ifdef OVR_INTERNAL_USE

#endif  //OVR_Util_GuiConsole_h
