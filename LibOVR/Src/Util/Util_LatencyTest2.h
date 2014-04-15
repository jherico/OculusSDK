/************************************************************************************

PublicHeader:   OVR.h
Filename    :   Util_LatencyTest2.h
Content     :   Wraps the lower level LatencyTester interface for DK2 and adds functionality.
Created     :   March 10, 2014
Authors     :   Volga Aksoy

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

#ifndef OVR_Util_LatencyTest2_h
#define OVR_Util_LatencyTest2_h

#include "../OVR_Device.h"

#include "../Kernel/OVR_String.h"
#include "../Kernel/OVR_List.h"
#include "../Kernel/OVR_Lockless.h"

namespace OVR { namespace Util {


enum {
    LT2_ColorIncrement                  = 32,
    LT2_PixelTestThreshold              = LT2_ColorIncrement / 3,
    LT2_IncrementCount                  = 256 / LT2_ColorIncrement,
    LT2_TimeoutWaitingForColorDetected  = 1000  // 1 second
};

//-------------------------------------------------------------------------------------

// Describes frame scanout time used for latency testing.
struct FrameTimeRecord
{
    int    ReadbackIndex;
    double TimeSeconds;

    // Utility functions to convert color to readBack indices and back.
    // The purpose of ReadbackIndex is to allow direct comparison by value.

    static bool ColorToReadbackIndex(int *readbackIndex, unsigned char color)
    {
        int compareColor = color - LT2_ColorIncrement/2;
        int index        = color / LT2_ColorIncrement;  // Use color without subtraction due to rounding.
        int delta        = compareColor - index * LT2_ColorIncrement;

        if ((delta < LT2_PixelTestThreshold) && (delta > -LT2_PixelTestThreshold))
        {
            *readbackIndex = index;
            return true;
        }
        return false;
    }

    static unsigned char ReadbackIndexToColor(int readbackIndex)
    {
        OVR_ASSERT(readbackIndex < LT2_IncrementCount);
        return (unsigned char)(readbackIndex * LT2_ColorIncrement + LT2_ColorIncrement/2);
    }
};

// FrameTimeRecordSet is a container holding multiple consecutive frame timing records
// returned from the lock-less state. Used by FrameTimeManager. 

struct FrameTimeRecordSet
{
    enum {
        RecordCount = 4,
        RecordMask  = RecordCount - 1
    };
    FrameTimeRecord Records[RecordCount];    
    int             NextWriteIndex;

    FrameTimeRecordSet()
    {
        NextWriteIndex = 0;
        memset(this, 0, sizeof(FrameTimeRecordSet));
    }

    void AddValue(int readValue, double timeSeconds)
    {        
        Records[NextWriteIndex].ReadbackIndex = readValue;
        Records[NextWriteIndex].TimeSeconds   = timeSeconds;
        NextWriteIndex ++;
        if (NextWriteIndex == RecordCount)
            NextWriteIndex = 0;
    }
    // Matching should be done starting from NextWrite index 
    // until wrap-around

    const FrameTimeRecord& operator [] (int i) const
    {
        return Records[(NextWriteIndex + i) & RecordMask];
    }

    const FrameTimeRecord& GetMostRecentFrame()
    {
        return Records[(NextWriteIndex - 1) & RecordMask];
    }

    // Advances I to  absolute color index
    bool FindReadbackIndex(int* i, int readbackIndex) const
    {
        for (; *i < RecordCount; *i++)
        {
            if ((*this)[*i].ReadbackIndex == readbackIndex)
                return true;
        }
        return false;
    }

    bool IsAllZeroes() const
    {
        for (int i = 0; i < RecordCount; i++)
            if (Records[i].ReadbackIndex != 0)
                return false;
        return true;
    }
};


//-------------------------------------------------------------------------------------
// ***** LatencyTest2
//
// LatencyTest2 utility class wraps the low level SensorDevice and manages the scheduling
// of a latency test. A single test is composed of a series of individual latency measurements
// which are used to derive min, max, and an average latency value.
//
// Developers are required to call the following methods:
//      SetDevice - Sets the SensorDevice to be used for the tests.
//      ProcessInputs - This should be called at the same place in the code where the game engine
//                      reads the headset orientation from LibOVR (typically done by calling
//                      'GetOrientation' on the SensorFusion object). Calling this at the right time
//                      enables us to measure the same latency that occurs for headset orientation
//                      changes.
//      DisplayScreenColor -    The latency tester works by sensing the color of the pixels directly
//                              beneath it. The color of these pixels can be set by drawing a small
//                              quad at the end of the rendering stage. The quad should be small
//                              such that it doesn't significantly impact the rendering of the scene,
//                              but large enough to be 'seen' by the sensor. See the SDK
//                              documentation for more information.
//		GetResultsString -	Call this to get a string containing the most recent results.
//							If the string has already been gotten then NULL will be returned.
//							The string pointer will remain valid until the next time this 
//							method is called.
//

class LatencyTest2 : public NewOverrideBase
{
public:
    LatencyTest2(SensorDevice* device = NULL);
    ~LatencyTest2();
    
    // Set the Latency Tester device that we'll use to send commands to and receive
    // notification messages from.
    bool        SetSensorDevice(SensorDevice* device);
    bool        SetDisplayDevice(LatencyTestDevice* device);

    // Returns true if this LatencyTestUtil has a Latency Tester device.
    bool        HasDisplayDevice() const { return LatencyTesterDev.GetPtr() != NULL; }
    bool        HasDevice() const        { return Handler.IsHandlerInstalled(); }

    bool        DisplayScreenColor(Color& colorToDisplay);
	//const char*	GetResultsString();

    // Begin test. Equivalent to pressing the button on the latency tester.
    void        BeginTest(double startTime = -1.0f);
    bool		IsMeasuringNow() const { return TestActive; }
    double      GetMeasuredLatency() const { return LatencyMeasuredInSeconds; }

//
    FrameTimeRecordSet GetLocklessState() { return LockessRecords.GetState(); }

private:
    LatencyTest2* getThis()  { return this; }

    enum LatencyTestMessageType
    {
        LatencyTest_None,
        LatencyTest_Timer,
        LatencyTest_ProcessInputs,
    };
    
    void handleMessage(const MessagePixelRead& msg);

    class PixelReadHandler : public MessageHandler
    {
        LatencyTest2*    pLatencyTestUtil;
    public:
        PixelReadHandler(LatencyTest2* latencyTester) : pLatencyTestUtil(latencyTester) { }
        ~PixelReadHandler();

        virtual void OnMessage(const Message& msg);
    };
    PixelReadHandler            Handler;
    
    Ptr<SensorDevice>           HmdDevice;
    Ptr<LatencyTestDevice>      LatencyTesterDev;
    
    Lock                        TesterLock;
    bool                        TestActive;
    unsigned char               RenderColorValue;
    MessagePixelRead            LastPixelReadMsg;
    double                      StartTiming;
    unsigned int                RawStartTiming;
    UInt32                      RawLatencyMeasured;
    double                      LatencyMeasuredInSeconds;
    int                         NumMsgsBeforeSettle;
    unsigned int                NumTestsSuccessful;

    // MA:
    // Frames are added here, then copied into lockess state
    FrameTimeRecordSet                  RecentFrameSet;
    LocklessUpdater<FrameTimeRecordSet> LockessRecords;
};



}} // namespace OVR::Util

#endif // OVR_Util_LatencyTest2_h
