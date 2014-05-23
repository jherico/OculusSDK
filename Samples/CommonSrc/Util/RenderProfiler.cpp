/************************************************************************************

Filename    :   RenderProfiler.cpp
Content     :   Profiling for render.
Created     :   March 10, 2014
Authors     :   Caleb Leak

Copyright   :   Copyright 2012 Oculus VR, Inc. All Rights reserved.

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

#include "RenderProfiler.h"

using namespace OVR;

RenderProfiler::RenderProfiler()
{
    memset(SampleHistory, 0, sizeof(SampleHistory));
    memset(SampleAverage, 0, sizeof(SampleAverage));
    SampleCurrentFrame = 0;
}

void RenderProfiler::RecordSample(SampleType sampleType)
{
    if (sampleType == Sample_FrameStart)
    {
        // Recompute averages and subtract off frame start time.
        for (int sample = 1; sample < Sample_LAST; sample++)
        {
            SampleHistory[SampleCurrentFrame][sample] -= SampleHistory[SampleCurrentFrame][0];

            // Recompute the average for the current sample type.
            SampleAverage[sample] = 0.0;
            for (int frame = 0; frame < NumFramesOfTimerHistory; frame++)
            {
                SampleAverage[sample] += SampleHistory[frame][sample];
            }
            SampleAverage[sample] /= NumFramesOfTimerHistory;
        }

        SampleCurrentFrame = ((SampleCurrentFrame + 1) % NumFramesOfTimerHistory);
    }

    SampleHistory[SampleCurrentFrame][sampleType] = ovr_GetTimeInSeconds();
}

const double* RenderProfiler::GetLastSampleSet() const
{
    return SampleHistory[(SampleCurrentFrame - 1 + NumFramesOfTimerHistory) % NumFramesOfTimerHistory];
}

void RenderProfiler::DrawOverlay(RenderDevice* prender)
{
    char buf[256 * Sample_LAST];
    OVR_strcpy ( buf, sizeof(buf), "Timing stats" );     // No trailing \n is deliberate.

    /*int timerLastFrame = TimerCurrentFrame - 1;
    if ( timerLastFrame < 0 )
    {
        timerLastFrame = NumFramesOfTimerHistory - 1;
    }*/
    // Timer 0 is always the time at the start of the frame.

    const double* averages = GetAverages();
    const double* lastSampleSet = GetLastSampleSet();

    for ( int timerNum = 1; timerNum < Sample_LAST; timerNum++ )
    {
        char const *pName = "";
        switch ( timerNum )
        {
        case Sample_AfterGameProcessing:     pName = "AfterGameProcessing"; break;
        case Sample_AfterEyeRender     :     pName = "AfterEyeRender     "; break;
//        case Sample_BeforeDistortion   :     pName = "BeforeDistortion   "; break;
//        case Sample_AfterDistortion    :     pName = "AfterDistortion    "; break;
        case Sample_AfterPresent       :     pName = "AfterPresent       "; break;
//        case Sample_AfterFlush         :     pName = "AfterFlush         "; break;
        default: OVR_ASSERT ( false );
        }
        char bufTemp[256];
        OVR_sprintf ( bufTemp, sizeof(bufTemp), "\nRaw: %.2lfms\t400Ave: %.2lfms\t800%s",
                        lastSampleSet[timerNum] * 1000.0, averages[timerNum] * 1000.0, pName );
        OVR_strcat ( buf, sizeof(buf), bufTemp );
    }

    DrawTextBox(prender, 0.0f, 0.0f, 22.0f, buf, DrawText_Center);
}