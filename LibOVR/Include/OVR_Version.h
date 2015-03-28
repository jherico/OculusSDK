/************************************************************************************

Filename    :   OVRVersion.h
Content     :

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

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

#ifndef OVR_Version_h
#define OVR_Version_h





#if !defined(OVR_STRINGIZE)
    #define OVR_STRINGIZEIMPL(x) #x
    #define OVR_STRINGIZE(x)     OVR_STRINGIZEIMPL(x)
#endif

// We are on major version 5 of the beta pre-release SDK. At some point we will
// transition to product version 1 and reset the major version back to 1 (first 
// product release, version 1.0).
#define OVR_PRODUCT_VERSION 0
#define OVR_MAJOR_VERSION   5
#define OVR_MINOR_VERSION   0
#define OVR_PATCH_VERSION   1
#define OVR_BUILD_NUMBER    0

// "Product.Major.Minor.Patch"
#if !defined(OVR_VERSION_STRING)
    #define OVR_VERSION_STRING  OVR_STRINGIZE(OVR_PRODUCT_VERSION.OVR_MAJOR_VERSION.OVR_MINOR_VERSION.OVR_PATCH_VERSION)
#endif

// "Product.Major.Minor.Patch.Build"
#if !defined(OVR_DETAILED_VERSION_STRING)
    #define OVR_DETAILED_VERSION_STRING OVR_STRINGIZE(OVR_PRODUCT_VERSION.OVR_MAJOR_VERSION.OVR_MINOR_VERSION.OVR_PATCH_VERSION.OVR_BUILD_NUMBER)
#endif

// This is the firmware version for the DK2 headset sensor board.
//#if !defined(OVR_DK2_LATEST_FIRMWARE_MAJOR_VERSION)
    #define OVR_DK2_LATEST_FIRMWARE_MAJOR_VERSION 2
    #define OVR_DK2_LATEST_FIRMWARE_MINOR_VERSION 12
//#endif

// LibOVRRT description
// This appears in the user-visible file properties. It is intended to convey publicly available additional information
// such as feature builds.
#if !defined(OVR_FILE_DESCRIPTION_STRING)
    #if defined(_DEBUG)
        #define OVR_FILE_DESCRIPTION_STRING "LibOVRRT (debug)"
    #else
        #define OVR_FILE_DESCRIPTION_STRING "LibOVRRT"
    #endif
#endif


#endif // OVR_Version_h
