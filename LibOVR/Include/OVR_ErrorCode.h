/********************************************************************************//**

\file  OVR_ErrorCode.h
\brief This header provides LibOVR error code declarations

\copyright Copyright 2015 Oculus VR, LLC All Rights reserved.
\n
Licensed under the Oculus VR Rift SDK License Version 3.2 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.
\n
You may obtain a copy of the License at
\n
http://www.oculusvr.com/licenses/LICENSE-3.2 
\n
Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*************************************************************************************/

#ifndef OVR_ErrorCode_h
#define OVR_ErrorCode_h


#include "OVR_Version.h"
#include <stdint.h>







#ifndef OVR_RESULT_DEFINED
#define OVR_RESULT_DEFINED ///< Allows ovrResult to be independently defined.
/// API call results are represented at the highest level by a single ovrResult.
typedef int32_t ovrResult;
#endif


// Success is a value greater or equal to 0, while all error types are negative values.
#ifndef OVR_SUCCESS_DEFINED
#define OVR_SUCCESS_DEFINED ///< Allows ovrResult to be independently defined.
enum
{
    /// This is a general success result. Use OVR_SUCCESS to test for success.
    ovrSuccess = 0,
    
    /// Returned from a call to SubmitFrame. The call succeeded, but what the app
    /// rendered will not be visible on the HMD. Ideally the app should continue
    /// calling SubmitFrame, but not do any rendering. When the result becomes
    /// ovrSuccess, rendering should continue as usual.
    ovrSuccess_NotVisible = 1000,
};
#endif


/// \brief Indicates if an ovrResult indicates success.
///
/// Some functions return additional successful values other than ovrSucces and
/// require usage of this macro to indicate successs.
#if !defined(OVR_SUCCESS)
    #define OVR_SUCCESS(result) (result >= 0)
#endif


enum
{
    /* General errors */
    ovrError_MemoryAllocationFailure = -1000,   ///< Failure to allocate memory.
    ovrError_SocketCreationFailure   = -1001,   ///< Failure to create a socket.
    ovrError_InvalidHmd              = -1002,   ///< Invalid HMD parameter provided.
    ovrError_Timeout                 = -1003,   ///< The operation timed out.
    ovrError_NotInitialized          = -1004,   ///< The system or component has not been initialized.
    ovrError_InvalidParameter        = -1005,   ///< Invalid parameter provided. See error info or log for details.
    ovrError_ServiceError            = -1006,   ///< Generic service error. See error info or log for details.
    ovrError_NoHmd                   = -1007,   ///< The given HMD doesn't exist.

    /* Audio error range, reserved for Audio errors. */
    ovrError_AudioReservedBegin      = -2000,   ///< First Audio error.
    ovrError_AudioReservedEnd        = -2999,   ///< Last Audio error.

    /* Initialization errors. */
    ovrError_Initialize              = -3000,   ///< Generic initialization error.
    ovrError_LibLoad                 = -3001,   ///< Couldn't load LibOVRRT.
    ovrError_LibVersion              = -3002,   ///< LibOVRRT version incompatibility.
    ovrError_ServiceConnection       = -3003,   ///< Couldn't connect to the OVR Service.
    ovrError_ServiceVersion          = -3004,   ///< OVR Service version incompatibility.
    ovrError_IncompatibleOS          = -3005,   ///< The operating system version is incompatible.
    ovrError_DisplayInit             = -3006,   ///< Unable to initialize the HMD display.
    ovrError_ServerStart             = -3007,   ///< Unable to start the server. Is it already running?
    ovrError_Reinitialization        = -3008,   ///< Attempting to re-initialize with a different version.

    /*Hardware Errors*/
    ovrError_InvalidBundleAdjustment = -4000,   ///< Headset has no bundle adjustment data.
    ovrError_USBBandwidth            = -4001,   ///< The USB hub cannot handle the camera frame bandwidth.
};


#endif /* OVR_ErrorCode_h */


