/************************************************************************************

Filename    :   CAPI_GlobalState.h
Content     :   Maintains global state of the CAPI
Created     :   January 24, 2013
Authors     :   Michael Antonov

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.1 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.1 

Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#ifndef OVR_CAPI_GlobalState_h
#define OVR_CAPI_GlobalState_h

#include "../OVR_CAPI.h"
#include "../OVR_Device.h"
#include "../Kernel/OVR_Timer.h"
#include "../Kernel/OVR_Math.h"

#include "CAPI_HMDState.h"

namespace OVR { namespace CAPI {

//-------------------------------------------------------------------------------------
// ***** OVRGlobalState

// Global DeviceManager state - singleton instance of this is created
// by ovr_Initialize().
class GlobalState  : public MessageHandler,  public NewOverrideBase
{  
public:
    GlobalState();
    ~GlobalState();

    static GlobalState *pInstance;
    
    int         EnumerateDevices();
    HMDDevice*  CreateDevice(int index);

    // MessageHandler implementation
    void        OnMessage(const Message& msg);

    // Helpers used to keep track of HMDs and notify them of sensor changes.
    void        AddHMD(HMDState* hmd);
    void        RemoveHMD(HMDState* hmd);
    void        NotifyHMDs_AddDevice(DeviceType deviceType);

    const char* GetLastError()
    {
        return 0;
    }

    DeviceManager* GetManager() { return pManager; }

protected:

    Ptr<DeviceManager>  pManager;
    Lock                EnumerationLock;
    Array<DeviceHandle> EnumeratedDevices;
    
    // Currently created hmds; protected by Manager lock.
    List<HMDState>      HMDs;
};

}} // namespace OVR::CAPI

#endif


