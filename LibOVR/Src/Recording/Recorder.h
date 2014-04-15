/************************************************************************************

Filename    :   Recorder.h
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

#ifndef OVR_Recorder_h
#define OVR_Recorder_h

#define ENABLE_RECORDING 0

#include "../../Include/OVR.h"

#if ENABLE_RECORDING

#include "../Vision/Vision_CameraCalibration.h"
#include "../Vision/Vision_Blob.h"
#include "../Vision/Vision_Image.h"
#include "../Vision/Vision_ModulatedLEDModel.h"
#include "LogDataTypes.h"
#include "matfile.h"

#define RECORDING_LOCATION "%Y%m%d_%H%M%S"

#endif
namespace OVR{

    typedef UByte RecordingMode;
        enum RecordingMode_t
        {
            RecordingOff = 0x0,
            RecordForPlayback = 0x1,
            RecordForLogging = 0x2
        };
};

#if ENABLE_RECORDING

namespace OVR{
    
    class Recorder
    {
    public:

        static void SetPrefix(const char* prefix);
        static String GetPrefix();

        struct StartupParams
        {
            StartupParams()
                : intrinsics(),
                  distortion(),
                  ledPositions(),
                  imuPosition(),
                  devIfcVersion(1)
            {}

            ~StartupParams() 
            {
            }

            Vision::CameraIntrinsics		 intrinsics;
            Vision::DistortionCoefficients	 distortion;
            Array<PositionCalibrationReport> ledPositions;
            PositionCalibrationReport		 imuPosition;
            UByte							 devIfcVersion;
        };

        // Global Interface
        static void Buffer(const Message& msg);

        static void Buffer(const Vision::CameraIntrinsics& intrinsics, 
                           const Vision::DistortionCoefficients& distortion);

        static void Buffer(const Array<PositionCalibrationReport>& ledPositions);

        static void Buffer(const PositionCalibrationReport& imuPosition);

        static void BufferDevIfcVersion(const UByte devIfcVersion);

        template<typename T>
        static void LogData(const char* label, const T& data)
        {
            Recorder* myRecorder = GetRecorder();
            if(myRecorder && (myRecorder->recordingMode & RecordForLogging))
                myRecorder->DoLogData(label, data);
        }

        static void LogData(const char* label, const Vision::Blob blobs[], const int numElements);

        static void LogData(const char* label, const Vector3d& vect3);

        static void LogData(const char* label, const Quatd& quat);

        static void LogData(const char* label, const Posed& pose);

        static Recorder* GetRecorder();
        // Instantiates Recorder if it does not already exist.  
        static Recorder* BuildRecorder();
        // Activates or deactivates recording.  Returns resultant state (true = recording, false = not recording).
        static bool ToggleRecording(const RecordingMode mode);

        Recorder();
    
        ~Recorder();

        void SaveCameraParams(const Vision::CameraIntrinsics& intrinsics,
                              const Vision::DistortionCoefficients& distortion);

        void SaveLedPositions(const Array<PositionCalibrationReport>& ledPositions);

        void SaveImuPosition(const PositionCalibrationReport& imuPosition);

        void SaveDevIfcVersion(const UByte devIfcVersion);

        void WriteToRec(const Array<UByte>& buffer);

        template<class T>
        void DoLogData(const char* label, const T& data)
        {
            if(!(recordingMode & RecordForLogging))
                return;
            Ptr<LogDataEntryBase> entry;
            StringHash<Ptr<LogDataEntryBase> >::Iterator iter = logDataBuffer.Find(label);
            if(!iter.IsEnd())
                entry = logDataBuffer.Find(label)->Second;
            if(!entry)
            {
                // Add new entry
                entry = getNewEntry(label, data);
                logDataBuffer.Add(label, entry);
            }

            OVR_ASSERT(entry != NULL);

            // Add new sample to the entry that we found

            Array<T>& myBuffer = dynamic_cast<LogDataEntry<T>*>(entry.GetPtr())->buffer;
            myBuffer.PushBack(data);
        }

        void DoLogData(const char* label, const Vision::Blob blobs[]);

        void DoLogData(const char* label, const Posed& pose);

        void DoLogData(const char* label, const Vector3d& vect3);

        void DoLogData(const char* label, const Quatd& quat);
        
        // Activates or deactivates recording.  Returns resultant state (true = recording, false = not recording).
        bool DoToggleRecording(const RecordingMode mode);

        // Keep this up-to-date when the recording format changes
        static const UInt16	RECORDING_FORMAT_VERSION = 1;

    private:
        Ptr<LogDataEntryBase> getNewEntry(const char* label, const float&);
        Ptr<LogDataEntryBase> getNewEntry(const char* label, const double&);
        Ptr<LogDataEntryBase> getNewEntry(const char* label, const int&);
        Ptr<LogDataEntryBase> getNewEntry(const char* label, const Vision::Blob[]);
        Ptr<LogDataEntryBase> getNewEntry(const char* label, const Posed&);
        Ptr<LogDataEntryBase> getNewEntry(const char* label, const Vector3d&);
        Ptr<LogDataEntryBase> getNewEntry(const char* label, const Quatd&);

        void start();

        // Serialize the startup params that we have saved and write them to file.  Then set readyForMessages flag.
        void writeStartupParams();
    
        int bufferPositionReport(const PositionCalibrationReport& report, UByte* buffer);

        void finalize();

        void writeBlobStats(const char* label, LogDataEntryBase* entry);

        void writeVector3d(const char* label, const Array<Vector3d>& data);

        void writePosed(const char* label, const Array<Posed>& data);

        void writeQuatd(const char* label, const Array<Quatd>& data);

        void reset();

        String getFilePrefix();

        // File that will contain simulation/playback data
        FILE*								recFile;
        vortex::CMatFile					matFile;

        // Logging data to be written to .mat file
        StringHash<Ptr<LogDataEntryBase> > logDataBuffer;

        StartupParams startup; // Startup params.  Must be written before general messages
        bool		  readyForMessages; // Indicates that the startup params have been written, and we can safely write messages to the .rec file

        // To preserve ordering of incoming messages
        Lock								recorderLock;
        // How/are we currently recording?		
        UByte								recordingMode;
    };

};

#else  // If Recording is not enabled, then no-op all the functions so they can be inlined/optimized away by the compiler.

namespace OVR{
    
    namespace Vision{
        class CameraIntrinsics;
        class DistortionCoefficients;
        class Blob;
    };
    struct PositionCalibrationReport;

    class Recorder
    {
    public:
        static void Buffer(const Message&) { }

        static void Buffer(const Vision::CameraIntrinsics&, 
                           const Vision::DistortionCoefficients&)
        { }

        static void Buffer(const Array<PositionCalibrationReport>&) { } 

        static void Buffer(const PositionCalibrationReport&) { }

        static void BufferDevIfcVersion(const UByte) { };

        static Recorder* GetRecorder() { return NULL; }

        static Recorder* BuildRecorder() { return NULL; }

        static bool ToggleRecording(const int) { return false; }

        template<typename T>
        static void LogData(const char*, const T&) { };

        static void LogData(const char*, const Vision::Blob[], const int) { };

        Recorder() { }
    
        ~Recorder() { }

        bool DoToggleRecording(const int) { return false; }

        void AddToBuffer(const Message&) { }
    };
} // namespace OVR

#endif // ENABLE_RECORDING

#endif // OVR_Recorder_h
