/************************************************************************************

Filename    :   OculusWorldDemo.cpp
Content     :   First-person view test application for Oculus Rift - Implementation
Created     :   October 4, 2012
Authors     :   Michael Antonov, Andrew Reisse, Steve LaValle, Dov Katz
				Peter Hoff, Dan Goodman, Bryan Croteau                

Copyright   :   Copyright 2012 Oculus VR, LLC. All Rights reserved.

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

#include "OculusWorldDemo.h"
#include "Kernel/OVR_Threads.h"
#include "Util/Util_SystemGUI.h"


OVR_DISABLE_MSVC_WARNING(4996) // "scanf may be unsafe"

//-------------------------------------------------------------------------------------
// ***** OculusWorldDemoApp

OculusWorldDemoApp::OculusWorldDemoApp() :
    pRender(0),
    RenderParams(),
    WindowSize(1280,800),
    ScreenNumber(0),
    FirstScreenInCycle(0),
    SupportsSrgb(true),             // May be proven false below.
    SupportsMultisampling(true),    // May be proven false below.

    LastVisionProcessingTime(0.),
    VisionTimesCount(0),
    VisionProcessingSum(0.),
    VisionProcessingAverage(0.),

  //RenderTargets()
  //MsaaRenderTargets()
    DrawEyeTargets(NULL),
    Hmd(0),
  //EyeRenderDesc[2];
  //Projection[2];          // Projection matrix for eye.
  //OrthoProjection[2];     // Projection for 2D.
  //EyeRenderPose[2];       // Poses we used for rendering.
  //EyeTexture[2];
  //EyeRenderSize[2];       // Saved render eye sizes; base for dynamic sizing.
    StartTrackingCaps(0),
    UsingDebugHmd(false),

    FrameCounter(0),
	TotalFrameCounter(0),
    SecondsPerFrame(0.f),
    FPS(0.f),
    LastFpsUpdate(0.0),
    LastUpdate(0.0),

    MainFilePath(),
    CollisionModels(),
    GroundCollisionModels(),

    LoadingState(LoadingState_Frame0),
    HaveVisionTracking(false),
    HavePositionTracker(false),
    HaveHMDConnected(false),

    LastGamepadState(),

    ThePlayer(),
    View(),
    MainScene(),
    LoadingScene(),
    SmallGreenCube(),

	OculusCubesScene(),
	RedCubesScene(),
	BlueCubesScene(),

    HmdFrameTiming(),
    HmdStatus(0),

    NotificationTimeout(0.0),

    HmdSettingsChanged(false),

    RendertargetIsSharedByBothEyes(false),
    DynamicRezScalingEnabled(false),
	EnableSensor(true),
    MonoscopicRender(false),
    PositionTrackingScale(1.0f),
    ScaleAffectsEyeHeight(false),
    DesiredPixelDensity(1.0f),
    FovSideTanMax(1.0f), // Updated based on Hmd.
    FovSideTanLimit(1.0f), // Updated based on Hmd.
    FadedBorder(true),

    TimewarpEnabled(true),
    TimewarpNoJitEnabled(false),
    TimewarpRenderIntervalInSeconds(0.0f),
    FreezeEyeUpdate(false),
    FreezeEyeOneFrameRendered(false),
    ComputeShaderEnabled(false),

    CenterPupilDepthMeters(0.05f),
    ForceZeroHeadMovement(false),
    VsyncEnabled(true),
    MultisampleEnabled(true),
#if defined(OVR_OS_LINUX)
    LinuxFullscreenOnDevice(false),
#endif
    IsLowPersistence(true),
    DynamicPrediction(true),
    DisplaySleep(false),
    PositionTrackingEnabled(true),
	PixelLuminanceOverdrive(true),
    HqAaDistortion(true),
    MirrorToWindow(true),

    DistortionClearBlue(0),
    ShiftDown(false),
    CtrlDown(false),
    IsLogging(false),

    SceneMode(Scene_World),
    GridDisplayMode(GridDisplay_None),
    GridMode(Grid_Lens),
    TextScreen(Text_None),
    BlocksShowType(0),
    BlocksCenter(0.0f, 0.0f, 0.0f),
    Menu(),
    Profiler(),
    ExceptionHandler()
{
    EyeRenderSize[0] = EyeRenderSize[1] = Sizei(0);

    // EyeRenderDesc[], EyeTexture[] : Initialized in CalculateHmdValues()
}

OculusWorldDemoApp::~OculusWorldDemoApp()
{
    CleanupDrawTextFont();

    if (Hmd)
    {
        ovrHmd_Destroy(Hmd);
        Hmd = 0;
    }
	    
	CollisionModels.ClearAndRelease();
	GroundCollisionModels.ClearAndRelease();

    ovr_Shutdown();
}



int OculusWorldDemoApp::OnStartup(int argc, const char** argv)
{
    OVR::Thread::SetCurrentThreadName("OWDMain");

    // *** Setup exception handler

    ExceptionHandler.SetExceptionListener(this, 0);
    ExceptionHandler.SetExceptionPaths("default", "default");
    ExceptionHandler.EnableReportPrivacy(false); // If we were collecting these reports then we need to get user permission in order to enable disable privacy.
    ExceptionHandler.Enable(true);
    

    // *** Oculus HMD & Sensor Initialization

    // Create DeviceManager and first available HMDDevice from it.
    // Sensor object is created from the HMD, to ensure that it is on the
    // correct device.

    #if defined(OVR_OS_WIN32)
        OVR::Thread::SetCurrentPriority(Thread::HighestPriority);
    
        if(OVR::Thread::GetCPUCount() >= 4) // Don't do this unless there are at least 4 processors, otherwise the process could hog the machine.
        {
            SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
        }
    #endif

    ovr_Initialize();

	Hmd = ovrHmd_Create(0);

	if (!Hmd)
	{
        Menu.SetPopupMessage("Unable to create HMD: %s", ovrHmd_GetLastError(NULL));

		// If we didn't detect an Hmd, create a simulated one for debugging.
		Hmd = ovrHmd_CreateDebug(ovrHmd_DK2);
		UsingDebugHmd = true;
		if (!Hmd)
		{   // Failed Hmd creation.
			return 1;
		}
	}
    
    if (Hmd->HmdCaps & ovrHmdCap_ExtendDesktop)
    {
        WindowSize = Hmd->Resolution;
    }
    else
    {
        // In Direct App-rendered mode, we can use smaller window size,
        // as it can have its own contents and isn't tied to the buffer.
        WindowSize = Sizei(1100, 618);//Sizei(960, 540); avoid rotated output bug.
    }


    // ***** Setup System Window & rendering.

    if (!SetupWindowAndRendering(argc, argv))
    {
        return 1;
    }

    NotificationTimeout = ovr_GetTimeInSeconds() + 10.0f;

    // Initialize FovSideTanMax, which allows us to change all Fov sides at once - Fov
    // starts at default and is clamped to this value.
    FovSideTanLimit = FovPort::Max(Hmd->MaxEyeFov[0], Hmd->MaxEyeFov[1]).GetMaxSideTan();
    FovSideTanMax   = FovPort::Max(Hmd->DefaultEyeFov[0], Hmd->DefaultEyeFov[1]).GetMaxSideTan();

    PositionTrackingEnabled = (Hmd->TrackingCaps & ovrTrackingCap_Position) ? true : false;

	PixelLuminanceOverdrive = (Hmd->DistortionCaps & ovrDistortionCap_Overdrive) ? true : false;

    HqAaDistortion = (Hmd->DistortionCaps & ovrDistortionCap_HqDistortion) ? true : false;

    // *** Configure HMD Stereo settings.
    
    CalculateHmdValues();

    // Query eye height.
    ThePlayer.UserEyeHeight = ovrHmd_GetFloat(Hmd, OVR_KEY_EYE_HEIGHT, ThePlayer.UserEyeHeight);
    ThePlayer.BodyPos.y     = ThePlayer.UserEyeHeight;
    // Center pupil for customization; real game shouldn't need to adjust this.
    CenterPupilDepthMeters  = ovrHmd_GetFloat(Hmd, "CenterPupilDepth", 0.0f);


    ThePlayer.bMotionRelativeToBody = false;  // Default to head-steering for DK1

    if (UsingDebugHmd)
        Menu.SetPopupMessage("NO HMD DETECTED");
    else if (!(ovrHmd_GetTrackingState(Hmd, 0.0f).StatusFlags & ovrStatus_HmdConnected))
        Menu.SetPopupMessage("NO SENSOR DETECTED");
    else if (Hmd->HmdCaps & ovrHmdCap_ExtendDesktop)
        Menu.SetPopupMessage("Press F9 for Full-Screen on Rift");
	else
		Menu.SetPopupMessage("Please put on Rift");

    // Give first message 10 sec timeout, add border lines.
    Menu.SetPopupTimeout(10.0f, true);

    PopulateOptionMenu();

    // *** Identify Scene File & Prepare for Loading

    InitMainFilePath();  
    PopulatePreloadScene();

    LastUpdate = ovr_GetTimeInSeconds();

    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "-StartPerfLog") && i < argc - 1)
        {
            ovrHmd_StartPerfLog(Hmd, argv[i + 1], 0);
        }
    }

    return 0;
}


bool OculusWorldDemoApp::SetupWindowAndRendering(int argc, const char** argv)
{
    // *** Window creation

	void* windowHandle = pPlatform->SetupWindow(WindowSize.w, WindowSize.h);

	if(!windowHandle)
        return false;
    
	ovrHmd_AttachToWindow( Hmd, windowHandle, NULL, NULL );

    // Report relative mouse motion in OnMouseMove
    pPlatform->SetMouseMode(Mouse_Relative);

    // *** Initialize Rendering

    #if defined(OVR_OS_MS)
        const char* graphics = "d3d11";  //Default to DX11. Can be overridden below.
    #else
        const char* graphics = "GL";
    #endif

    // Select renderer based on command line arguments.
    // Example usage: App.exe -r d3d9 -fs
    // Example usage: App.exe -r GL
    // Example usage: App.exe -r GL -GLVersion 4.1 -GLCoreProfile -DebugEnabled 
    for(int i = 1; i < argc; i++)
    {
        const bool lastArg = (i == (argc - 1)); // False if there are any more arguments after this.
        
        if(!OVR_stricmp(argv[i], "-r") && !lastArg)  // Example: -r GL
        {
            graphics = argv[++i];
        }
        else if(!OVR_stricmp(argv[i], "-fs"))        // Example: -fs
        {
            RenderParams.Fullscreen = 1;
        }
        else if(!OVR_stricmp(argv[i], "-MultisampleDisabled")) // Example: -MultisampleDisabled
        {
            MultisampleEnabled = false;
        }
        else if(!OVR_stricmp(argv[i], "-DebugEnabled")) // Example: -DebugEnabled
        {
            RenderParams.DebugEnabled = true;
        }
        else if(!OVR_stricmp(argv[i], "-GLVersion") && !lastArg) // Example: -GLVersion 3.2
        {
            const char* version = argv[++i];
            sscanf(version, "%d.%d", &RenderParams.GLMajorVersion, &RenderParams.GLMinorVersion);
        }
        else if(!OVR_stricmp(argv[i], "-GLCoreProfile")) // Example: -GLCoreProfile
        {
            RenderParams.GLCoreProfile = true;
        }
        else if(!OVR_stricmp(argv[i], "-GLCoreCompatibilityProfile")) // Example: -GLCompatibilityProfile
        {
            RenderParams.GLCompatibilityProfile = true;
        }
        else if(!OVR_stricmp(argv[i], "-GLForwardCompatibleProfile")) // Example: -GLForwardCompatibleProfile
        {
            RenderParams.GLForwardCompatibleProfile = true;
        }
    }

    // Setup RenderParams.RenderAPIType
    if (OVR_stricmp(graphics, "GL") == 0)
        RenderParams.RenderAPIType = ovrRenderAPI_OpenGL;
    #if defined(OVR_OS_MS)
        else if (OVR_stricmp(graphics, "d3d9") == 0)
            RenderParams.RenderAPIType = ovrRenderAPI_D3D9;
        else if (OVR_stricmp(graphics, "d3d10") == 0)
            RenderParams.RenderAPIType = ovrRenderAPI_D3D10;
        else if (OVR_stricmp(graphics, "d3d11") == 0)
            RenderParams.RenderAPIType = ovrRenderAPI_D3D11;
    #endif

    StringBuffer title;
    title.AppendFormat("Oculus World Demo %s : %s", graphics, Hmd->ProductName[0] ? Hmd->ProductName : "<unknown device>");
    pPlatform->SetWindowTitle(title);

    if ((RenderParams.RenderAPIType == ovrRenderAPI_OpenGL) && !(Hmd->HmdCaps & ovrHmdCap_ExtendDesktop))
        SupportsSrgb = false;

    // Ideally we would use the created OpenGL context to determine multisamping support,
    // but that creates something of a chicken-and-egg problem which is easier to solve in
    // practice by the code below, as it's the only case where multisamping isn't supported
    // in modern computers of interest to us.
    #if defined(OVR_OS_MAC)
        if (RenderParams.GLMajorVersion < 3)
            SupportsMultisampling = false;
    #endif
    
    // Enable multi-sampling by default.
    RenderParams.Display        = DisplayId(Hmd->DisplayDeviceName, Hmd->DisplayId);
    RenderParams.SrgbBackBuffer = SupportsSrgb && false;   // don't create sRGB back-buffer for OWD
    RenderParams.Multisample    = (SupportsMultisampling && MultisampleEnabled) ? 1 : 0;
    RenderParams.Resolution     = Hmd->Resolution;
  //RenderParams.Fullscreen     = true;

    pRender = pPlatform->SetupGraphics(OVR_DEFAULT_RENDER_DEVICE_SET,
                                       graphics, RenderParams); // To do: Remove the graphics argument to SetupGraphics, as RenderParams already has this info.
    return (pRender != nullptr);
}

// Custom formatter for Timewarp interval message.
static String FormatTimewarp(OptionVar* var)
{    
    char    buff[64];
    float   timewarpInterval = *var->AsFloat();
    OVR_sprintf(buff, sizeof(buff), "%.1fms, %.1ffps",
                timewarpInterval * 1000.0f,
                ( timewarpInterval > 0.000001f ) ? 1.0f / timewarpInterval : 10000.0f);
    return String(buff);
}

static String FormatMaxFromSideTan(OptionVar* var)
{
    char   buff[64];
    float  degrees = 2.0f * atan(*var->AsFloat()) * (180.0f / MATH_FLOAT_PI);
    OVR_sprintf(buff, sizeof(buff), "%.1f Degrees", degrees);
    return String(buff);
}

void OculusWorldDemoApp::PopulateOptionMenu()
{
    // For shortened function member access.
    typedef OculusWorldDemoApp OWD;

    // *** Scene Content Sub-Menu
      
    // Test
    /*
        Menu.AddEnum("Scene Content.EyePoseMode", &FT_EyePoseState).AddShortcutKey(Key_Y).
        AddEnumValue("Separate Pose",  0).
        AddEnumValue("Same Pose",      1).
        AddEnumValue("Same Pose+TW",   2);
    */

    // Navigate between scenes.
    Menu.AddEnum("Scene Content.Rendered Scene ';'", &SceneMode).AddShortcutKey(Key_Semicolon).
                 AddEnumValue("World",        Scene_World).
                 AddEnumValue("Cubes",        Scene_Cubes).
                 AddEnumValue("Oculus Cubes", Scene_OculusCubes);  
    // Animating blocks    
    Menu.AddEnum("Scene Content.Animated Blocks", &BlocksShowType).
                 AddShortcutKey(Key_B).SetNotify(this, &OWD::BlockShowChange).
                 AddEnumValue("None",  0).
                 AddEnumValue("Horizontal Circle", 1).
                 AddEnumValue("Vertical Circle",   2).
                 AddEnumValue("Bouncing Blocks",   3);  
    // Toggle grid
    Menu.AddEnum("Scene Content.Grid Display 'G'",  &GridDisplayMode).AddShortcutKey(Key_G).
                 AddEnumValue("No Grid",             GridDisplay_None).
                 AddEnumValue("Grid Only",           GridDisplay_GridOnly).
                 AddEnumValue("Grid And Scene",      GridDisplay_GridAndScene);  
    Menu.AddEnum("Scene Content.Grid Mode 'H'",     &GridMode).AddShortcutKey(Key_H).
                 AddEnumValue("4-pixel RT-centered", Grid_Rendertarget4).
                 AddEnumValue("16-pixel RT-centered",Grid_Rendertarget16).
                 AddEnumValue("Lens-centered grid",  Grid_Lens);  

    // *** Scene Content Sub-Menu
    Menu.AddBool( "Render Target.Share RenderTarget",  &RendertargetIsSharedByBothEyes).
                                                        AddShortcutKey(Key_F8).SetNotify(this, &OWD::HmdSettingChange);
    Menu.AddBool( "Render Target.Dynamic Res Scaling", &DynamicRezScalingEnabled).
                                                        AddShortcutKey(Key_F8, ShortcutKey::Shift_RequireOn);
    Menu.AddBool( "Sensor Toggle 'F6'",                &EnableSensor).
                                                        AddShortcutKey(Key_F6).
                                                        SetNotify(this, &OWD::HmdSensorToggle);
    Menu.AddBool( "Render Target.Monoscopic Render 'F7'", &MonoscopicRender).
                                                        AddShortcutKey(Key_F7).
                                                        SetNotify(this, &OWD::HmdSettingChangeFreeRTs);
    Menu.AddFloat( "Render Target.Max Fov",            &FovSideTanMax, 0.2f, FovSideTanLimit, 0.02f,
                                                        "%.1f Degrees", 1.0f, &FormatMaxFromSideTan).
                                                        SetNotify(this, &OWD::HmdSettingChange).
                                                        AddShortcutUpKey(Key_I).AddShortcutDownKey(Key_K);
    Menu.AddFloat("Render Target.Pixel Density",       &DesiredPixelDensity, 0.5f, 2.5, 0.025f, "%3.2f", 1.0f).
                                                        SetNotify(this, &OWD::HmdSettingChange);
    if (SupportsMultisampling)
    Menu.AddBool("Render Target.MultiSample 'F4'",    &MultisampleEnabled)    .AddShortcutKey(Key_F4).SetNotify(this, &OWD::MultisampleChange);
    Menu.AddBool("Render Target.High Quality Distortion 'F5'", &HqAaDistortion).AddShortcutKey(Key_F5).SetNotify(this, &OWD::HmdSettingChange);
    //Menu.AddBool("Render Target.sRGB Distortion 'F6'", &SupportsSrgb).AddShortcutKey(Key_F6).SetNotify(this, &OWD::MultisampleChange);
    Menu.AddEnum( "Render Target.Distortion Clear Color",  &DistortionClearBlue).
                                                        SetNotify(this, &OWD::DistortionClearColorChange).
                                                        AddEnumValue("Black",  0).
                                                        AddEnumValue("Blue", 1);
    Menu.AddBool( "Render Target.Faded Border",         &FadedBorder).
                                                        SetNotify(this, &OWD::HmdSettingChange);

    // Timewarp
    Menu.AddBool( "Timewarp.TimewarpEnabled 'O'",   &TimewarpEnabled).AddShortcutKey(Key_O).
    																SetNotify(this, &OWD::HmdSettingChange);
    Menu.AddBool( "Timewarp.Timewarp No-JIT",       &TimewarpNoJitEnabled).SetNotify(this, &OWD::HmdSettingChange);
    Menu.AddBool( "Timewarp.FreezeEyeUpdate 'C'",   &FreezeEyeUpdate).AddShortcutKey(Key_C);
    Menu.AddFloat("Timewarp.RenderIntervalSeconds", &TimewarpRenderIntervalInSeconds,   
                                                    0.0001f, 1.00f, 0.0001f, "%.1f", 1.0f, &FormatTimewarp).
                                                    AddShortcutUpKey(Key_J).AddShortcutDownKey(Key_U);

