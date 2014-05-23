/************************************************************************************

Filename    :   OVR_OSX_SensorDevice.cpp
Content     :   OSX SensorDevice implementation
Created     :   March 14, 2013
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

#include "OVR_OSX_HMDDevice.h"
#include "OVR_SensorImpl.h"
#include "OVR_DeviceImpl.h"

namespace OVR { namespace OSX {

} // namespace OSX

//-------------------------------------------------------------------------------------
void SensorDeviceImpl::EnumerateHMDFromSensorDisplayInfo(const SensorDisplayInfoImpl& displayInfo, 
                                                         DeviceFactory::EnumerateVisitor& visitor)
{

    OSX::HMDDeviceCreateDesc hmdCreateDesc(&OSX::HMDDeviceFactory::GetInstance(), 1, 1, "", 0);

    hmdCreateDesc.SetScreenParameters(  0, 0,
                                        displayInfo.HResolution, displayInfo.VResolution,
                                        displayInfo.HScreenSize, displayInfo.VScreenSize,
                                        displayInfo.VCenter, displayInfo.LensSeparation);

    if ((displayInfo.DistortionType & SensorDisplayInfoImpl::Mask_BaseFmt) == SensorDisplayInfoImpl::Base_Distortion)
    {
        hmdCreateDesc.SetDistortion(displayInfo.DistortionK);
        // TODO: add DistortionEqn
    }
    
    visitor.Visit(hmdCreateDesc);
}

} // namespace OVR


