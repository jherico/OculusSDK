/************************************************************************************

PublicHeader:   OVR_ErrorCode.h
Copyright   :   Copyright 2015 Oculus VR, LLC All Rights reserved.

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

#include "OVR_Version.h"




#ifndef OVR_ErrorCode_h
#define OVR_ErrorCode_h


#include <stdint.h>


/* API call results are represented at the highest level by a single ovrResult. */
#ifndef OVR_RESULT_DEFINED
#define OVR_RESULT_DEFINED
typedef int32_t ovrResult;
#endif


/* Success is zero, while all error types are non-zero values. */
#ifndef OVR_SUCCESS_DEFINED
#define OVR_SUCCESS_DEFINED
const ovrResult ovrSuccess = 0;
#endif


enum
{
    /* Initialization errors. */
    ovrError_Initialize         = 1000,   /* Generic initialization error.                          */
    ovrError_LibLoad            = 1001,   /* Couldn't load LibOVRRT.                                */
    ovrError_LibVersion         = 1002,   /* LibOVRRT version incompatibility.                      */
    ovrError_ServiceConnection  = 1003,   /* Couldn't connect to the OVR Service.                   */
    ovrError_ServiceVersion     = 1004,   /* OVR Service version incompatibility.                   */
    ovrError_IncompatibleOS     = 1005,   /* The operating system version is incompatible.          */
    ovrError_DisplayInit        = 1006,   /* Unable to initialize the HMD display.                  */
    ovrError_ServerStart        = 1007,   /* Unable to start the server. Is it already running?     */
    ovrError_Reinitialization   = 1008    /* Attempting to re-initialize with a different version.  */
};


#endif /* Header include guard */