#if defined(OVR_OS_WIN32) || defined(OVR_OS_WIN64)
    Menu.AddBool( "Timewarp.ComputeShaderEnabled",  &ComputeShaderEnabled).
    																SetNotify(this, &OWD::HmdSettingChange);
#endif
    
    
    // First page properties
    Menu.AddFloat("Player.Position Tracking Scale", &PositionTrackingScale, 0.00f, 50.0f, 0.05f).
                                                    SetNotify(this, &OWD::EyeHeightChange);
    Menu.AddBool("Player.Scale Affects Player Height", &ScaleAffectsEyeHeight).SetNotify(this, &OWD::EyeHeightChange);
    Menu.AddFloat("Player.User Eye Height",         &ThePlayer.UserEyeHeight, 0.2f, 2.5f, 0.02f,
                                                    "%4.2f m").SetNotify(this, &OWD::EyeHeightChange).
                                                    AddShortcutUpKey(Key_Equal).AddShortcutDownKey(Key_Minus);
    Menu.AddFloat("Player.Center Pupil Depth",      &CenterPupilDepthMeters, 0.0f, 0.2f, 0.001f,
                                                    "%4.3f m").SetNotify(this, &OWD::CenterPupilDepthChange);

    Menu.AddBool("Body Relative Motion",&ThePlayer.bMotionRelativeToBody).AddShortcutKey(Key_E);    
    Menu.AddBool("Zero Head Movement",  &ForceZeroHeadMovement) .AddShortcutKey(Key_F7, ShortcutKey::Shift_RequireOn);
    Menu.AddBool("VSync 'V'",           &VsyncEnabled)          .AddShortcutKey(Key_V).SetNotify(this, &OWD::HmdSettingChange);
    Menu.AddBool("Logging 'L'", &IsLogging).AddShortcutKey(Key_L).SetNotify(this, &OWD::ToggleLogging);
    Menu.AddTrigger("Recenter HMD pose 'R'").AddShortcutKey(Key_R).SetNotify(this, &OWD::ResetHmdPose);
    
    // Add DK2 options to menu only for that headset.
    if (Hmd->TrackingCaps & ovrTrackingCap_Position)
    {
        Menu.AddBool("Low Persistence 'P'",     &IsLowPersistence).
                                                  AddShortcutKey(Key_P).SetNotify(this, &OWD::HmdSettingChange);
        Menu.AddBool("Dynamic Prediction",      &DynamicPrediction).
                                                  SetNotify(this, &OWD::HmdSettingChange);
        Menu.AddBool("Display Sleep",           &DisplaySleep).
                                                  AddShortcutKey(Key_Z).SetNotify(this, &OWD::HmdSettingChange);
        Menu.AddBool("Positional Tracking 'X'", &PositionTrackingEnabled).
                                                  AddShortcutKey(Key_X).SetNotify(this, &OWD::HmdSettingChange);
		Menu.AddBool("Pixel Luminance Overdrive", &PixelLuminanceOverdrive).SetNotify(this, &OWD::HmdSettingChange);        
    }

    if (!(Hmd->HmdCaps & ovrHmdCap_ExtendDesktop))
    {
        Menu.AddBool("Mirror To Window", &MirrorToWindow).
                                         AddShortcutKey(Key_M).SetNotify(this, &OWD::MirrorSettingChange);
    }
}


