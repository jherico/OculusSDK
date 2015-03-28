/************************************************************************************

Filename    :   Util_DataLogger.h
Content     :   General purpose data logging to Matlab
Created     :   Oct 3, 2014
Authors     :   Neil Konzen

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.

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

#ifndef OVR_Util_DataLogger_h
#define OVR_Util_DataLogger_h

#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_String.h"
#include "Util_MatFile.h"

#ifndef OVR_ENABLE_DATALOGGER
#define OVR_ENABLE_DATALOGGER   1
#endif

namespace OVR {

template<typename T> class Vector3;
template<typename T> class Quat;
template<typename T> class Pose;

namespace Util {

#if OVR_ENABLE_DATALOGGER

class DataLogger;


// DataLogger Channel
class DataLoggerChannel
{
public:
    void Log(const void* data, int sampleSize);

    bool IsLogging() const;
    bool IsFull() const;

    // Logging functions for some common types
    template<typename T>
    void Log(const Quat<T>& q);

    template<typename T>
    void Log(const Vector3<T>& v);

    template<typename T>
    void Log(const Pose<T>& p);

private:
    friend class DataLogger;

    DataLoggerChannel(DataLogger* logger, const char* name, int sampleSize, int sampleRate, bool doubleMatrix);
    ~DataLoggerChannel();

    bool Write(MatFile& matfile);

private:
    DataLogger* Logger;
    String      Name;
    int         SampleSize;
    int         SampleRate;
    int         SampleCount;
    int         MaxSampleCount;
    bool        DoubleMatrix;
    uint8_t*    SampleData;
};

// DataLogger class
class DataLogger
{
public:
    DataLogger();
    ~DataLogger();

    // Parses C command line parameters "-log <objectIndex> <matFileName> <logTime>"
    static bool ParseCommandLine(int* pargc, const char** pargv[], int& logIndex, String& logFile, double& logTime);

    void SetLogFile(const char* filename, double logTime);
    bool SaveLogFile();

    bool IsLogging() const                       { return LogTime > 0; }

    DataLoggerChannel* GetChannel(const char* name);
    DataLoggerChannel* CreateChannel(const char* name, int sampleSize, int sampleRate, bool doubleMatrix = true);

    void Log(const char* name, const void* data, int sampleSize, int sampleRate, bool doubleMatrix = true)
    {
        DataLoggerChannel *channel = CreateChannel(name, sampleSize, sampleRate, doubleMatrix);
        if (channel) channel->Log(data, sampleSize);
    }

private:
    friend class DataLoggerChannel;

    DataLoggerChannel* GetChannelNoLock(const char* name);

    Lock   TheLock;
    double LogTime;
    String Filename;
    ArrayPOD<DataLoggerChannel*> Channels;
};

#else   // OVR_ENABLE_DATALOGGER

// Disabled, no-op implementation
class DataLoggerChannel
{
public:
    OVR_FORCE_INLINE void Log(const void* data, int sampleSize)    { OVR_UNUSED2(data, sampleSize); }

    OVR_FORCE_INLINE bool IsLogging() const                  { return false; }
    OVR_FORCE_INLINE bool IsFull() const                     { return false; }

    template<typename T>
    OVR_FORCE_INLINE void Log(const Quat<T>& q)    { OVR_UNUSED(q);  }

    template<typename T>
    OVR_FORCE_INLINE void Log(const Vector3<T>& v) { OVR_UNUSED(v);  }

    template<typename T>
    OVR_FORCE_INLINE void Log(const Pose<T>& p)    { OVR_UNUSED(p); }
};

class DataLogger
{
public:
    static bool ParseCommandLine(int* pargc, const char** pargv[], int& logIndex, String& logFile, double& logTime)   { OVR_UNUSED5(pargc, pargv, logIndex, logFile, logTime); return false; }

    OVR_FORCE_INLINE void SetLogFile(const char* filename, double logTime)   { OVR_UNUSED2(filename, logTime);  }
    OVR_FORCE_INLINE bool SaveLogFile()                          { return true; }

    OVR_FORCE_INLINE bool IsLogging() const                      { return false; }

    OVR_FORCE_INLINE DataLoggerChannel* GetChannel(const char* name)       { OVR_UNUSED(name);  return &NullChannel; }
    OVR_FORCE_INLINE DataLoggerChannel* CreateChannel(const char* name, int sampleSize, int sampleRate)  { OVR_UNUSED3(name, sampleSize, sampleRate); return &NullChannel; }

private:
    friend class DataLoggerChannel;
    static DataLoggerChannel NullChannel;
};

#endif  // OVR_ENABLE_DATALOGGER


}}	// namespace OVR::Util

#endif // OVR_Util_DataLogger_h
