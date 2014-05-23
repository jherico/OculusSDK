/************************************************************************************

Filename    :   Recording.h
Content     :   Support for recording sensor + camera data
Created     :   March 14, 2014
Notes       : 

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

#ifndef OVR_Recording_h
#define OVR_Recording_h

namespace OVR { namespace Recording {

enum RecordingMode
{
    RecordingOff = 0x0,
    RecordForPlayback = 0x1,
    RecordForLogging = 0x2
};

}} // OVR::Recording

#ifdef ENABLE_RECORDING

#include "Recording/Recording_Recorder.h"

#else
// If Recording is not enabled, then stub it out

namespace OVR { 
    
struct PositionCalibrationReport;
namespace Vision {
    class CameraIntrinsics;
    class DistortionCoefficients;
    class Blob;
};

namespace Recording {

class Recorder
{
public:
    OVR_FORCE_INLINE void RecordCameraParams(const Vision::CameraIntrinsics&, 
                                             const Vision::DistortionCoefficients&) { }
    OVR_FORCE_INLINE void RecordLedPositions(const Array<PositionCalibrationReport>&) { }
    OVR_FORCE_INLINE void RecordUserParams(const Vector3f&, float) { }
    OVR_FORCE_INLINE void RecordDeviceIfcVersion(UByte) { }
    OVR_FORCE_INLINE void RecordMessage(const Message&) { }
    OVR_FORCE_INLINE void RecordCameraFrameUsed(UInt32) { }
    OVR_FORCE_INLINE void RecordVisionSuccess(UInt32) { }
    template<typename T> OVR_FORCE_INLINE void LogData(const char*, const T&) { }
    OVR_FORCE_INLINE void SetRecordingMode(RecordingMode) { }
    OVR_FORCE_INLINE RecordingMode GetRecordingMode() { return RecordingOff; }
};

extern Recorder r;

OVR_FORCE_INLINE Recorder& GetRecorder() { return r; }
    
}} // namespace OVR::Recording

#endif // ENABLE_RECORDING

#endif // OVR_Recording_h