void OculusWorldDemoApp::CalculateHmdValues()
{
    // Initialize eye rendering information for ovrHmd_Configure.
    // The viewport sizes are re-computed in case RenderTargetSize changed due to HW limitations.
    ovrFovPort eyeFov[2];
    eyeFov[0] = Hmd->DefaultEyeFov[0];
    eyeFov[1] = Hmd->DefaultEyeFov[1];

    // Clamp Fov based on our dynamically adjustable FovSideTanMax.
    // Most apps should use the default, but reducing Fov does reduce rendering cost.
    eyeFov[0] = FovPort::Min(eyeFov[0], FovPort(FovSideTanMax));
    eyeFov[1] = FovPort::Min(eyeFov[1], FovPort(FovSideTanMax));


    if (MonoscopicRender)
    {
        // MonoscopicRender does three things:
        //  1) Sets FOV to maximum symmetrical FOV based on both eyes
        //  2) Sets eye HmdToEyeViewOffset values to 0.0 (effective IPD == 0)
        //  3) Uses only the Left texture for rendering.
        
        eyeFov[0] = FovPort::Max(eyeFov[0], eyeFov[1]);
        eyeFov[1] = eyeFov[0];

        Sizei recommenedTexSize = ovrHmd_GetFovTextureSize(Hmd, ovrEye_Left,
                                                           eyeFov[0], DesiredPixelDensity);

        Sizei textureSize = EnsureRendertargetAtLeastThisBig(Rendertarget_Left,  recommenedTexSize);

        EyeRenderSize[0] = Sizei::Min(textureSize, recommenedTexSize);
        EyeRenderSize[1] = EyeRenderSize[0];

        // Store texture pointers that will be passed for rendering.
        EyeTexture[0]                       = RenderTargets[Rendertarget_Left].OvrTex;
        EyeTexture[0].Header.TextureSize    = textureSize;
        EyeTexture[0].Header.RenderViewport = Recti(EyeRenderSize[0]);
        // Right eye is the same.
        EyeTexture[1] = EyeTexture[0];
    }

    else
    {
        // Configure Stereo settings. Default pixel density is 1.0f.
        Sizei recommenedTex0Size = ovrHmd_GetFovTextureSize(Hmd, ovrEye_Left,  eyeFov[0], DesiredPixelDensity);
        Sizei recommenedTex1Size = ovrHmd_GetFovTextureSize(Hmd, ovrEye_Right, eyeFov[1], DesiredPixelDensity);

        if (RendertargetIsSharedByBothEyes)
        {
            Sizei  rtSize(recommenedTex0Size.w + recommenedTex1Size.w,
                          Alg::Max(recommenedTex0Size.h, recommenedTex1Size.h));

            // Use returned size as the actual RT size may be different due to HW limits.
            rtSize = EnsureRendertargetAtLeastThisBig(Rendertarget_BothEyes, rtSize);

            // Don't draw more then recommended size; this also ensures that resolution reported
            // in the overlay HUD size is updated correctly for FOV/pixel density change.            
            EyeRenderSize[0] = Sizei::Min(Sizei(rtSize.w/2, rtSize.h), recommenedTex0Size);
            EyeRenderSize[1] = Sizei::Min(Sizei(rtSize.w/2, rtSize.h), recommenedTex1Size);

            // Store texture pointers that will be passed for rendering.
            // Same texture is used, but with different viewports.
            EyeTexture[0]                       = RenderTargets[Rendertarget_BothEyes].OvrTex;
            EyeTexture[0].Header.TextureSize    = rtSize;
            EyeTexture[0].Header.RenderViewport = Recti(Vector2i(0), EyeRenderSize[0]);
            EyeTexture[1]                       = RenderTargets[Rendertarget_BothEyes].OvrTex;
            EyeTexture[1].Header.TextureSize    = rtSize;
            EyeTexture[1].Header.RenderViewport = Recti(Vector2i((rtSize.w+1)/2, 0), EyeRenderSize[1]);
        }
        else
        {
            Sizei tex0Size = EnsureRendertargetAtLeastThisBig(Rendertarget_Left,  recommenedTex0Size);
            Sizei tex1Size = EnsureRendertargetAtLeastThisBig(Rendertarget_Right, recommenedTex1Size);

            EyeRenderSize[0] = Sizei::Min(tex0Size, recommenedTex0Size);
            EyeRenderSize[1] = Sizei::Min(tex1Size, recommenedTex1Size);

            // Store texture pointers and viewports that will be passed for rendering.
            EyeTexture[0]                       = RenderTargets[Rendertarget_Left].OvrTex;
            EyeTexture[0].Header.TextureSize    = tex0Size;
            EyeTexture[0].Header.RenderViewport = Recti(EyeRenderSize[0]);
            EyeTexture[1]                       = RenderTargets[Rendertarget_Right].OvrTex;
            EyeTexture[1].Header.TextureSize    = tex1Size;
            EyeTexture[1].Header.RenderViewport = Recti(EyeRenderSize[1]);
        }
    }

    DrawEyeTargets = (MultisampleEnabled && SupportsMultisampling) ? MsaaRenderTargets : RenderTargets;

    // Hmd caps.
    unsigned hmdCaps = (VsyncEnabled ? 0 : ovrHmdCap_NoVSync);
    if (IsLowPersistence)
        hmdCaps |= ovrHmdCap_LowPersistence;

    // ovrHmdCap_DynamicPrediction - enables internal latency feedback
    if (DynamicPrediction)
        hmdCaps |= ovrHmdCap_DynamicPrediction;

    // ovrHmdCap_DisplayOff - turns off the display
    if (DisplaySleep)
        hmdCaps |= ovrHmdCap_DisplayOff;

    if (!MirrorToWindow)
        hmdCaps |= ovrHmdCap_NoMirrorToWindow;

    // If using our driver, display status overlay messages.
    if (!(Hmd->HmdCaps & ovrHmdCap_ExtendDesktop) && (NotificationTimeout != 0.0f))
    {                
        GetPlatformCore()->SetNotificationOverlay(0, 28, 8,
           "Rendering to the Hmd - Please put on your Rift");
        GetPlatformCore()->SetNotificationOverlay(1, 24, -8,
            MirrorToWindow ? "'M' - Mirror to Window [On]" : "'M' - Mirror to Window [Off]");
    }


    ovrHmd_SetEnabledCaps(Hmd, hmdCaps);


	ovrRenderAPIConfig config         = pRender->Get_ovrRenderAPIConfig();
    unsigned           distortionCaps = ovrDistortionCap_Chromatic;

    if (FadedBorder)
        distortionCaps |= ovrDistortionCap_Vignette;
    if (SupportsSrgb)
        distortionCaps |= ovrDistortionCap_SRGB;
	if(PixelLuminanceOverdrive)
		distortionCaps |= ovrDistortionCap_Overdrive;
    if (TimewarpEnabled)
        distortionCaps |= ovrDistortionCap_TimeWarp;
    if(TimewarpNoJitEnabled)
        distortionCaps |= ovrDistortionCap_ProfileNoTimewarpSpinWaits;
    if(HqAaDistortion)
        distortionCaps |= ovrDistortionCap_HqDistortion;
#if defined(OVR_OS_LINUX)
    if (LinuxFullscreenOnDevice)
        distortionCaps |= ovrDistortionCap_LinuxDevFullscreen;
#endif

#if defined(OVR_OS_WIN32) || defined(OVR_OS_WIN64)
    if(ComputeShaderEnabled)
        distortionCaps |= ovrDistortionCap_ComputeShader;
#endif

    if (!ovrHmd_ConfigureRendering( Hmd, &config, distortionCaps, eyeFov, EyeRenderDesc ))
    {
        // Fail exit? TBD
        return;
    }

    if (MonoscopicRender)
    {
        // Remove IPD adjust
        EyeRenderDesc[0].HmdToEyeViewOffset = Vector3f(0);
        EyeRenderDesc[1].HmdToEyeViewOffset = Vector3f(0);
    }

    unsigned sensorCaps = ovrTrackingCap_Orientation|ovrTrackingCap_MagYawCorrection;
    if (PositionTrackingEnabled)
        sensorCaps |= ovrTrackingCap_Position;
      
    if (StartTrackingCaps != sensorCaps)
    {
        ovrHmd_ConfigureTracking(Hmd, sensorCaps, 0);
        StartTrackingCaps = sensorCaps;
    }    

    // Calculate projections
    Projection[0] = ovrMatrix4f_Projection(EyeRenderDesc[0].Fov,  0.01f, 10000.0f, true);
    Projection[1] = ovrMatrix4f_Projection(EyeRenderDesc[1].Fov,  0.01f, 10000.0f, true);

    float    orthoDistance = 0.8f; // 2D is 0.8 meter from camera
    Vector2f orthoScale0   = Vector2f(1.0f) / Vector2f(EyeRenderDesc[0].PixelsPerTanAngleAtCenter);
    Vector2f orthoScale1   = Vector2f(1.0f) / Vector2f(EyeRenderDesc[1].PixelsPerTanAngleAtCenter);
    
    OrthoProjection[0] = ovrMatrix4f_OrthoSubProjection(Projection[0], orthoScale0, orthoDistance,
                                                        EyeRenderDesc[0].HmdToEyeViewOffset.x);
    OrthoProjection[1] = ovrMatrix4f_OrthoSubProjection(Projection[1], orthoScale1, orthoDistance,
                                                        EyeRenderDesc[1].HmdToEyeViewOffset.x);

    // all done
    HmdSettingsChanged = false;
}



