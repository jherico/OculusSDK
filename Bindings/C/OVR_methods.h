/************************************************************************************

Copyright 2013 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

*************************************************************************************/

#pragma once

#ifndef OVR_HEADER
#error "Don't include OVR_methods.h directly.  Include OculusVR.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// TODO more comprehensive Rift location and opening, support for multiple rifts
//OVR_HANDLE ovrOpenRift(char * riftId);
//// Search for connected Oculus Rift devices, and populate the passed OvrRiftInfo
//// with the information for the first found rift
//void ovrFindFirstRift(OvrRiftInfo * out);
//void ovrFindNextRift(OvrRiftInfo * out);
// TODO setting and getting range
//// Sets maximum range settings for the sensor described by SensorRange.
//// The function will fail if you try to pass values outside maximum supported
//// by the HW, as described by ovrGetTrackerMaxRange.
//// Pass waitFlag == true to wait for command completion. For waitFlag == true,
//// returns true if the range was applied successfully (no HW error).
//// For waitFlag = false, return 'true' means that command was enqueued successfully.
//void ovrSetTrackerRange(OVR_HANDLE device, const OvrTrackerRange * in);
//
//// Return the current sensor range settings for the device. These may not exactly
//// match the values applied through SetRange.
//void ovrGetTrackerRange(OVR_HANDLE device, OvrTrackerRange * out);
//
//// Return the maximum sensor range settings for the device.
//void ovrGetTrackerMaxRange(OVR_HANDLE device, OvrTrackerRange * out);


// A convenience method to open the first connected Rift.  This will iterate across all
// of the rift devices until it finds one it can open.
OVR_HANDLE ovrOpenFirstAvailableRift();
OVR_HANDLE ovrOpenRiftRecording(char * recordingFile);
void ovrCloseRift(OVR_HANDLE device);

// Sets report rate (in Hz) of OvrTrackerMessage messages
// Currently supported maximum rate is 1000Hz.
// If the rate is set to 500 or 333 Hz then the callback will be invoked twice
// or thrice at the same 'tick'.
// If the rate is  < 333 then the OnMessage / MessageBodyFrame will be called three
// times for each 'tick': the first call will contain averaged values, the second
// and third calls will provide with most recent two recorded samples.
void ovrSetTrackerRate(OVR_HANDLE device, uint32_t hertz);

// Returns currently set report rate, in Hz. If 0 - error occurred.
// Note, this value may be different from the one provided for SetReportRate. The return
// value will contain the actual rate.
uint32_t ovrGetTrackerRate(OVR_HANDLE device);

void ovrGetDisplayInfo(OVR_HANDLE device, OvrDisplayInfo * out);

OVR_SENSOR_CALLBACK ovrRegisterSampleHandler(OVR_HANDLE device, OVR_SENSOR_CALLBACK callback);

#ifdef __cplusplus
}
#endif
