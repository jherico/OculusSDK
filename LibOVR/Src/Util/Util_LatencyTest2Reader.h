/************************************************************************************

Filename    :   Util_LatencyTest2Reader.h
Content     :   Shared functionality for the DK2 latency tester
Created     :   July 8, 2014
Authors     :   Volga Aksoy, Chris Taylor

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

#ifndef OVR_Util_LatencyTest2Reader_h
#define OVR_Util_LatencyTest2Reader_h

#include "Vision/SensorFusion/Vision_SensorState.h"
#include "Util_LatencyTest2State.h"

namespace OVR { namespace Util {


//-----------------------------------------------------------------------------
// RecordStateReader

// User interface to retrieve pose from the sensor fusion subsystem
class RecordStateReader : public NewOverrideBase
{
protected:
    const Vision::CombinedHmdUpdater* Updater;

public:
    RecordStateReader()
        : Updater(NULL)
    {
    }

    // Initialize the updater
    void SetUpdater(const Vision::CombinedHmdUpdater *updater)
    {
        Updater = updater;
    }

    void GetRecordSet(FrameTimeRecordSet& recordset);
};


}} // namespace OVR::Util

#endif // OVR_Util_LatencyTest2Reader_h
