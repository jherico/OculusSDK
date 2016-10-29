/************************************************************************************
Filename    :   Win32_RoomTiny_Main.cpp
Content     :   First-person view test application for Oculus Rift
Created     :   20th July 2016
Authors     :   Tom Heath
Copyright   :   Copyright 2012 Oculus, Inc. All Rights reserved.

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
/// A sample to show vibration generation, using buffered input.
/// Press X to generate vibration in the Left Touch controller.
/// Note - the Touch controller is not graphically displayed, 
/// to keep the sample minimal.

#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"        // Basic VR

struct BufferedHaptics : BasicVR
{
	BufferedHaptics(HINSTANCE hinst) : BasicVR(hinst, L"BufferedHaptics") {}

	void MainLoop()
	{
		Layer[0] = new VRLayer(Session);

		// We are going to make a HAPTIC buffer 
		int bufferSize = 256;
		unsigned char * dataBuffer = (unsigned char *)malloc(bufferSize);

		// Check with the SDK that the buffer format is as we expect
		ovrTouchHapticsDesc desc = ovr_GetTouchHapticsDesc(Session, ovrControllerType_LTouch);
		if (desc.SampleSizeInBytes != 1)        FATALERROR("Our assumption of 1 byte per element, is no longer valid"); 
		if (desc.SubmitMaxSamples < bufferSize) FATALERROR("Can't handle this many samples");
	
		// Fill the buffer with a sine wave rising and falling over the duration,
		// and with a lowered effective frequency in latter half by setting alternate intensities to zero
		for (int i = 0; i < bufferSize; i++)
		{
			dataBuffer[i] = (unsigned char)(255.0f*(sin(((3.14159265359f*i) / ((float)bufferSize)))));
			if ((i > bufferSize / 2) && (i % 2)) dataBuffer[i] = 0;
		}

		// Finally, make an SDK structure containing our buffer,
		// and we are ready to submit it anytime we are ready to 'play it'
		ovrHapticsBuffer buffer;
		buffer.SubmitMode = ovrHapticsBufferSubmit_Enqueue;
		buffer.SamplesCount = bufferSize;
		buffer.Samples = (void *)dataBuffer;

		// Main loop
		while (HandleMessages())
		{
			Layer[0]->GetEyePoses();

			// Submit the haptic buffer to 'play' upon pressing X button 
			ovrInputState inputState;
			ovr_GetInputState(Session, ovrControllerType_Touch, &inputState);
			if (inputState.Buttons & ovrTouch_X)
			{
				// Only submit the buffer if there is enough space available
				ovrHapticsPlaybackState playbackState;
				ovrResult result = ovr_GetControllerVibrationState(Session, ovrControllerType_LTouch, &playbackState);
				if (playbackState.RemainingQueueSpace >= bufferSize)
				{
					ovr_SubmitControllerVibration(Session, ovrControllerType_LTouch, &buffer);
				}
			}

			// Render just a standard scene in the HMD
			for (int eye = 0; eye < 2; ++eye)
			{
				XMMATRIX viewProj = Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye);
			}

			Layer[0]->PrepareLayerHeader();
			DistortAndPresent(1);
		}

		free(dataBuffer);
	}
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	BufferedHaptics app(hinst);
	return app.Run();
}