// Returns the actual size present.
Sizei OculusWorldDemoApp::EnsureRendertargetAtLeastThisBig(int rtNum, Sizei requestedSize)
{
    OVR_ASSERT((rtNum >= 0) && (rtNum < Rendertarget_LAST));

    // Texture size that we already have might be big enough.
    Sizei newRTSize;

    RenderTarget& rt = RenderTargets[rtNum];
    RenderTarget& msrt = MsaaRenderTargets[rtNum];
    if (!rt.pTex)
    {
        // Hmmm... someone nuked my texture. Rez change or similar. Make sure we reallocate.
        rt.OvrTex.Header.TextureSize = Sizei(0);
        
        if(MultisampleEnabled && SupportsMultisampling)
            msrt.OvrTex.Header.TextureSize = Sizei(0);

        newRTSize = requestedSize;
    }
    else
    {
        newRTSize = rt.OvrTex.Header.TextureSize;
    }

    // %50 linear growth each time is a nice balance between being too greedy
    // for a 2D surface and too slow to prevent fragmentation.
    while ( newRTSize.w < requestedSize.w )
    {
        newRTSize.w += newRTSize.w/2;
    }
    while ( newRTSize.h < requestedSize.h )
    {
        newRTSize.h += newRTSize.h/2;
    }

    // Put some sane limits on it. 4k x 4k is fine for most modern video cards.
    // Nobody should be messing around with surfaces smaller than 4k pixels these days.
    newRTSize = Sizei::Max(Sizei::Min(newRTSize, Sizei(4096)), Sizei(64));

    // Does that require actual reallocation?
    if (Sizei(rt.OvrTex.Header.TextureSize) != newRTSize)        
    {        
        int format = Texture_RGBA | Texture_RenderTarget;
        if (SupportsSrgb)
            format |= Texture_SRGB;

        rt.pTex = *pRender->CreateTexture(format, newRTSize.w, newRTSize.h, NULL);
        rt.pTex->SetSampleMode(Sample_ClampBorder | Sample_Linear);
        
        // Configure texture for SDK Rendering.
        rt.OvrTex = rt.pTex->Get_ovrTexture();
        
        if(MultisampleEnabled && SupportsMultisampling)
        {
            int msaaformat = format | 4;    // 4 is MSAA rate

            msrt.pTex = *pRender->CreateTexture(msaaformat, newRTSize.w, newRTSize.h, NULL);
            msrt.pTex->SetSampleMode(Sample_ClampBorder | Sample_Linear);

            // Configure texture for SDK Rendering.
            msrt.OvrTex = rt.pTex->Get_ovrTexture();
        }
    }
    
    return newRTSize;
}


