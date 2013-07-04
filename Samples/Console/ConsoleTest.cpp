/************************************************************************************

Filename    :   SensorBoxTest.h
Content     :   Visual orientaion sensor test app; renders a rotating box over axes.
Created     :   October 1, 2012
Authors     :   Michael Antonov

Copyright   :   Copyright 2012 Oculus, Inc. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*************************************************************************************/

#include "OVR.h"
#include "OVR_DeviceImpl.h"

using namespace OVR;

int main(int argc, char** argv) {
    System::Init();
    DeviceManager * pManager = DeviceManager::Create();

    HMDDevice * pHMD = pManager->EnumerateDevices<HMDDevice>().CreateDevice();

}
