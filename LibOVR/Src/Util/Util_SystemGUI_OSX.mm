/************************************************************************************

Filename    :   Util_SystemGUI.mm
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



#include "Util_SystemGUI.h"

#include <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>


namespace OVR { namespace Util {


bool DisplayMessageBox(const char* pTitle, const char* pText)
{
    // To consider: Replace usage of CFUserNotificationDisplayAlert with something a little smarter.
    
    size_t         titleLength = strlen(pTitle);
    size_t         textLength = strlen(pText);
    if(textLength > 1500)   // CFUserNotificationDisplayAlert isn't smart enough to handle large text sizes and screws up its size if so.
        textLength = 1500;  // Problem: this can theoretically split a UTF8 multibyte sequence. Need to find a divisible boundary.
    CFAllocatorRef allocator = NULL;  // To do: support alternative allocator.
    CFStringRef    titleRef = CFStringCreateWithBytes(allocator, (const UInt8*)pTitle, (CFIndex)titleLength, kCFStringEncodingUTF8, false);
    CFStringRef    textRef  = CFStringCreateWithBytes(allocator, (const UInt8*)pText,  (CFIndex)textLength,  kCFStringEncodingUTF8, false);
    CFOptionFlags  result;

    CFUserNotificationDisplayAlert(0,               // No timeout
                                   kCFUserNotificationNoteAlertLevel,
                                   NULL,            // Icon URL, use default.
                                   NULL,            // Unused
                                   NULL,            // Localization of strings
                                   titleRef,        // Title text
                                   textRef,         // Body text
                                   CFSTR("OK"),     // Default "OK" text in button
                                   CFSTR("Cancel"), // Other button title
                                   NULL,            // Yet another button title, NULL means no other button.
                                   &result);        // response flags
    CFRelease(titleRef);
    CFRelease(textRef);
    
    return (result == kCFUserNotificationDefaultResponse);
}


} } // namespace OVR { namespace Util {


