/************************************************************************************

PublicHeader:   n/a
Filename    :   Tracing.h
Content     :   Performance tracing
Created     :   December 4, 2014
Author      :   Ed Hutchins

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

************************************************************************************/

#ifndef OVR_Tracing_h
#define OVR_Tracing_h

//-----------------------------------------------------------------------------------
// ***** OVR_ENABLE_ETW_TRACING definition (XXX default to on for windows builds?)
//

#ifdef OVR_OS_WIN32
#define OVR_ENABLE_ETW_TRACING
#endif

//-----------------------------------------------------------------------------------
// ***** Trace* definitions
//

#ifdef OVR_ENABLE_ETW_TRACING

    #define TracingIsEnabled() (OVR_SDK_LibOVREnableBits[0] != 0)

    #ifdef TRACE_STATE_CAPTURE_FUNC
        // hook in our own state capture callback to record the state of all opened HMDs (supress unused parameter warnings with void() casts)
        #define MCGEN_PRIVATE_ENABLE_CALLBACK_V2(SourceId, ControlCode, Level, MatchAnyKeyword, MatchAllKeywords, FilterData, CallbackContext) \
        ( \
            void(SourceId), \
            void(Level), \
            void(MatchAnyKeyword), \
            void(MatchAllKeywords), \
            void(FilterData), \
            void(CallbackContext), \
            (((ControlCode) == EVENT_CONTROL_CODE_CAPTURE_STATE) ? (TRACE_STATE_CAPTURE_FUNC) : 0) \
        )
    #endif

	#if !defined(_In_reads_)
		// get VS2010 working
		#define _In_reads_(x)
	#endif

    #include "LibOVREvents.h"

    // Register/Unregister the OVR_SDK_LibOVR provider with ETW
    // (MCGEN_PRIVATE_ENABLE_CALLBACK_V2 hooks in our state capture)
    #define TraceInit() \
    do { \
        ULONG status = EventRegisterOVR_SDK_LibOVR(); \
        if (ERROR_SUCCESS != status) { \
            LogError("[LibOVR] Failed to register ETW provider (%ul)", status); \
        } \
    } while (0)
    #define TraceFini() EventUnregisterOVR_SDK_LibOVR()

    // Trace function call and return for perf, and waypoints for debug
    #define TraceCall(frameIndex) EventWriteCall(__FUNCTIONW__, __LINE__, (frameIndex))
    #define TraceReturn(frameIndex) EventWriteReturn(__FUNCTIONW__, __LINE__, (frameIndex))
    #define TraceWaypoint(frameIndex) EventWriteWaypoint(__FUNCTIONW__, __LINE__, (frameIndex))

    // DistortionRenderer events
    #define TraceDistortionBegin(id, frameIndex) EventWriteDistortionBegin((id), (frameIndex))
    #define TraceDistortionWaitGPU(id, frameIndex) EventWriteDistortionWaitGPU((id), (frameIndex))
    #define TraceDistortionPresent(id, frameIndex) EventWriteDistortionPresent((id), (frameIndex))
    #define TraceDistortionEnd(id, frameIndex) EventWriteDistortionEnd((id), (frameIndex))

    // Tracking Camera events
    #define _TraceCameraFrameData(fn,img) \
        fn( \
            0, \
            (img).FrameNumber, \
            (img).ArrivalTime, \
            (img).CaptureTime, \
            0 \
          )
    #define TraceCameraFrameReceived(img) _TraceCameraFrameData(EventWriteCameraFrameReceived,(img))
    #define TraceCameraBeginProcessing(img) _TraceCameraFrameData(EventWriteCameraBeginProcessing,(img))
    #define TraceCameraFrameRequest(requestNumber, frameCount, lastFrameNumber) EventWriteCameraFrameRequest(requestNumber, frameCount, lastFrameNumber)
    #define TraceCameraEndProcessing(img) _TraceCameraFrameData(EventWriteCameraEndProcessing,(img))
    #define TraceCameraSkippedFrames(requestNumber, frameCount, lastFrameNumber) EventWriteCameraSkippedFrames(requestNumber, frameCount, lastFrameNumber)

    // Trace the interesting parts of an ovrHmdDesc structure
    #define TraceHmdDesc(desc) \
        EventWriteHmdDesc( \
            (desc).Type, \
            (desc).VendorId, \
            (desc).ProductId, \
            (desc).SerialNumber, \
            (desc).FirmwareMajor, \
            (desc).FirmwareMinor, \
            (desc).HmdCaps, \
            (desc).TrackingCaps, \
            (desc).DistortionCaps, \
            (desc).Resolution.w, \
            (desc).Resolution.h \
        )

    // Trace part of a JSON string (events have a 64k limit)
    #define TraceJSONChunk(Name, TotalChunks, ChunkSequence, TotalSize, ChunkSize, ChunkOffset, Chunk) \
        EventWriteJSONChunk(Name, TotalChunks, ChunkSequence, TotalSize, ChunkSize, ChunkOffset, Chunk)

    // Trace messages from the public ovr_Trace API and our internal logger
    #define TraceLogDebug(message) EventWriteLogDebugMessage(message)
    #define TraceLogInfo(message) EventWriteLogInfoMessage(message)
    #define TraceLogError(message) EventWriteLogErrorMessage(message)

    // Trace an ovrTrackingState
    #define TraceTrackingState(ts) \
        EventWriteHmdTrackingState( \
            (ts).HeadPose.TimeInSeconds, \
            &(ts).HeadPose.ThePose.Orientation.x, \
            &(ts).HeadPose.ThePose.Position.x, \
            &(ts).HeadPose.AngularVelocity.x, \
            &(ts).HeadPose.LinearVelocity.x, \
            &(ts).CameraPose.Orientation.x, \
            &(ts).CameraPose.Position.x, \
            &(ts).RawSensorData.Accelerometer.x, \
            &(ts).RawSensorData.Gyro.x, \
            &(ts).RawSensorData.Magnetometer.x, \
            (ts).RawSensorData.Temperature, \
            (ts).RawSensorData.TimeInSeconds, \
            (ts).StatusFlags, \
            (ts).LastCameraFrameCounter \
        )

    #define TraceCameraBlobs(blobs) \
        if (EventEnabledCameraBlobs()) \
        { \
            const int max_blobs = 80; \
            int count = (blobs).GetSizeI(); \
            double x[max_blobs]; \
            double y[max_blobs]; \
            int size[max_blobs]; \
            if (count > max_blobs) \
                count = max_blobs; \
            for (int i = 0; i < count; ++i) \
            { \
                x[i] = (blobs)[i].Position.x; \
                y[i] = (blobs)[i].Position.y; \
                size[i] = (blobs)[i].BlobSize; \
            } \
            EventWriteCameraBlobs(count, x, y, size); \
        } \
        else ((void)0)