//-----------------------------------------------------------------------------
// ***** Message Handlers

void OculusWorldDemoApp::OnResize(int width, int height)
{
    WindowSize = Sizei(width, height);
    HmdSettingsChanged = true;
}

void OculusWorldDemoApp::OnMouseMove(int x, int y, int modifiers)
{
    OVR_UNUSED(y);
    if(modifiers & Mod_MouseRelative)
    {
        // Get Delta
        int dx = x;

        // Apply to rotation. Subtract for right body frame rotation,
        // since yaw rotation is positive CCW when looking down on XZ plane.
        ThePlayer.BodyYaw   -= (Sensitivity * dx) / 360.0f;
    }
}


void OculusWorldDemoApp::OnKey(OVR::KeyCode key, int chr, bool down, int modifiers)
{
    if (down)
    {   // Dismiss Safety warning with any key.
        ovrHmd_DismissHSWDisplay(Hmd);
    }

    if (Menu.OnKey(key, chr, down, modifiers))
        return;

    // Handle player movement keys.
    if (ThePlayer.HandleMoveKey(key, down))
        return;

    switch(key)
    {
    case Key_Q:
        if (down && (modifiers & Mod_Control))
            pPlatform->Exit(0);
        break;
        
    case Key_Escape:
        // Back to primary windowed
        if (!down) ChangeDisplay ( true, false, false );
        break;
        
    case Key_F9:
        // Cycle through displays, going fullscreen on each one.
        if (!down) ChangeDisplay ( false, true, false );
        break;
        
#ifdef OVR_OS_MAC
     // F11 is reserved on Mac, F10 doesn't work on Windows
    case Key_F10:  
#else
    case Key_F11:
#endif
        if (!down) ChangeDisplay ( false, false, true );
        break;
        
	case Key_Space:
        if (!down)
        {
            TextScreen = (enum TextScreen)((TextScreen + 1) % Text_Count);
        }
        break;

    // Distortion correction adjustments
    case Key_Backslash:        
        break;
        // Holding down Shift key accelerates adjustment velocity.
    case Key_Shift:
        ShiftDown = down;
        break;
    case Key_Control:
        CtrlDown = down;
        break;

       // Reset the camera position in case we get stuck
    case Key_T:
        if (down)
        {
            struct {
                float  x, z;
                float  YawDegrees;
            }  Positions[] =
            {
               // x         z           Yaw
                { 7.7f,     -1.0f,      180.0f },   // The starting position.
                { 10.0f,    10.0f,      90.0f  },   // Outside, looking at some trees.
                { 19.26f,   5.43f,      22.0f  },   // Outside, looking at the fountain.
            };

            static int nextPosition = 0;
            nextPosition = (nextPosition + 1) % (sizeof(Positions)/sizeof(Positions[0]));

            ThePlayer.BodyPos = Vector3f(Positions[nextPosition].x,
                                         ThePlayer.UserEyeHeight, Positions[nextPosition].z);
            ThePlayer.BodyYaw = DegreeToRad( Positions[nextPosition].YawDegrees );
        }
        break;

    case Key_BracketLeft: // Control-Shift-[  --> Test OVR_ASSERT
        if(down && (modifiers & Mod_Control) && (modifiers & Mod_Shift))
            OVR_ASSERT(key != Key_BracketLeft);
        break;

    case Key_BracketRight: // Control-Shift-]  --> Test exception handling
        if(down && (modifiers & Mod_Control) && (modifiers & Mod_Shift))
            OVR::CreateException(OVR::kCETAccessViolation);
        break;

    case Key_Num1:
        ThePlayer.BodyPos = Vector3f(-1.85f, 6.0f, -0.52f);
        ThePlayer.BodyPos.y += ThePlayer.UserEyeHeight;
        ThePlayer.BodyYaw = 3.1415f / 2;
        ThePlayer.HandleMovement(0, &CollisionModels, &GroundCollisionModels, ShiftDown);
        break;

     default:
        break;
    }
}

//-----------------------------------------------------------------------------


Matrix4f OculusWorldDemoApp::CalculateViewFromPose(const Posef& pose)
{
    Posef worldPose = ThePlayer.VirtualWorldTransformfromRealPose(pose);

    // Rotate and position View Camera
    Vector3f up      = worldPose.Rotation.Rotate(UpVector);
    Vector3f forward = worldPose.Rotation.Rotate(ForwardVector);

    // Transform the position of the center eye in the real world (i.e. sitting in your chair)
    // into the frame of the player's virtual body.

    Vector3f viewPos = ForceZeroHeadMovement ? ThePlayer.BodyPos : worldPose.Translation;

    Matrix4f view = Matrix4f::LookAtRH(viewPos, viewPos + forward, up);
    return view;
}



