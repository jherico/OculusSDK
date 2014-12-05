/************************************************************************************

Filename    :   OVR_Win32_ShimVersion.h
Content     :   Versioning info for our display shim
Created     :   Nov 4, 2014
Authors     :   Scott Bassett

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

#define STRINGIZE_(x) #x
#define STRINGIZE(x) STRINGIZE_(x)

#define OVR_MAKE_VERSION(major, minor, patch)   (ULONG)(((major) << 24) | ((minor) << 16) | patch)
#define OVR_GET_VERSION_MAJOR(x)                (ULONG)(((x) >> 24) & 0x000000FF)
#define OVR_GET_VERSION_MINOR(x)                (ULONG)(((x) >> 16) & 0x000000FF)
#define OVR_GET_VERSION_PATCH(x)                (ULONG)((x) & 0x0000FFFF)

#define OVR_RENDER_SHIM_VERSION_MAJOR       1
#define OVR_RENDER_SHIM_VERSION_MINOR       0
#define OVR_RENDER_SHIM_VERSION_PATCH       0
#define OVR_RENDER_SHIM_VERSION             OVR_MAKE_VERSION(OVR_RENDER_SHIM_VERSION_MAJOR, OVR_RENDER_SHIM_VERSION_MINOR, OVR_RENDER_SHIM_VERSION_PATCH)
#define OVR_RENDER_SHIM_VERSION_STRING      (STRINGIZE(OVR_RENDER_SHIM_VERSION_MAJOR) "." STRINGIZE(OVR_RENDER_SHIM_VERSION_MINOR) "." STRINGIZE(OVR_RENDER_SHIM_VERSION_PATCH))

// IF YOU CHANGE ANY OF THESE NUMBERS YOU MUST UPDATE MULTIPLE FILES.
// PLEASE LOOK AT CHANGELIST 31947 TO SEE THE FULL LIST.
#define OVR_RTFILTER_VERSION_MAJOR          1
#define OVR_RTFILTER_VERSION_MINOR          2
#define OVR_RTFILTER_VERSION_PATCH          2
#define OVR_RTFILTER_VERSION                OVR_MAKE_VERSION(OVR_RTFILTER_VERSION_MAJOR, OVR_RTFILTER_VERSION_MINOR, OVR_RTFILTER_VERSION_PATCH)
#define OVR_RTFILTER_VERSION_STRING         (STRINGIZE(OVR_RTFILTER_VERSION_MAJOR) "." STRINGIZE(OVR_RTFILTER_VERSION_MINOR) "." STRINGIZE(OVR_RTFILTER_VERSION_PATCH))
