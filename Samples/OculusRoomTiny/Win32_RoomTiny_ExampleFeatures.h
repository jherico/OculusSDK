/************************************************************************************
Filename    :   Win32_RoomTiny_ExampleFeatures.h
Content     :   First-person view test application for Oculus Rift
Created     :   October 20th, 2014
Author      :   Tom Heath
Copyright   :   Copyright 2014 Oculus, Inc. All Rights reserved.
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

// Note, these options may not work in combination, 
//       and may not apply to both SDK-rendered and APP-rendered

int clock = 0; 

#if !SDK_RENDER
// Part 1 of 5 - Stereo-matching one-eye-per-frame.
// We render only one eye per frame, employing a 3rd buffer, so we can wait until both frames 
// stereoscopically match before presenting them, timewarped to the user.  
// We do this by having 2 buffers for the left eye, so we can hang onto an older version.
// Operate with the 'M' key.
// Non SDK-rendered only.
ImageBuffer * extraEyeRenderTexture; 
ovrPosef      extraRenderPose; 
float         extraYaw;   
ShaderFill  * extraShaderFill;

//Used by some features
void MakeNewDistortionMeshes(float overrideEyeRelief=0);
#endif

//-----------------------------------------------------------------------------------------------------
void ExampleFeatures1(float * pSpeed, int * pTimesToRenderScene, ovrVector3f * useHmdToEyeViewOffset)
{
    // Update the clock, used by some of the features
    clock++;

	// Recenter the Rift by pressing 'R'
    if (DX11.Key['R'])
        ovrHmd_RecenterPose(HMD);

    // Toggle to monoscopic by holding the 'I' key,
    // to recognise the pitfalls of no stereoscopic viewing, how easy it
    // is to get this wrong, and displaying the method to manually adjust.
    if (DX11.Key['I'])
	{
		useHmdToEyeViewOffset[0].x = 0; // This value would normally be half the IPD,
		useHmdToEyeViewOffset[1].x = 0; //  received from the loaded profile. 
	}

#if SDK_RENDER
    OVR_UNUSED(pSpeed);
    OVR_UNUSED(pTimesToRenderScene);

    // Dismiss the Health and Safety message by pressing any key
    if (DX11.IsAnyKeyPressed())
        ovrHmd_DismissHSWDisplay(HMD);

#else
    // Shows the range of eye relief possible from the config tool, and how to 
    // live adjust them in an application.  Use keys '1' and '2'.
    // Note that the distortion meshes need to be recreated when this is adjusted,
	// hence not currently a realtime switch.
    // Non SDK-rendered only.
	// TBD - an example of reverting the eye relief back to the profile value 
    if (DX11.Key['1']) MakeNewDistortionMeshes(0.001f); // Min eye relief
    if (DX11.Key['2']) MakeNewDistortionMeshes(1.000f); // Max eye relief


    // Pressing '8' shows a method for varying FOV, and also underlines how FOV varies
    // Note that the distortion meshes need to be recreated when this is adjusted,
	// hence not currently a realtime switch.
    // Non SDK-rendered only.
    if (DX11.Key['8']) 
    {
        EyeRenderDesc[0].Fov.UpTan   = HMD->DefaultEyeFov[0].UpTan   + 0.2f*sin(0.20f*clock);
        EyeRenderDesc[0].Fov.DownTan = HMD->DefaultEyeFov[0].DownTan + 0.2f*sin(0.16f*clock);
        EyeRenderDesc[1].Fov.UpTan   = HMD->DefaultEyeFov[1].UpTan   + 0.2f*sin(0.20f*clock);
        EyeRenderDesc[1].Fov.DownTan = HMD->DefaultEyeFov[1].DownTan + 0.2f*sin(0.16f*clock);
        MakeNewDistortionMeshes();
    }

    // PART 1 of 2.  GPU/CPU parallelism
    // *** Not currently supported with Direct Mode*** - Extended mode only.
    // This allows the GPU and CPU to operate in parallel,
    // rather than the CPU waiting for GPU before the end of a frame.
    // Currently there is a downside that this adds a frame of latency. 
    // To test operation, increase TimesToDrawScene until can't maintain framerate.
    // The current framerate is shown in output window.
    // Then press 'H' to see if smooth as parallelism kicks in 
    // Non SDK-rendered only.
	// Vary this load to demonstrate (dependent on hardware), and might want to modify 
	// the value in the main file as well, for a like-for-like comparison.
	// The default value below is for high-end hardware. 
    if (DX11.Key['H'])
        *pTimesToRenderScene = 875; // Vary this load to demonstrate (dependent on hardware);

    // Part 2 of 5 - Stereo-matching one-eye-per-frame.
    if (DX11.Key['M'])                         *pSpeed *= 2;    
    if (((clock % 2) != 0) && (DX11.Key['M'])) *pSpeed = 0;
#endif
}

//---------------------------------------------------------------------------------------------
void ExampleFeatures2(int eye, ImageBuffer ** pUseBuffer, ovrPosef ** pUseEyePose, float ** pUseYaw,         
                      bool * pClearEyeImage,bool * pUpdateEyeImage)
{
    // A debug function that allows the pressing of 'F' to freeze/cease the generation of any new eye
    // buffers, therefore showing the independent operation of timewarp. 
    // Recommended for your applications
    if (DX11.Key['F'])
        *pClearEyeImage = *pUpdateEyeImage = false;                        

    // This illustrates how the SDK allows the developer to vary the eye buffer resolution 
    // in realtime.  Adjust with the '9' key.
    if (DX11.Key['9']) 
        EyeRenderViewport[eye].Size.h = (int)(pEyeRenderTexture[eye]->Size.h*(2+sin(0.1f*clock))/3.0f);

     // Press 'N' to simulate if, instead of rendering frames, exhibit blank frames
    // in order to guarantee frame rate.   Not recommended at all, but useful to see,
    // just in case some might consider it a viable alternative to juddering frames.
    const int BLANK_FREQUENCY = 10;
    if ((DX11.Key['N']) && ((clock % (BLANK_FREQUENCY*2)) == (eye*BLANK_FREQUENCY)))
        *pUpdateEyeImage = false;

#if SDK_RENDER
    OVR_UNUSED3(pUseYaw, pUseEyePose, pUseBuffer);
#else

    // A simple technique for reducing the burden on your app, by rendering only one eye per frame.
    // It may be applicable to some applications, in some circumstances, but not to all.
    // It highlights the method and also you can see the associated artifacts on the floor,
    // and local juddering on the moving cube.
    // Note that timewarp is extended to make the user yaw smooth.
    // Note there is likely detriment from apparent IPD variation when strafing.
    // Operate with the 'G' key.
    // Non SDK-rendered only.
    if ((DX11.Key['G']) && ((clock & 1) == eye))
        *pClearEyeImage = *pUpdateEyeImage = false;  

    // Part 3 of 5 - Stereo-matching one-eye-per-frame.
    if (DX11.Key['M'])
    {
        if ((clock & 1) != eye)
            *pClearEyeImage = *pUpdateEyeImage = false; 
        if (((clock % 4) == 2) && (eye == 0))
        { 
            *pUseBuffer  = extraEyeRenderTexture;
            *pUseEyePose = &extraRenderPose;
            *pUseYaw     = &extraYaw;
        }
    }
    #endif
}


#if !SDK_RENDER

//-----------------------------------------------------------------------------------------
void ExampleFeatures3(D3D11_INPUT_ELEMENT_DESC * VertexDesc,int numVertexDesc,
                      char* vertexShader, char* pixelShader)
{
    // Part 4 of 5 - Stereo-matching one-eye-per-frame.
    Sizei idealTexSize    = ovrHmd_GetFovTextureSize(HMD, (ovrEyeType)0, HMD->DefaultEyeFov[0], 1.0f); 
    extraEyeRenderTexture = new ImageBuffer(true, false, idealTexSize);
    extraShaderFill       = new ShaderFill(VertexDesc, numVertexDesc, vertexShader, pixelShader,
                                           extraEyeRenderTexture, false);
}

//-----------------------------------------------------------------------------------------------------
void ExampleFeatures4(int eye, ShaderFill ** pUseShaderfill,ovrPosef ** pUseEyePose,
                      float ** pUseYaw, double * pDebugTimeAdjuster,bool * pWaitForGPU)
{
    // Part 5 of 5 - Stereo-matching one-eye-per-frame.
    if ((DX11.Key['M']) && (((clock % 4) == 0) || ((clock % 4) == 3)) && (eye == 0))   
    {
        *pUseEyePose    = &extraRenderPose;
        *pUseYaw        = &extraYaw;
        *pUseShaderfill = extraShaderFill;
    }

    // Adjustng the timing in order to display and recognise the detrimental effects of incorrect timing,
    // and for perhaps correcting timing temporarily on less supported hardware.
    // Non SDK-rendered only.
    if (DX11.Key['4']) *pDebugTimeAdjuster = -0.026; // Greatly underpredicting
    if (DX11.Key['5']) *pDebugTimeAdjuster = -0.006; // Slightly underpredicting
    if (DX11.Key['6']) *pDebugTimeAdjuster = +0.006; // Slightly overpredicting
    if (DX11.Key['7']) *pDebugTimeAdjuster = +0.026; // Greatly overpredicting

    // PART 2 of 2.  GPU/CPU parallelism
    if (DX11.Key['H'])
        *pWaitForGPU = false;
}

#endif