void OculusWorldDemoApp::OnIdle()
{
    double curtime = ovr_GetTimeInSeconds();
    // If running slower than 10fps, clamp. Helps when debugging, because then dt can be minutes!
    float  dt      = Alg::Min<float>(float(curtime - LastUpdate), 0.1f);
    LastUpdate     = curtime;    


    Profiler.RecordSample(RenderProfiler::Sample_FrameStart);

    if (LoadingState == LoadingState_DoLoad)
    {
        PopulateScene(MainFilePath.ToCStr());
        LoadingState = LoadingState_Finished;
        return;
    }    

    if (HmdSettingsChanged)
    {
        CalculateHmdValues();        
    }
    
    // Kill overlays in non-mirror mode after timeout.
    if ((NotificationTimeout != 0.0) && (curtime > NotificationTimeout))
    {
        if (MirrorToWindow)
        {
            GetPlatformCore()->SetNotificationOverlay(0,0,0,0);
            GetPlatformCore()->SetNotificationOverlay(1,0,0,0);
        }
        NotificationTimeout = 0.0;
    }


    HmdFrameTiming = ovrHmd_BeginFrame(Hmd, 0);


    // Update gamepad.
    GamepadState gamepadState;
    if (GetPlatformCore()->GetGamepadManager()->GetGamepadState(0, &gamepadState))
    {
        GamepadStateChanged(gamepadState);
    }

    ovrTrackingState trackState = ovrHmd_GetTrackingState(Hmd, HmdFrameTiming.ScanoutMidpointSeconds);
    HmdStatus = trackState.StatusFlags;

    // Report vision tracking
	bool hadVisionTracking = HaveVisionTracking;
	HaveVisionTracking = (trackState.StatusFlags & ovrStatus_PositionTracked) != 0;
	if (HaveVisionTracking && !hadVisionTracking)
		Menu.SetPopupMessage("Vision Tracking Acquired");
    if (!HaveVisionTracking && hadVisionTracking)
		Menu.SetPopupMessage("Lost Vision Tracking");

    // Report position tracker
    bool hadPositionTracker = HavePositionTracker;
    HavePositionTracker = (trackState.StatusFlags & ovrStatus_PositionConnected) != 0;
    if (HavePositionTracker && !hadPositionTracker)
        Menu.SetPopupMessage("Position Tracker Connected");
    if (!HavePositionTracker && hadPositionTracker)
        Menu.SetPopupMessage("Position Tracker Disconnected");

    // Report position tracker
    bool hadHMDConnected = HaveHMDConnected;
    HaveHMDConnected = (trackState.StatusFlags & ovrStatus_HmdConnected) != 0;
    if (HaveHMDConnected && !hadHMDConnected)
        Menu.SetPopupMessage("HMD Connected");
    if (!HaveHMDConnected && hadHMDConnected)
        Menu.SetPopupMessage("HMD Disconnected");

    UpdateVisionProcessingTime(trackState);

    // Check if any new devices were connected.
    ProcessDeviceNotificationQueue();
    // FPS count and timing.
    UpdateFrameRateCounter(curtime);

    
    // Update pose based on frame!
    ThePlayer.HeadPose = trackState.HeadPose.ThePose;
    // Movement/rotation with the gamepad.
    ThePlayer.BodyYaw -= ThePlayer.GamepadRotate.x * dt;
    ThePlayer.HandleMovement(dt, &CollisionModels, &GroundCollisionModels, ShiftDown);


    // Record after processing time.
    Profiler.RecordSample(RenderProfiler::Sample_AfterGameProcessing);    


    // Determine if we are rendering this frame. Frame rendering may be
    // skipped based on FreezeEyeUpdate and Time-warp timing state.
    bool bupdateRenderedView = FrameNeedsRendering(curtime);
    
    if (bupdateRenderedView)
    {
        // If render texture size is changing, apply dynamic changes to viewport.
        ApplyDynamicResolutionScaling();

        pRender->BeginScene(PostProcess_None);

        ovrTrackingState hmdState;
        ovrVector3f hmdToEyeViewOffset[2] = { EyeRenderDesc[0].HmdToEyeViewOffset, EyeRenderDesc[1].HmdToEyeViewOffset };
        ovrHmd_GetEyePoses(Hmd, 0, hmdToEyeViewOffset, EyeRenderPose, &hmdState);

        // It is important to have head movement in scale with IPD.
        // If you shrink one, you should also shrink the other.
        // So with zero IPD (i.e. everything at infinity),
        // head movement should also be zero.
        EyeRenderPose[0].Position = ((Vector3f)EyeRenderPose[0].Position) * PositionTrackingScale;
        EyeRenderPose[1].Position = ((Vector3f)EyeRenderPose[1].Position) * PositionTrackingScale;

        if (MonoscopicRender)
        {             
            // Zero IPD eye rendering: draw into left eye only,
            // re-use  texture for right eye.
            pRender->SetRenderTarget(DrawEyeTargets[Rendertarget_Left].pTex);
            pRender->Clear();
        
            View = CalculateViewFromPose(hmdState.HeadPose.ThePose);
            RenderEyeView(ovrEye_Left);
            // Note: Second eye gets texture is (initialized to same value above).
        }

        else if (RendertargetIsSharedByBothEyes)
        {
            // Shared render target eye rendering; set up RT once for both eyes.
            pRender->SetRenderTarget(DrawEyeTargets[Rendertarget_BothEyes].pTex);
            pRender->Clear();

            for (int eyeIndex = 0; eyeIndex < ovrEye_Count; eyeIndex++)
            {
                ovrEyeType eye = Hmd->EyeRenderOrder[eyeIndex];

                View = CalculateViewFromPose(EyeRenderPose[eye]);
                RenderEyeView(eye);
            }
        }

        else
        {

            // Separate eye rendering - each eye gets its own render target.
            for (int eyeIndex = 0; eyeIndex < ovrEye_Count; eyeIndex++)
            {      
                ovrEyeType eye = Hmd->EyeRenderOrder[eyeIndex];
                pRender->SetRenderTarget(
                    DrawEyeTargets[(eye == 0) ? Rendertarget_Left : Rendertarget_Right].pTex);
                pRender->Clear();
            
                View = CalculateViewFromPose(EyeRenderPose[eye]);
                RenderEyeView(eye);
            }
        }   

        pRender->SetDefaultRenderTarget();
        pRender->FinishScene();

        if(MultisampleEnabled && SupportsMultisampling)
        {
            if (MonoscopicRender)
            {
                pRender->ResolveMsaa(MsaaRenderTargets[Rendertarget_Left].pTex, RenderTargets[Rendertarget_Left].pTex);
            }
            else if (RendertargetIsSharedByBothEyes)
            {
                pRender->ResolveMsaa(MsaaRenderTargets[Rendertarget_BothEyes].pTex, RenderTargets[Rendertarget_BothEyes].pTex);
            }
            else
            {
                for (int eyeIndex = 0; eyeIndex < ovrEye_Count; eyeIndex++)
                    pRender->ResolveMsaa(MsaaRenderTargets[eyeIndex].pTex, RenderTargets[eyeIndex].pTex);
            }
        }
    }
       
    /*
    double t= ovr_GetTimeInSeconds();
    while (ovr_GetTimeInSeconds() < (t + 0.017))
    {

    } */

    Profiler.RecordSample(RenderProfiler::Sample_AfterEyeRender);

    // TODO: These happen inside ovrHmd_EndFrame; need to hook into it.
    //Profiler.RecordSample(RenderProfiler::Sample_BeforeDistortion);
    ovrHmd_EndFrame(Hmd, EyeRenderPose, EyeTexture);
    Profiler.RecordSample(RenderProfiler::Sample_AfterPresent);    
}



// Determine whether this frame needs rendering based on time-warp timing and flags.
bool OculusWorldDemoApp::FrameNeedsRendering(double curtime)
{    
    static double   lastUpdate          = 0.0;    
    double          renderInterval      = TimewarpRenderIntervalInSeconds;
    double          timeSinceLast       = curtime - lastUpdate;
    bool            updateRenderedView  = true;

    if (FreezeEyeUpdate)
    {
        // Draw one frame after (FreezeEyeUpdate = true) to update message text.            
        if (FreezeEyeOneFrameRendered)
            updateRenderedView = false;
        else
            FreezeEyeOneFrameRendered = true;
    }
    else
    {
        FreezeEyeOneFrameRendered = false;

        if ( (timeSinceLast < 0.0) || ((float)timeSinceLast > renderInterval) )
        {
            // This allows us to do "fractional" speeds, e.g. 45fps rendering on a 60fps display.
            lastUpdate += renderInterval;
            if ( timeSinceLast > 5.0 )
            {
                // renderInterval is probably tiny (i.e. "as fast as possible")
                lastUpdate = curtime;
            }

            updateRenderedView = true;
        }
        else
        {
            updateRenderedView = false;
        }
    }        
        
    return updateRenderedView;
}


void OculusWorldDemoApp::ApplyDynamicResolutionScaling()
{
    if (!DynamicRezScalingEnabled)
    {
        // Restore viewport rectangle in case dynamic res scaling was enabled before.
        EyeTexture[0].Header.RenderViewport.Size = EyeRenderSize[0];
        EyeTexture[1].Header.RenderViewport.Size = EyeRenderSize[1];
        return;
    }
   
    // Demonstrate dynamic-resolution rendering.
    // This demo is too simple to actually have a framerate that varies that much, so we'll
    // just pretend this is trying to cope with highly dynamic rendering load.
    float dynamicRezScale = 1.0f;

    {
        // Hacky stuff to make up a scaling...
        // This produces value oscillating as follows: 0 -> 1 -> 0.        
        static double dynamicRezStartTime   = ovr_GetTimeInSeconds();
        float         dynamicRezPhase       = float ( ovr_GetTimeInSeconds() - dynamicRezStartTime );
        const float   dynamicRezTimeScale   = 4.0f;

        dynamicRezPhase /= dynamicRezTimeScale;
        if ( dynamicRezPhase < 1.0f )
        {
            dynamicRezScale = dynamicRezPhase;
        }
        else if ( dynamicRezPhase < 2.0f )
        {
            dynamicRezScale = 2.0f - dynamicRezPhase;
        }
        else
        {
            // Reset it to prevent creep.
            dynamicRezStartTime = ovr_GetTimeInSeconds();
            dynamicRezScale     = 0.0f;
        }

        // Map oscillation: 0.5 -> 1.0 -> 0.5
        dynamicRezScale = dynamicRezScale * 0.5f + 0.5f;
    }

    Sizei sizeLeft  = EyeRenderSize[0];
    Sizei sizeRight = EyeRenderSize[1];
    
    // This viewport is used for rendering and passed into ovrHmd_EndEyeRender.
    EyeTexture[0].Header.RenderViewport.Size = Sizei(int(sizeLeft.w  * dynamicRezScale),
                                                     int(sizeLeft.h  * dynamicRezScale));
    EyeTexture[1].Header.RenderViewport.Size = Sizei(int(sizeRight.w * dynamicRezScale),
                                                     int(sizeRight.h * dynamicRezScale));
}


void OculusWorldDemoApp::UpdateFrameRateCounter(double curtime)
{
    FrameCounter++;
	TotalFrameCounter++;
    float secondsSinceLastMeasurement = (float)( curtime - LastFpsUpdate );

    if (secondsSinceLastMeasurement >= SecondsOfFpsMeasurement)
    {
        SecondsPerFrame = (float)( curtime - LastFpsUpdate ) / (float)FrameCounter;
        FPS             = 1.0f / SecondsPerFrame;
        LastFpsUpdate   = curtime;
        FrameCounter =   0;
    }
}

