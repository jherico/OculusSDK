/************************************************************************************

Filename    :   Util_LatencyTest2_Legacy.h
Content     :   Backwards compatible code for 0.4/0.5
Created     :   April 16, 2015
Authors     :   Volga Aksoy, Chris Taylor

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

#ifndef OVR_Util_LatencyTest2_Legacy_h
#define OVR_Util_LatencyTest2_Legacy_h

#include <OVR_CAPI.h>
#include "Kernel/OVR_Types.h"

namespace OVR { namespace Util {


//-------------------------------------------------------------------------------------
// FrameTimeRecord

// Describes frame scan-out time used for latency testing.
//
// This structure needs to be the same size and layout on 32-bit and 64-bit arch.
// Update OVR_PadCheck.cpp when updating this object.
struct OVR_ALIGNAS(8) FrameTimeRecord
{
    int    ReadbackIndex;

    OVR_UNUSED_STRUCT_PAD(pad0, 4); // Indicates there is implicit padding added.

    double TimeSeconds;
};

//-----------------------------------------------------------------------------
// FrameTimeRecordSet
//
// Legacy frame timing structure for 0.4/0.5.
//
// This structure needs to be the same size and layout on 32-bit and 64-bit arch.
// Update OVR_PadCheck.cpp when updating this object.
struct OVR_ALIGNAS(8) FrameTimeRecordSet
{
    enum {
        RecordCount = 4,
        RecordMask = RecordCount - 1
    };
    FrameTimeRecord Records[RecordCount];
    int             NextWriteIndex;

    OVR_UNUSED_STRUCT_PAD(pad0, 4); // Indicates there is implicit padding added.

    FrameTimeRecordSet();

    void AddValue(int readValue, double timeSeconds);
    // Matching should be done starting from NextWrite index 
    // until wrap-around

    const FrameTimeRecord& operator [] (int i) const;

    // Advances I to absolute color index
    bool FindReadbackIndex(int* i, int readbackIndex) const;

    bool IsAllZeroes() const;
};

static_assert(sizeof(FrameTimeRecord) == 4 + 4 + 8, "size mismatch");
static_assert(sizeof(FrameTimeRecordSet) == sizeof(FrameTimeRecord) * FrameTimeRecordSet::RecordCount + 4 + 4, "size mismatch");


}} // namespace OVR::Util

#endif // OVR_Util_LatencyTest2_Legacy_h
