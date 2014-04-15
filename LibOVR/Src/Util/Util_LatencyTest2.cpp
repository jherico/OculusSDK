/************************************************************************************

Filename    :   Util_LatencyTest2.cpp
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

#include "Util_LatencyTest2.h"

#include "../OVR_CAPI.h"
#include "../Kernel/OVR_Log.h"
#include "../Kernel/OVR_Timer.h"


namespace OVR { namespace Util {

//static const float      BIG_FLOAT = 1000000.0f;
//static const float      SMALL_FLOAT = -1000000.0f;

//-------------------------------------------------------------------------------------
// ***** LatencyTest2

LatencyTest2::LatencyTest2(SensorDevice* device)
 :  Handler(getThis())
 ,  TestActive(false)
 ,  StartTiming(-1)
 ,  LatencyMeasuredInSeconds(-1)
 ,  LastPixelReadMsg(NULL)
 ,  RenderColorValue(0)
 ,  NumMsgsBeforeSettle(0)
 ,  NumTestsSuccessful(0)
{
    if (device != NULL)
    {
        SetSensorDevice(device);
    }
}

LatencyTest2::~LatencyTest2()
{
    HmdDevice = NULL;
    LatencyTesterDev = NULL;
    
    Handler.RemoveHandlerFromDevices();
}

bool LatencyTest2::SetSensorDevice(SensorDevice* device)
{
    Lock::Locker devLocker(&TesterLock);

    // Enable/Disable pixel read from HMD
    if (device != HmdDevice)
    {
        Handler.RemoveHandlerFromDevices();

        HmdDevice = device;

        if (HmdDevice != NULL)
        {
            HmdDevice->AddMessageHandler(&Handler);
        }
    }

    return true;
}

bool LatencyTest2::SetDisplayDevice(LatencyTestDevice* device)
{
    Lock::Locker devLocker(&TesterLock);

    if (device != LatencyTesterDev)
    {
        LatencyTesterDev = device;
        if (LatencyTesterDev != NULL)
        {
            // Set display to initial (3 dashes).
            LatencyTestDisplay ltd(2, 0x40400040);
            LatencyTesterDev->SetDisplay(ltd);
        }
    }

    return true;
}

void LatencyTest2::BeginTest(double startTime)
{
    Lock::Locker devLocker(&TesterLock);

    if (!TestActive)
    {
        TestActive = true;
        NumMsgsBeforeSettle = 0;

        // Go to next pixel value
        //RenderColorValue = (RenderColorValue == 0) ? 255 : 0;
        RenderColorValue = (RenderColorValue + LT2_ColorIncrement) % 256;        
        RawStartTiming   = LastPixelReadMsg.RawSensorTime;
        
        if (startTime > 0.0)
            StartTiming = startTime;
        else
            StartTiming = ovr_GetTimeInSeconds();
          
    }
}

void LatencyTest2::handleMessage(const MessagePixelRead& msg)
{
    Lock::Locker devLocker(&TesterLock);

    // Hold onto the last message as we will use this when we start a new test
    LastPixelReadMsg = msg;

    // If color readback index is valid, store it in the lock-less queue.
    int readbackIndex = 0;
    if (FrameTimeRecord::ColorToReadbackIndex(&readbackIndex, msg.PixelReadValue))
    {
        RecentFrameSet.AddValue(readbackIndex, msg.FrameTimeSeconds);
        LockessRecords.SetState(RecentFrameSet);
    }

    NumMsgsBeforeSettle++;

    if (TestActive)
    {
        int pixelValueDiff = RenderColorValue - LastPixelReadMsg.PixelReadValue;
        int rawTimeDiff    = LastPixelReadMsg.RawFrameTime - RawStartTiming;

        if (pixelValueDiff < LT2_PixelTestThreshold && pixelValueDiff > -LT2_PixelTestThreshold)
        {
            TestActive = false;

            LatencyMeasuredInSeconds = LastPixelReadMsg.FrameTimeSeconds - StartTiming;
            RawLatencyMeasured       = rawTimeDiff;
            //LatencyMeasuredInSeconds = RawLatencyMeasured / 1000000.0;

            if(LatencyTesterDev && (NumTestsSuccessful % 5) == 0)
            {
                int displayNum = (int)(RawLatencyMeasured / 100.0);
                //int displayNum = NumMsgsBeforeSettle;
                //int displayNum = (int)(LatencyMeasuredInSeconds * 1000.0);
                LatencyTestDisplay ltd(1, displayNum);
                LatencyTesterDev->SetDisplay(ltd);
            }

            NumTestsSuccessful++;
        }
        else if (TestActive && (rawTimeDiff / 1000) > LT2_TimeoutWaitingForColorDetected)
        {
            TestActive = false;
            LatencyMeasuredInSeconds = -1;
        }
    }
}

LatencyTest2::PixelReadHandler::~PixelReadHandler()
{
    RemoveHandlerFromDevices();
}

void LatencyTest2::PixelReadHandler::OnMessage(const Message& msg)
{
    if(msg.Type == Message_PixelRead)
        pLatencyTestUtil->handleMessage(static_cast<const MessagePixelRead&>(msg));
}

bool LatencyTest2::DisplayScreenColor(Color& colorToDisplay)
{
    Lock::Locker devLocker(&TesterLock);
    colorToDisplay = Color(RenderColorValue, RenderColorValue, RenderColorValue, 255);

    return TestActive;
}

}} // namespace OVR::Util
