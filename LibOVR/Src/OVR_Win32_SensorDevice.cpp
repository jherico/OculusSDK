/************************************************************************************

Filename    :   OVR_Win32_SensorDevice.cpp
Content     :   Win32 SensorDevice implementation
Created     :   March 12, 2013
Authors     :   Lee Cooper

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

*************************************************************************************/

#include "OVR_Win32_SensorDevice.h"

#include "OVR_Win32_HMDDevice.h"
#include "OVR_SensorImpl.h"
#include "OVR_DeviceImpl.h"

namespace OVR { namespace Win32 {

} // namespace Win32

//-------------------------------------------------------------------------------------
void SensorDeviceImpl::EnumerateHMDFromSensorDisplayInfo
    (const SensorDisplayInfoImpl& displayInfo, 
     DeviceFactory::EnumerateVisitor& visitor)
{

    Win32::HMDDeviceCreateDesc hmdCreateDesc(&Win32::HMDDeviceFactory::Instance, String(), String());
    hmdCreateDesc.SetScreenParameters(  0, 0,
                                        displayInfo.HResolution, displayInfo.VResolution,
                                        displayInfo.HScreenSize, displayInfo.VScreenSize,
                                        displayInfo.VCenter, displayInfo.LensSeparation);

    if ((displayInfo.DistortionType & SensorDisplayInfoImpl::Mask_BaseFmt) == SensorDisplayInfoImpl::Base_Distortion)
    {
        // TODO: update to spline system.
        hmdCreateDesc.SetDistortion(displayInfo.DistortionK);
    }
   
    visitor.Visit(hmdCreateDesc);
}

} // namespace OVR


