/************************************************************************************

Filename    :   OculusWorldDemo.cpp
Content     :   First-person view test application for Oculus Rift - Implementation
Created     :   October 4, 2012
Authors     :   Michael Antonov, Andrew Reisse, Steve LaValle, Dov Katz
                Peter Hoff, Dan Goodman, Bryan Croteau                

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

#include "OculusWorldDemo.h"


//-------------------------------------------------------------------------------------
// ***** OculusWorldDemoApp

OculusWorldDemoApp::OculusWorldDemoApp()
    : pRender(0),
      WindowSize(1280,800),
      ScreenNumber(0),
      FirstScreenInCycle(0),
      Hmd(0),
      StartSensorCaps(0),
      UsingDebugHmd(false),
      LoadingState(LoadingState_Frame0),
      HaveVisionTracking(false),
      HmdStatus(0),
      
      // Initial location
      DistortionClearBlue(0),      
      ShiftDown(false),
      CtrlDown(false),
      HmdSettingsChanged(false),
      
      // Modifiable options.
      RendertargetIsSharedByBothEyes(false),
      DynamicRezScalingEnabled(false),
      ForceZeroIpd(false),
      DesiredPixelDensity(1.0f),
      FovSideTanMax(1.0f), // Updated based on Hmd.
      TimewarpEnabled(true),
      TimewarpRenderIntervalInSeconds(0.0f),
      FreezeEyeUpdate(false),
      FreezeEyeOneFrameRendered(false),
      CenterPupilDepthMeters(0.05f),
      ForceZeroHeadMovement(false),
      VsyncEnabled(true),
      MultisampleEnabled(true),
      IsLowPersistence(true),
      DynamicPrediction(true),
      PositionTrackingEnabled(true),

      // Scene state
      SceneMode(Scene_World),
      GridDisplayMode(GridDisplay_None),
      GridMode(Grid_Lens),
      TextScreen(Text_None),
      BlocksShowType(0),
      BlocksCenter(0.0f, 0.0f, 0.0f)     
{

    FPS             = 0.0f;
    SecondsPerFrame = 0.0f;
    FrameCounter    = 0;
    LastFpsUpdate   = 0;

    DistortionClearBlue = false;
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

    // *** Oculus HMD & Sensor Initialization

    // Create DeviceManager and first available HMDDevice from it.
    // Sensor object is created from the HMD, to ensure that it is on the
    // correct device.

    ovr_Initialize();

    Hmd = ovrHmd_Create(0);
    
    if (!Hmd)
    {
        // If we didn't detect an Hmd, create a simulated one for debugging.
        Hmd           = ovrHmd_CreateDebug(ovrHmd_DK1);
        UsingDebugHmd = true; 
        if (!Hmd)
        {   // Failed Hmd creation.
            return 1;
        }
    }

    // Get more details about the HMD.
    ovrHmd_GetDesc(Hmd, &HmdDesc);

    WindowSize = HmdDesc.Resolution;


    // ***** Setup System Window & rendering.

    if (!SetupWindowAndRendering(argc, argv))
        return 1;

    // Initialize FovSideTanMax, which allows us to change all Fov sides at once - Fov
    // starts at default and is clamped to this value.
    FovSideTanLimit = FovPort::Max(HmdDesc.MaxEyeFov[0], HmdDesc.MaxEyeFov[1]).GetMaxSideTan();
    FovSideTanMax   = FovPort::Max(HmdDesc.DefaultEyeFov[0], HmdDesc.DefaultEyeFov[1]).GetMaxSideTan();

    PositionTrackingEnabled = (HmdDesc.Caps & ovrHmdCap_Position) ? true : false;


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
    else if (!(ovrHmd_GetSensorState(Hmd, 0.0f).StatusFlags & ovrStatus_OrientationTracked))
        Menu.SetPopupMessage("NO SENSOR DETECTED");    
    else
        Menu.SetPopupMessage("Press F9 for Full-Screen on Rift");
    // Give first message 10 sec timeout, add border lines.
    Menu.SetPopupTimeout(10.0f, true);

    PopulateOptionMenu();

    // *** Identify Scene File & Prepare for Loading

    InitMainFilePath();  
    PopulatePreloadScene();
    
    LastUpdate = ovr_GetTimeInSeconds();
    
    return 0;
}


bool OculusWorldDemoApp::SetupWindowAndRendering(int argc, const char** argv)
{
    // *** Window creation

    if (!pPlatform->SetupWindow(WindowSize.w, WindowSize.h))    
        return false;    

    // Report relative mouse motion in OnMouseMove
    pPlatform->SetMouseMode(Mouse_Relative);


    // *** Initialize Rendering

    const char* graphics = "d3d11";

    // Select renderer based on command line arguments.
    for(int i = 1; i < argc; i++)
    {
        if(!strcmp(argv[i], "-r") && i < argc - 1)
        {
            graphics = argv[i + 1];
        }
        else if(!strcmp(argv[i], "-fs"))
        {
            RenderParams.Fullscreen = true;
        }
    }

    String title = "Oculus World Demo ";
    title += graphics;

    if (HmdDesc.ProductName[0])
    {
        title += " : ";
        title += HmdDesc.ProductName;
    }
    pPlatform->SetWindowTitle(title);

    // Enable multi-sampling by default.
    RenderParams.Display     = DisplayId(HmdDesc.DisplayDeviceName, HmdDesc.DisplayId);    
    RenderParams.Multisample = 1;
    //RenderParams.Fullscreen = true;
    pRender = pPlatform->SetupGraphics(OVR_DEFAULT_RENDER_DEVICE_SET,
                                       graphics, RenderParams);
    if (!pRender)
        return false;

    return true;
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
    float  degrees = 2.0f * atan(*var->AsFloat()) * (180.0f / Math<float>::Pi);
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
    Menu.AddBool( "Render Target.Zero IPD 'F7'",       &ForceZeroIpd).
                                                        AddShortcutKey(Key_F7).
                                                        SetNotify(this, &OWD::HmdSettingChangeFreeRTs);
    Menu.AddFloat("Render Target.Max Fov",             &FovSideTanMax, 0.2f, FovSideTanLimit, 0.02f,
                                                        "%.1f Degrees", 1.0f, &FormatMaxFromSideTan).
                                                        SetNotify(this, &OWD::HmdSettingChange).
                                                        AddShortcutUpKey(Key_I).AddShortcutDownKey(Key_K);
    Menu.AddFloat("Render Target.Pixel Density",    &DesiredPixelDensity, 0.5f, 1.5, 0.025f, "%3.2f", 1.0f).
                                                        SetNotify(this, &OWD::HmdSettingChange);
    Menu.AddEnum( "Render Target.Distortion Clear Color",  &DistortionClearBlue).
                                                        SetNotify(this, &OWD::DistortionClearColorChange).
                                                        AddEnumValue("Black",  0).
                                                        AddEnumValue("Blue", 1);    

    // Timewarp
    Menu.AddBool( "Timewarp.TimewarpEnabled 'O'",   &TimewarpEnabled).AddShortcutKey(Key_O).
                                                                    SetNotify(this, &OWD::HmdSettingChange);
    Menu.AddBool( "Timewarp.FreezeEyeUpdate 'C'",   &FreezeEyeUpdate).AddShortcutKey(Key_C);
    Menu.AddFloat("Timewarp.RenderIntervalSeconds", &TimewarpRenderIntervalInSeconds,
                                                    0.0001f, 1.00f, 0.0001f, "%.1f", 1.0f, &FormatTimewarp).
                                                    AddShortcutUpKey(Key_J).AddShortcutDownKey(Key_U);
    
    // First page properties
    Menu.AddFloat("User Eye Height",    &ThePlayer.UserEyeHeight, 0.2f, 2.5f, 0.02f,
                                        "%4.2f m").SetNotify(this, &OWD::EyeHeightChange).
                                        AddShortcutUpKey(Key_Equal).AddShortcutDownKey(Key_Minus);
    Menu.AddFloat("Center Pupil Depth", &CenterPupilDepthMeters, 0.0f, 0.2f, 0.001f,
                                        "%4.3f m").SetNotify(this, &OWD::CenterPupilDepthChange);
    Menu.AddBool("Body Relative Motion",&ThePlayer.bMotionRelativeToBody).AddShortcutKey(Key_E);    
    Menu.AddBool("Zero Head Movement",  &ForceZeroHeadMovement) .AddShortcutKey(Key_F7, ShortcutKey::Shift_RequireOn);
    Menu.AddBool("VSync 'V'",           &VsyncEnabled)          .AddShortcutKey(Key_V).SetNotify(this, &OWD::HmdSettingChange);
    Menu.AddBool("MultiSample 'F4'",    &MultisampleEnabled)    .AddShortcutKey(Key_F4).SetNotify(this, &OWD::MultisampleChange);
    
    // Add DK2 options to menu only for that headset.
    if (HmdDesc.Caps & ovrHmdCap_Position)
    {
        Menu.AddBool("Low Persistence 'P'",      &IsLowPersistence).     
                                                  AddShortcutKey(Key_P).SetNotify(this, &OWD::HmdSettingChange);
        Menu.AddBool("Dynamic Prediction",      &DynamicPrediction).     
                                                  SetNotify(this, &OWD::HmdSettingChange);
        Menu.AddBool("Positional Tracking 'X'",  &PositionTrackingEnabled).
                                                  AddShortcutKey(Key_X).SetNotify(this, &OWD::HmdSettingChange);
    }
}


void OculusWorldDemoApp::CalculateHmdValues()
{
    // Initialize eye rendering information for ovrHmd_Configure.
    // The viewport sizes are re-computed in case RenderTargetSize changed due to HW limitations.
    ovrEyeDesc eyes[2];
    eyes[0].Eye = ovrEye_Left;
    eyes[1].Eye = ovrEye_Right;
    eyes[0].Fov = HmdDesc.DefaultEyeFov[0];
    eyes[1].Fov = HmdDesc.DefaultEyeFov[1];

    // Clamp Fov based on our dynamically adjustable FovSideTanMax.
    // Most apps should use the default, but reducing Fov does reduce rendering cost.
    eyes[0].Fov = FovPort::Min(eyes[0].Fov, FovPort(FovSideTanMax));
    eyes[1].Fov = FovPort::Min(eyes[1].Fov, FovPort(FovSideTanMax));


    if (ForceZeroIpd)
    {
        // ForceZeroIpd does three things:
        //  1) Sets FOV to maximum symmetrical FOV based on both eyes
        //  2) Sets eye ViewAdjust values to 0.0 (effective IPD == 0)
        //  3) Uses only the Left texture for rendering.
        
        eyes[0].Fov = FovPort::Max(eyes[0].Fov, eyes[1].Fov);
        eyes[1].Fov = eyes[0].Fov;

        Sizei recommenedTexSize = ovrHmd_GetFovTextureSize(Hmd, ovrEye_Left,
                                                           eyes[0].Fov, DesiredPixelDensity);

        eyes[0].TextureSize         = EnsureRendertargetAtLeastThisBig(Rendertarget_Left,  recommenedTexSize);
        eyes[1].TextureSize         = eyes[0].TextureSize;
        eyes[0].RenderViewport.Pos  = Vector2i(0,0);
        eyes[0].RenderViewport.Size = Sizei::Min(eyes[0].TextureSize, recommenedTexSize);
        eyes[1].RenderViewport      = eyes[0].RenderViewport;

        // Store texture pointers that will be passed for rendering.
        EyeTexture[0] = RenderTargets[Rendertarget_Left].Tex;
        EyeTexture[1] = RenderTargets[Rendertarget_Left].Tex;
    }

    else
    {
        // Configure Stereo settings. Default pixel density is 1.0f.
        Sizei recommenedTex0Size = ovrHmd_GetFovTextureSize(Hmd, ovrEye_Left,  eyes[0].Fov, DesiredPixelDensity);
        Sizei recommenedTex1Size = ovrHmd_GetFovTextureSize(Hmd, ovrEye_Right, eyes[1].Fov, DesiredPixelDensity);

        if (RendertargetIsSharedByBothEyes)
        {
            Sizei  rtSize(recommenedTex0Size.w + recommenedTex1Size.w,
                          Alg::Max(recommenedTex0Size.h, recommenedTex1Size.h));

            // Use returned size as the actual RT size may be different due to HW limits.
            rtSize = EnsureRendertargetAtLeastThisBig(Rendertarget_BothEyes, rtSize);

            // Don't draw more then recommended size; this also ensures that resolution reported
            // in the overlay HUD size is updated correctly for FOV/pixel density change.            
            Sizei leftSize  = Sizei::Min(Sizei(rtSize.w/2, rtSize.h), recommenedTex0Size);
            Sizei rightSize = Sizei::Min(Sizei(rtSize.w/2, rtSize.h), recommenedTex1Size);

            eyes[0].TextureSize     = rtSize;
            eyes[1].TextureSize     = rtSize;
            eyes[0].RenderViewport  = Recti(Vector2i(0), leftSize);            
            eyes[1].RenderViewport  = Recti(Vector2i((rtSize.w+1)/2, 0), rightSize);

            // Store texture pointers that will be passed for rendering.
            // Same texture is used, but with different viewports.
            EyeTexture[0] = RenderTargets[Rendertarget_BothEyes].Tex;
            EyeTexture[1] = RenderTargets[Rendertarget_BothEyes].Tex;
            EyeTexture[0].Header.RenderViewport = eyes[0].RenderViewport;
            EyeTexture[1].Header.RenderViewport = eyes[1].RenderViewport;
        }

        else
        {
            eyes[0].TextureSize     = EnsureRendertargetAtLeastThisBig(Rendertarget_Left,  recommenedTex0Size);
            eyes[1].TextureSize     = EnsureRendertargetAtLeastThisBig(Rendertarget_Right, recommenedTex1Size);
            eyes[0].RenderViewport  = Recti(Sizei::Min(eyes[0].TextureSize, recommenedTex0Size));
            eyes[1].RenderViewport  = Recti(Sizei::Min(eyes[1].TextureSize, recommenedTex1Size));

            // Store texture pointers that will be passed for rendering.
            EyeTexture[0] = RenderTargets[Rendertarget_Left].Tex;
            EyeTexture[1] = RenderTargets[Rendertarget_Right].Tex;
        }
    }


    unsigned hmdCaps = ovrHmdCap_Orientation | (VsyncEnabled ? 0 : ovrHmdCap_NoVSync); 
    unsigned distortionCaps = ovrDistortion_Chromatic;

    ovrRenderAPIConfig config = pRender->Get_ovrRenderAPIConfig();

    if (TimewarpEnabled)
        distortionCaps |= ovrDistortion_TimeWarp;

    if (!ovrHmd_ConfigureRendering( Hmd, &config, hmdCaps, distortionCaps,
                                    eyes, EyeRenderDesc ))
    {
        // Fail exit? TBD
        return;
    }

    if (ForceZeroIpd)
    {
        // Remove IPD adjust
        EyeRenderDesc[0].ViewAdjust = Vector3f(0);
        EyeRenderDesc[1].ViewAdjust = Vector3f(0);
    }

    // ovrHmdCap_LatencyTest - enables internal latency feedback
    unsigned sensorCaps =  ovrHmdCap_Orientation|ovrHmdCap_YawCorrection|ovrHmdCap_LatencyTest;
    if (PositionTrackingEnabled)
        sensorCaps |= ovrHmdCap_Position;
    if (IsLowPersistence)
        sensorCaps |= ovrHmdCap_LowPersistence;
    if (DynamicPrediction)
        sensorCaps |= ovrHmdCap_DynamicPrediction;
    
    if (StartSensorCaps != sensorCaps)
    {
        ovrHmd_StartSensor(Hmd, sensorCaps, 0);
        StartSensorCaps = sensorCaps;
    }    

    // Calculate projections
    Projection[0] = ovrMatrix4f_Projection(EyeRenderDesc[0].Desc.Fov,  0.01f, 10000.0f, true);
    Projection[1] = ovrMatrix4f_Projection(EyeRenderDesc[1].Desc.Fov,  0.01f, 10000.0f, true);

    float    orthoDistance = 0.8f; // 2D is 0.8 meter from camera
    Vector2f orthoScale0   = Vector2f(1.0f) / Vector2f(EyeRenderDesc[0].PixelsPerTanAngleAtCenter);
    Vector2f orthoScale1   = Vector2f(1.0f) / Vector2f(EyeRenderDesc[1].PixelsPerTanAngleAtCenter);
    
    OrthoProjection[0] = ovrMatrix4f_OrthoSubProjection(Projection[0], orthoScale0, orthoDistance,
                                                        EyeRenderDesc[0].ViewAdjust.x);
    OrthoProjection[1] = ovrMatrix4f_OrthoSubProjection(Projection[1], orthoScale1, orthoDistance,
                                                        EyeRenderDesc[1].ViewAdjust.x);
}



// Returns the actual size present.
Sizei OculusWorldDemoApp::EnsureRendertargetAtLeastThisBig(int rtNum, Sizei requestedSize)
{
    OVR_ASSERT((rtNum >= 0) && (rtNum < Rendertarget_LAST));

    // Texture size that we already have might be big enough.
    Sizei newRTSize;

    RenderTarget& rt = RenderTargets[rtNum];
    if (!rt.pTex)
    {
        // Hmmm... someone nuked my texture. Rez change or similar. Make sure we reallocate.
        rt.Tex.Header.TextureSize = Sizei(0);
        newRTSize = requestedSize;
    }
    else
    {
        newRTSize = rt.Tex.Header.TextureSize;
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
    if (Sizei(rt.Tex.Header.TextureSize) != newRTSize)        
    {        
        rt.pTex = *pRender->CreateTexture(Texture_RGBA | Texture_RenderTarget | (MultisampleEnabled ? 4 : 1),
                                          newRTSize.w, newRTSize.h, NULL);
        rt.pTex->SetSampleMode(Sample_ClampBorder | Sample_Linear);


        // Configure texture for SDK Rendering.
        rt.Tex = rt.pTex->Get_ovrTexture();
    }

    return newRTSize;
}


//-----------------------------------------------------------------------------
// ***** Message Handlers

void OculusWorldDemoApp::OnResize(int width, int height)
{
    WindowSize = Sizei(width, height);
    // Re-calculate?
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
#ifndef OVR_OS_LINUX
        // Cycle through displays, going fullscreen on each one.
        if (!down) ChangeDisplay ( false, true, false );
        break;
#else
        // On Linux, fallthrough to F10/F11
#endif
        
#ifdef OVR_OS_MAC
     // F11 is reserved on Mac, F10 doesn't work on Windows
    case Key_F10:  
#else
    case Key_F11:
#endif
        if (!down) ChangeDisplay ( false, false, true );
        break;
        
    case Key_R:
        if (!down)
        {
            ovrHmd_ResetSensor(Hmd);
            Menu.SetPopupMessage("Sensor Fusion Reset");
        }
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
    Posef worldPose = ThePlayer.VirtualWorldPoseFromRealPose(pose);

    // Rotate and position View Camera
    Vector3f up      = worldPose.Orientation.Rotate(UpVector);
    Vector3f forward = worldPose.Orientation.Rotate(ForwardVector);

    // Transform the position of the center eye in the real world (i.e. sitting in your chair)
    // into the frame of the player's virtual body.

    // It is important to have head movement in scale with IPD.
    // If you shrink one, you should also shrink the other.
    // So with zero IPD (i.e. everything at infinity),
    // head movement should also be zero.
    Vector3f viewPos = ForceZeroHeadMovement ? ThePlayer.BodyPos : worldPose.Position;

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
        HmdSettingsChanged = false;
    }

    HmdFrameTiming = ovrHmd_BeginFrame(Hmd, 0);


    // Update gamepad.
    GamepadState gamepadState;
    if (GetPlatformCore()->GetGamepadManager()->GetGamepadState(0, &gamepadState))
    {
        GamepadStateChanged(gamepadState);
    }

    SensorState ss = ovrHmd_GetSensorState(Hmd, HmdFrameTiming.ScanoutMidpointSeconds);
    HmdStatus = ss.StatusFlags;

    // Change message status around positional tracking.
    bool hadVisionTracking = HaveVisionTracking;
    HaveVisionTracking = (ss.StatusFlags & Status_PositionTracked) != 0;
    if (HaveVisionTracking && !hadVisionTracking)
        Menu.SetPopupMessage("Vision Tracking Acquired");
    if (!HaveVisionTracking && hadVisionTracking)
        Menu.SetPopupMessage("Lost Vision Tracking");
    
    // Check if any new devices were connected.
    ProcessDeviceNotificationQueue();
    // FPS count and timing.
    UpdateFrameRateCounter(curtime);

    
    // Update pose based on frame!
    ThePlayer.HeadPose = ss.Predicted.Transform;
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

        if (ForceZeroIpd)
        {             
            // Zero IPD eye rendering: draw into left eye only,
            // re-use  texture for right eye.
            pRender->SetRenderTarget(RenderTargets[Rendertarget_Left].pTex);
            pRender->Clear();
        
            ovrPosef eyeRenderPose = ovrHmd_BeginEyeRender(Hmd, ovrEye_Left);
        
            View = CalculateViewFromPose(eyeRenderPose);
            RenderEyeView(ovrEye_Left);
            ovrHmd_EndEyeRender(Hmd, ovrEye_Left, eyeRenderPose, &EyeTexture[ovrEye_Left]);

            // Second eye gets the same texture (initialized to same value above).
            ovrHmd_BeginEyeRender(Hmd, ovrEye_Right); 
            ovrHmd_EndEyeRender(Hmd, ovrEye_Right, eyeRenderPose, &EyeTexture[ovrEye_Right]);
        }

        else if (RendertargetIsSharedByBothEyes)
        {
            // Shared render target eye rendering; set up RT once for both eyes.
            pRender->SetRenderTarget(RenderTargets[Rendertarget_BothEyes].pTex);
            pRender->Clear();

            for (int eyeIndex = 0; eyeIndex < ovrEye_Count; eyeIndex++)
            {      
                ovrEyeType eye = HmdDesc.EyeRenderOrder[eyeIndex];
                ovrPosef   eyeRenderPose = ovrHmd_BeginEyeRender(Hmd, eye);

                View = CalculateViewFromPose(eyeRenderPose);
                RenderEyeView(eye); 
                ovrHmd_EndEyeRender(Hmd, eye, eyeRenderPose, &EyeTexture[eye]);
            }
        }

        else
        {
            // Separate eye rendering - each eye gets its own render target.
            for (int eyeIndex = 0; eyeIndex < ovrEye_Count; eyeIndex++)
            {      
                ovrEyeType eye = HmdDesc.EyeRenderOrder[eyeIndex];
                pRender->SetRenderTarget(
                    RenderTargets[(eye == 0) ? Rendertarget_Left : Rendertarget_Right].pTex);
                pRender->Clear();
            
                ovrPosef eyeRenderPose = ovrHmd_BeginEyeRender(Hmd, eye);

                View = CalculateViewFromPose(eyeRenderPose);
                RenderEyeView(eye);            
                ovrHmd_EndEyeRender(Hmd, eye, eyeRenderPose, &EyeTexture[eye]);
            }
        }   

        pRender->SetRenderTarget(0);
        pRender->FinishScene();        
    }

    Profiler.RecordSample(RenderProfiler::Sample_AfterEyeRender);

    // TODO: These happen inside ovrHmd_EndFrame; need to hook into it.
    //Profiler.RecordSample(RenderProfiler::Sample_BeforeDistortion);
    ovrHmd_EndFrame(Hmd);
    pRender->Present(true);
    Profiler.RecordSample(RenderProfiler::Sample_AfterPresent);    
}



// Determine whether this frame needs rendering based on time-warp timing and flags.
bool OculusWorldDemoApp::FrameNeedsRendering(double curtime)
{    
    static double   lastUpdate          = 0.0;    
    double          renderInterval      = TimewarpRenderIntervalInSeconds;
    double          timeSinceLast       = curtime - lastUpdate;
    bool            updateRenderedView  = true;

    if (TimewarpEnabled)
    {
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
    }
        
    return updateRenderedView;
}


void OculusWorldDemoApp::ApplyDynamicResolutionScaling()
{
    if (!DynamicRezScalingEnabled)
    {
        // Restore viewport rectangle in case dynamic res scaling was enabled before.
        EyeTexture[0].Header.RenderViewport = EyeRenderDesc[0].Desc.RenderViewport;
        EyeTexture[1].Header.RenderViewport = EyeRenderDesc[1].Desc.RenderViewport;
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

    Sizei sizeLeft  = EyeRenderDesc[0].Desc.RenderViewport.Size;
    Sizei sizeRight = EyeRenderDesc[1].Desc.RenderViewport.Size;
    
    // This viewport is used for rendering and passed into ovrHmd_EndEyeRender.
    EyeTexture[0].Header.RenderViewport.Size = Sizei(int(sizeLeft.w  * dynamicRezScale),
                                                             int(sizeLeft.h  * dynamicRezScale));
    EyeTexture[1].Header.RenderViewport.Size = Sizei(int(sizeRight.w * dynamicRezScale),
                                                             int(sizeRight.h * dynamicRezScale));
}


void OculusWorldDemoApp::UpdateFrameRateCounter(double curtime)
{
    FrameCounter++;
    float secondsSinceLastMeasurement = (float)( curtime - LastFpsUpdate );

    if (secondsSinceLastMeasurement >= SecondsOfFpsMeasurement)
    {
        SecondsPerFrame = (float)( curtime - LastFpsUpdate ) / (float)FrameCounter;
        FPS             = 1.0f / SecondsPerFrame;
        LastFpsUpdate   = curtime;
        FrameCounter =   0;
    }
}


void OculusWorldDemoApp::RenderEyeView(ovrEyeType eye)
{
    Recti    renderViewport = EyeTexture[eye].Header.RenderViewport;
    Matrix4f viewAdjust     = Matrix4f::Translation(Vector3f(EyeRenderDesc[eye].ViewAdjust));


    // *** 3D - Configures Viewport/Projection and Render
    
    pRender->ApplyStereoParams(renderViewport, Projection[eye]);
    pRender->SetDepthMode(true, true);

    Matrix4f baseTranslate = Matrix4f::Translation(ThePlayer.BodyPos);
    Matrix4f baseYaw       = Matrix4f::RotationY(ThePlayer.BodyYaw.Get());


    if (GridDisplayMode != GridDisplay_GridOnly)
    {
        if (SceneMode != Scene_OculusCubes)
        {
            MainScene.Render(pRender, viewAdjust * View);        
            RenderAnimatedBlocks(eye, ovr_GetTimeInSeconds());
        }
        
        if (SceneMode == Scene_Cubes)
        {
            // Draw scene cubes overlay. Red if position tracked, blue otherwise.
            Scene sceneCubes = (HmdStatus & ovrStatus_PositionTracked) ?
                               RedCubesScene : BlueCubesScene;        
            sceneCubes.Render(pRender, viewAdjust * View * baseTranslate * baseYaw);
        }

        else if (SceneMode == Scene_OculusCubes)
        {
            OculusCubesScene.Render(pRender, viewAdjust * View * baseTranslate * baseYaw);
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
    "Spacebar                 \t500 Toggle debug info overlay\n"
    "W, S                    \t500 Move forward, back\n"
    "A, D                     \t500 Strafe left, right\n"
    "Mouse move             \t500 Look left, right\n"
    "Left gamepad stick     \t500 Move\n"
    "Right gamepad stick    \t500 Turn\n"
    "T                        \t500 Reset player position";
    
static const char* HelpText2 =        
    "R              \t250 Reset sensor orientation\n"
    "G                 \t250 Cycle grid overlay mode\n"
    "-, +           \t250 Adjust eye height\n"
    "Esc            \t250 Cancel full-screen\n"
    "F4                \t250 Multisampling toggle\n"    
    "F9             \t250 Hardware full-screen (low latency)\n"
    "F11            \t250 Faked full-screen (easier debugging)\n"
    "Ctrl+Q            \t250 Quit";


void FormatLatencyReading(char* buff, UPInt size, float val)
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
        char buf[512], gpustat[256];

        // Average FOVs.
        FovPort leftFov  = EyeRenderDesc[0].Desc.Fov;
        FovPort rightFov = EyeRenderDesc[1].Desc.Fov;
        
        // Rendered size changes based on selected options & dynamic rendering.
        int pixelSizeWidth = EyeTexture[0].Header.RenderViewport.Size.w +
                             ((!ForceZeroIpd) ?
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
                            " PostPresent: %s  ",
                            latencyText0, latencyText1, latencyText2);
            }
        }

        ThePlayer.HeadPose.Orientation.GetEulerAngles<Axis_Y, Axis_X, Axis_Z>(&hmdYaw, &hmdPitch, &hmdRoll);
        OVR_sprintf(buf, sizeof(buf),
                    " HMD YPR:%4.0f %4.0f %4.0f   Player Yaw: %4.0f\n"
                    " FPS: %.1f  ms/frame: %.1f Frame: %d\n"
                    " Pos: %3.2f, %3.2f, %3.2f  HMD: %s\n"
                    " EyeHeight: %3.2f, IPD: %3.1fmm\n" //", Lens: %s\n"
                    " FOV %3.1fx%3.1f, Resolution: %ix%i\n"
                    "%s",
                    RadToDegree(hmdYaw), RadToDegree(hmdPitch), RadToDegree(hmdRoll),
                    RadToDegree(ThePlayer.BodyYaw.Get()),
                    FPS, SecondsPerFrame * 1000.0f, FrameCounter,
                    ThePlayer.BodyPos.x, ThePlayer.BodyPos.y, ThePlayer.BodyPos.z,
                    //GetDebugNameHmdType ( TheHmdRenderInfo.HmdType ),
                    HmdDesc.ProductName,
                    ThePlayer.UserEyeHeight,
                    ovrHmd_GetFloat(Hmd, OVR_KEY_IPD, 0) * 1000.0f,
                    //( EyeOffsetFromNoseLeft + EyeOffsetFromNoseRight ) * 1000.0f,
                    //GetDebugNameEyeCupType ( TheHmdRenderInfo.EyeCups ),  // Lens/EyeCup not exposed
                    
                    (leftFov.GetHorizontalFovDegrees() + rightFov.GetHorizontalFovDegrees()) * 0.5f,
                    (leftFov.GetVerticalFovDegrees() + rightFov.GetVerticalFovDegrees()) * 0.5f,

                    pixelSizeWidth, pixelSizeHeight,

                    latency2Text
                    );

        size_t texMemInMB = pRender->GetTotalTextureMemoryUsage() / 1058576;
        if (texMemInMB)
        {
            OVR_sprintf(gpustat, sizeof(gpustat), " GPU Tex: %u MB", texMemInMB);
            OVR_strcat(buf, sizeof(buf), gpustat);
        }
        
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

void OculusWorldDemoApp::HmdSettingChangeFreeRTs(OptionVar*)
{
    HmdSettingsChanged = true;
    // Cause the RTs to be recreated with the new mode.
    for ( int rtNum = 0; rtNum < Rendertarget_LAST; rtNum++ )
        RenderTargets[rtNum].pTex = NULL;        
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


//-----------------------------------------------------------------------------

void OculusWorldDemoApp::ProcessDeviceNotificationQueue()
{
    // TBD: Process device plug & Unplug     
}


//-----------------------------------------------------------------------------
void OculusWorldDemoApp::ChangeDisplay ( bool bBackToWindowed, bool bNextFullscreen,
                                         bool bFullWindowDebugging )
{
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
        RenderParams.Display = DisplayId(HmdDesc.DisplayDeviceName, HmdDesc.DisplayId);
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
                    DisplayId HMD (HmdDesc.DisplayDeviceName, HmdDesc.DisplayId);
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
        pPlatform->SetFullscreen(RenderParams, Display_Window);
        if ( screenNumberToSwitchTo >= 0 )
        {
            // Go fullscreen.
            RenderParams.Display = pPlatform->GetDisplay(screenNumberToSwitchTo);
            pRender->SetParams(RenderParams);
            pPlatform->SetFullscreen(RenderParams, Display_Fullscreen);            
        }
    }

    
    // Updates render target pointers & sizes.
    HmdSettingChangeFreeRTs();    
}

void OculusWorldDemoApp::GamepadStateChanged(const GamepadState& pad)
{
    ThePlayer.GamepadMove   = Vector3f(pad.LX * pad.LX * (pad.LX > 0 ? 1 : -1),
                             0,
                             pad.LY * pad.LY * (pad.LY > 0 ? -1 : 1));
    ThePlayer.GamepadRotate = Vector3f(2 * pad.RX, -2 * pad.RY, 0);

    UInt32 gamepadDeltas = (pad.Buttons ^ LastGamepadState.Buttons) & pad.Buttons;

    if (gamepadDeltas)
    {
        Menu.OnGamepad(gamepadDeltas);
    }

    LastGamepadState = pad;
}


//-------------------------------------------------------------------------------------

OVR_PLATFORM_APP(OculusWorldDemoApp);