void OculusWorldDemoApp::UpdateVisionProcessingTime(const ovrTrackingState& trackState)
{
    // Update LastVisionProcessingTime
    if (trackState.LastVisionProcessingTime != LastVisionProcessingTime)
    {
        LastVisionProcessingTime = trackState.LastVisionProcessingTime;

        VisionProcessingSum += LastVisionProcessingTime;

        if (VisionTimesCount >= 20)
        {
            VisionProcessingAverage = VisionProcessingSum / 20.;
            VisionProcessingSum = 0.;
            VisionTimesCount = 0;
        }
        else
        {
            VisionTimesCount++;
        }
    }
}

void OculusWorldDemoApp::RenderEyeView(ovrEyeType eye)
{
    Recti    renderViewport = EyeTexture[eye].Header.RenderViewport;

    // *** 3D - Configures Viewport/Projection and Render
    
    pRender->ApplyStereoParams(renderViewport, Projection[eye]);
    pRender->SetDepthMode(true, true);

    Matrix4f baseTranslate = Matrix4f::Translation(ThePlayer.BodyPos);
    Matrix4f baseYaw       = Matrix4f::RotationY(ThePlayer.BodyYaw.Get());


    if (GridDisplayMode != GridDisplay_GridOnly)
    {
        if (SceneMode != Scene_OculusCubes)
        {
            MainScene.Render(pRender, View);        
            RenderAnimatedBlocks(eye, ovr_GetTimeInSeconds());
        }
	    
        if (SceneMode == Scene_Cubes)
	    {
            // Draw scene cubes overlay. Red if position tracked, blue otherwise.
            Scene sceneCubes = (HmdStatus & ovrStatus_PositionTracked) ?
                               RedCubesScene : BlueCubesScene;        
            sceneCubes.Render(pRender, View * baseTranslate * baseYaw);
        }

	    else if (SceneMode == Scene_OculusCubes)
	    {
            OculusCubesScene.Render(pRender, View * baseTranslate * baseYaw);
        }
    }   

    if (GridDisplayMode != GridDisplay_None)
    {
        RenderGrid(eye);
    }


    // *** 2D Text - Configure Orthographic rendering.

    // Render UI in 2D orthographic coordinate system that maps [-1,1] range
    // to a readable FOV area centered at your eye and properly adjusted.
    pRender->ApplyStereoParams(renderViewport, OrthoProjection[eye]);
    pRender->SetDepthMode(false, false);

    // We set this scale up in CreateOrthoSubProjection().
    float textHeight = 22.0f;

    // Display Loading screen-shot in frame 0.
    if (LoadingState != LoadingState_Finished)
    {
        const float scale = textHeight * 25.0f;
        Matrix4f view ( scale, 0.0f, 0.0f, 0.0f, scale, 0.0f, 0.0f, 0.0f, scale );
        LoadingScene.Render(pRender, view);
        String loadMessage = String("Loading ") + MainFilePath;
        DrawTextBox(pRender, 0.0f, -textHeight, textHeight, loadMessage.ToCStr(), DrawText_HCenter);
        LoadingState = LoadingState_DoLoad;
    }

    // HUD overlay brought up by spacebar.
    RenderTextInfoHud(textHeight);

    // Menu brought up by 
    Menu.Render(pRender);
}



// NOTE - try to keep these in sync with the PDF docs!
static const char* HelpText1 =
    "Spacebar 	            \t500 Toggle debug info overlay\n"
    "W, S            	    \t500 Move forward, back\n"
    "A, D 		    	    \t500 Strafe left, right\n"
    "Mouse move 	        \t500 Look left, right\n"
    "Left gamepad stick     \t500 Move\n"
    "Right gamepad stick    \t500 Turn\n"
    "T			            \t500 Reset player position";
    
static const char* HelpText2 =        
    "R              \t250 Reset sensor orientation\n"
    "G 			    \t250 Cycle grid overlay mode\n"
    "-, +           \t250 Adjust eye height\n"
    "Esc            \t250 Cancel full-screen\n"
    "F4			    \t250 Multisampling toggle\n"    
    "F9             \t250 Hardware full-screen (low latency)\n"
    "F11            \t250 Faked full-screen (easier debugging)\n"
    "Ctrl+Q		    \t250 Quit";


void FormatLatencyReading(char* buff, size_t size, float val)
{    
    if (val < 0.000001f)
        OVR_strcpy(buff, size, "N/A   ");
    else
        OVR_sprintf(buff, size, "%4.2fms", val * 1000.0f);    
}


void OculusWorldDemoApp::RenderTextInfoHud(float textHeight)
{
    // View port & 2D ortho projection must be set before call.
    
    float hmdYaw, hmdPitch, hmdRoll;
    switch(TextScreen)
    {
    case Text_Info:
    {
        char buf[512];

        // Average FOVs.
        FovPort leftFov  = EyeRenderDesc[0].Fov;
        FovPort rightFov = EyeRenderDesc[1].Fov;
        
        // Rendered size changes based on selected options & dynamic rendering.
        int pixelSizeWidth = EyeTexture[0].Header.RenderViewport.Size.w +
                             ((!MonoscopicRender) ?
                               EyeTexture[1].Header.RenderViewport.Size.w : 0);
        int pixelSizeHeight = ( EyeTexture[0].Header.RenderViewport.Size.h +
                                EyeTexture[1].Header.RenderViewport.Size.h ) / 2;

        // No DK2, no message.
        char latency2Text[128] = "";
        {
            //float latency2 = ovrHmd_GetMeasuredLatencyTest2(Hmd) * 1000.0f; // show it in ms
            //if (latency2 > 0)
            //    OVR_sprintf(latency2Text, sizeof(latency2Text), "%.2fms", latency2);

            float latencies[3] = { 0.0f, 0.0f, 0.0f };
            if (ovrHmd_GetFloatArray(Hmd, "DK2Latency", latencies, 3) == 3)
            {
                char latencyText0[32], latencyText1[32], latencyText2[32];
                FormatLatencyReading(latencyText0, sizeof(latencyText0), latencies[0]);
                FormatLatencyReading(latencyText1, sizeof(latencyText1), latencies[1]);
                FormatLatencyReading(latencyText2, sizeof(latencyText2), latencies[2]);

                OVR_sprintf(latency2Text, sizeof(latency2Text),
                            " DK2 Latency  Ren: %s  TWrp: %s\n"
                            " PostPresent: %s  VisionProc: %1.2f ms ",
                            latencyText0, latencyText1, latencyText2,
                            (float)VisionProcessingAverage * 1000);
            }
        }

        ThePlayer.HeadPose.Rotation.GetEulerAngles<Axis_Y, Axis_X, Axis_Z>(&hmdYaw, &hmdPitch, &hmdRoll);
        OVR_sprintf(buf, sizeof(buf),
                    " HMD YPR:%4.0f %4.0f %4.0f   Player Yaw: %4.0f\n"
                    " FPS: %.1f  ms/frame: %.1f Frame: %03d %d\n"
                    " Pos: %3.2f, %3.2f, %3.2f  HMD: %s\n"
                    " EyeHeight: %3.2f, IPD: %3.1fmm\n" //", Lens: %s\n"
                    " FOV %3.1fx%3.1f, Resolution: %ix%i\n"
                    "%s",
                    RadToDegree(hmdYaw), RadToDegree(hmdPitch), RadToDegree(hmdRoll),
                    RadToDegree(ThePlayer.BodyYaw.Get()),
                    FPS, SecondsPerFrame * 1000.0f, FrameCounter, TotalFrameCounter % 2,
                    ThePlayer.BodyPos.x, ThePlayer.BodyPos.y, ThePlayer.BodyPos.z,
                    //GetDebugNameHmdType ( TheHmdRenderInfo.HmdType ),
                    Hmd->ProductName,
                    ThePlayer.UserEyeHeight,
                    ovrHmd_GetFloat(Hmd, OVR_KEY_IPD, 0) * 1000.0f,
                    //( EyeOffsetFromNoseLeft + EyeOffsetFromNoseRight ) * 1000.0f,
                    //GetDebugNameEyeCupType ( TheHmdRenderInfo.EyeCups ),  // Lens/EyeCup not exposed
                    
                    (leftFov.GetHorizontalFovDegrees() + rightFov.GetHorizontalFovDegrees()) * 0.5f,
                    (leftFov.GetVerticalFovDegrees() + rightFov.GetVerticalFovDegrees()) * 0.5f,

                    pixelSizeWidth, pixelSizeHeight,

                    latency2Text
                    );

#if 0   // Enable if interested in texture memory usage stats
        size_t texMemInMB = pRender->GetTotalTextureMemoryUsage() / (1024 * 1024); // 1 MB
        if (texMemInMB)
        {
            char gpustat[256];
            OVR_sprintf(gpustat, sizeof(gpustat), "\nGPU Tex: %u MB", texMemInMB);
            OVR_strcat(buf, sizeof(buf), gpustat);
        }
#endif

        DrawTextBox(pRender, 0.0f, 0.0f, textHeight, buf, DrawText_Center);
    }
    break;
    
    case Text_Timing:    
        Profiler.DrawOverlay(pRender);    
    break;
    
    case Text_Help1:
        DrawTextBox(pRender, 0.0f, 0.0f, textHeight, HelpText1, DrawText_Center);
        break;
    case Text_Help2:
        DrawTextBox(pRender, 0.0f, 0.0f, textHeight, HelpText2, DrawText_Center);
        break;
    
    case Text_None:
        break;

    default:
        OVR_ASSERT ( !"Missing text screen" );
        break;    
    }
}