#else // OVR_ENABLE_ETW_TRACING

    // Eventually other platforms could support their form of performance tracing
    #define TracingIsEnabled() (false)
    #define TraceInit() ((void)0)
    #define TraceFini() ((void)0)
    #define TraceCall(frameIndex) ((void)0)
    #define TraceReturn(frameIndex) ((void)0)
    #define TraceWaypoint(frameIndex) ((void)0)
    #define TraceDistortionBegin(id, frameIndex) ((void)0)
    #define TraceDistortionWaitGPU(id, frameIndex) ((void)0)
    #define TraceDistortionPresent(id, frameIndex) ((void)0)
    #define TraceDistortionEnd(id, frameIndex) ((void)0)
    #define TraceCameraFrameReceived(cfd) ((void)0)
    #define TraceCameraBeginProcessing(cfd) ((void)0)
    #define TraceCameraFrameRequest(requestNumber, frameCount, lastFrameNumber) ((void)0)
    #define TraceCameraEndProcessing(cfd) ((void)0)
    #define TraceCameraSkippedFrames(requestNumber, frameCount, lastFrameNumber) ((void)0)
    #define TraceHmdDesc(desc) ((void)0)
    #define TraceJSONChunk(Name, TotalChunks, ChunkSequence, TotalSize, ChunkSize, ChunkOffset, Chunk) ((void)0)
    #define TraceLogDebug(message) ((void)0)
    #define TraceLogInfo(message) ((void)0)
    #define TraceLogError(message) ((void)0)
    #define TraceTrackingState(ts) ((void)0)
    #define TraceCameraBlobs(blobs) ((void)0)

#endif // OVR_ENABLE_ETW_TRACING

#endif // OVR_Tracing_h
