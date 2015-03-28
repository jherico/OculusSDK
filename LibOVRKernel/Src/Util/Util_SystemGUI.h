/************************************************************************************

Filename    :   Util_SystemGUI.h
Content     :   OS GUI access, usually for diagnostics.
Created     :   October 20, 2014
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

#ifndef OVR_Util_GUI_h
#define OVR_Util_GUI_h


namespace OVR { namespace Util {

    // Displays a modal message box on the default GUI display (not on a VR device). 
    // The message box interface (e.g. OK button) is not localized.
    bool DisplayMessageBox(const char* pTitle, const char* pText);

    bool DisplayMessageBoxF(const char* pTitle, const char* pFormat, ...);

} } // namespace OVR::Util


#endif