//-----------------------------------------------------------------------------
// ***** Callbacks For Menu changes

// Non-trivial callback go here.

void OculusWorldDemoApp::HmdSensorToggle(OptionVar* var)
{
	if (*var->AsBool())
	{
		EnableSensor = true;
		if (!ovrHmd_ConfigureTracking(Hmd, StartTrackingCaps, 0))
		{
			OVR_ASSERT(false);
		}
	}
	else
	{
		EnableSensor = false;
		ovrHmd_ConfigureTracking(Hmd, 0, 0);
	}
}

void OculusWorldDemoApp::HmdSettingChangeFreeRTs(OptionVar*)
{
    HmdSettingsChanged = true;
    // Cause the RTs to be recreated with the new mode.
    for ( int rtNum = 0; rtNum < Rendertarget_LAST; rtNum++ )
    {
        RenderTargets[rtNum].pTex = NULL;
        MsaaRenderTargets[rtNum].pTex = NULL;
    }
}

void OculusWorldDemoApp::MultisampleChange(OptionVar*)
{
    HmdSettingChangeFreeRTs();
}

void OculusWorldDemoApp::CenterPupilDepthChange(OptionVar*)
{
    ovrHmd_SetFloat(Hmd, "CenterPupilDepth", CenterPupilDepthMeters);
}

void OculusWorldDemoApp::DistortionClearColorChange(OptionVar*)
{
    float clearColor[2][4] = { { 0.0f, 0.0f, 0.0f, 0.0f },
                               { 0.0f, 0.5f, 1.0f, 0.0f } };
    ovrHmd_SetFloatArray(Hmd, "DistortionClearColor",
                         clearColor[(int)DistortionClearBlue], 4);
}

void OculusWorldDemoApp::ToggleLogging(OptionVar*)
{
    if (IsLogging)
    {
        ovrHmd_StartPerfLog(Hmd, "OWDLog.csv", 0);
    }
    else
    {
        ovrHmd_StopPerfLog(Hmd);
    }
}

void OculusWorldDemoApp::ResetHmdPose(OptionVar* /* = 0 */)
{
    ovrHmd_RecenterPose(Hmd);
    Menu.SetPopupMessage("Sensor Fusion Recenter Pose");
}

//-----------------------------------------------------------------------------

void OculusWorldDemoApp::ProcessDeviceNotificationQueue()
{
    // TBD: Process device plug & Unplug     
}

//-----------------------------------------------------------------------------
void OculusWorldDemoApp::ChangeDisplay ( bool bBackToWindowed, bool bNextFullscreen,
                                         bool bFullWindowDebugging )
{
    // Display mode switching doesn't make sense in App driver mode.
    if (!(Hmd->HmdCaps & ovrHmdCap_ExtendDesktop))
        return;

    // Exactly one should be set...
    OVR_ASSERT ( ( bBackToWindowed ? 1 : 0 ) + ( bNextFullscreen ? 1 : 0 ) +
                 ( bFullWindowDebugging ? 1 : 0 ) == 1 );
    OVR_UNUSED ( bNextFullscreen );

    if ( bFullWindowDebugging )
    {
        // Slightly hacky. Doesn't actually go fullscreen, just makes a screen-sized wndow.
        // This has higher latency than fullscreen, and is not intended for actual use, 
        // but makes for much nicer debugging on some systems.
        RenderParams = pRender->GetParams();
        RenderParams.Display = DisplayId(Hmd->DisplayDeviceName, Hmd->DisplayId);
        pRender->SetParams(RenderParams);

        pPlatform->SetMouseMode(Mouse_Normal);            
        pPlatform->SetFullscreen(RenderParams, pRender->IsFullscreen() ? Display_Window : Display_FakeFullscreen);
        pPlatform->SetMouseMode(Mouse_Relative); // Avoid mode world rotation jump.
   
        // If using an HMD, enable post-process (for distortion) and stereo.        
        if (RenderParams.IsDisplaySet() && pRender->IsFullscreen())
        {            
            //SetPostProcessingMode ( PostProcess );
        }
    }
    else
    {
        int screenCount = pPlatform->GetDisplayCount();

        int screenNumberToSwitchTo;
        if ( bBackToWindowed )
        {
            screenNumberToSwitchTo = -1;
        }
        else
        {
            if (!pRender->IsFullscreen())
            {
                // Currently windowed.
                // Try to find HMD Screen, making it the first screen in the full-screen cycle.
                FirstScreenInCycle = 0;
                if (!UsingDebugHmd)
                {
                    DisplayId HMD (Hmd->DisplayDeviceName, Hmd->DisplayId);
                    for (int i = 0; i< screenCount; i++)
                    {   
                        if (pPlatform->GetDisplay(i) == HMD)
                        {
                            FirstScreenInCycle = i;
                            break;
                        }
                    }            
                }
                ScreenNumber = FirstScreenInCycle;
                screenNumberToSwitchTo = ScreenNumber;
            }
            else
            {
                // Currently fullscreen, so cycle to the next screen.
                ScreenNumber++;
                if (ScreenNumber == screenCount)
                {
                    ScreenNumber = 0;
                }
                screenNumberToSwitchTo = ScreenNumber;
                if (ScreenNumber == FirstScreenInCycle)
                {
                    // We have cycled through all the fullscreen displays, so go back to windowed mode.
                    screenNumberToSwitchTo = -1;
                }
            }
        }

        // Always restore windowed mode before going to next screen, even if we were already fullscreen.
#if defined(OVR_OS_LINUX)
        LinuxFullscreenOnDevice = false;
        HmdSettingsChanged = true;
#endif
        pPlatform->SetFullscreen(RenderParams, Display_Window);
        if ( screenNumberToSwitchTo >= 0 )
        {
            // Go fullscreen.
            RenderParams.Display = pPlatform->GetDisplay(screenNumberToSwitchTo);
            pRender->SetParams(RenderParams);
            pPlatform->SetFullscreen(RenderParams, Display_Fullscreen);            
            
#if defined(OVR_OS_LINUX)
            DisplayId HMD (Hmd->DisplayDeviceName, Hmd->DisplayId);
            if (pPlatform->GetDisplay(screenNumberToSwitchTo) == HMD)
            {
                LinuxFullscreenOnDevice = true;
                HmdSettingsChanged = true;
            }
#endif
        }
    }

    
    // Updates render target pointers & sizes.
    HmdSettingChangeFreeRTs();    
}

void OculusWorldDemoApp::GamepadStateChanged(const GamepadState& pad)
{
    if (pad.Buttons != 0)
    {   // Dismiss Safety warning with any key.
        ovrHmd_DismissHSWDisplay(Hmd);
    }

    ThePlayer.GamepadMove   = Vector3f(pad.LX * pad.LX * (pad.LX > 0 ? 1 : -1),
                             0,
                             pad.LY * pad.LY * (pad.LY > 0 ? -1 : 1));
    ThePlayer.GamepadRotate = Vector3f(2 * pad.RX, -2 * pad.RY, 0);

    uint32_t gamepadDeltas = (pad.Buttons ^ LastGamepadState.Buttons) & pad.Buttons;

    if (gamepadDeltas)
    {
        Menu.OnGamepad(gamepadDeltas);
    }

    LastGamepadState = pad;
}


int OculusWorldDemoApp::HandleException(uintptr_t /*userValue*/, OVR::ExceptionHandler* /*pExceptionHandler*/, ExceptionInfo* /*pExceptionInfo*/, const char* reportFilePath)
{
    const char* uiText = ExceptionHandler::GetExceptionUIText(reportFilePath);

    if(uiText)
    {
        OVR::Util::DisplayMessageBox("Exception encountered in OculusWorldDemo", uiText);
        ExceptionHandler::FreeExceptionUIText(uiText);
    }

    return 0;
}





//-------------------------------------------------------------------------------------

OVR_PLATFORM_APP(OculusWorldDemoApp);
