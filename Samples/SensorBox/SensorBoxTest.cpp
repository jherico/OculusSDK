/************************************************************************************

Filename    :   SensorBoxTest.h
Content     :   Visual orientaion sensor test app; renders a rotating box over axes.
Created     :   October 1, 2012
Authors     :   Michael Antonov

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

#include "OVR.h"
#include "Kernel/OVR_String.h"

#include "../CommonSrc/Platform/Platform_Default.h"
#include "../CommonSrc/Render/Render_Device.h"
#include "../CommonSrc/Platform/Gamepad.h"

using namespace OVR;
using namespace OVR::Platform;
using namespace OVR::Render;

//-------------------------------------------------------------------------------------
// ***** SensorBoxTest Description

// This application renders an axes-colored box that rotates with sensor input. App allows
// user to toggle views for debugging purposes by pressing F1, F2, F3 keys.
// Application further allows running multiple sensors at once to compare sensor quality.
//
// The Right-handed coordinate system is defines as follows (as seen in perspective view):
//  Y - Up    (colored red)
//  Z - Back  (Out from screen, colored blue)
//  X - Right (green)
//  All cameras are looking at the origin.  

// Camera view types.
enum ViewType
{
    View_Perspective,
    View_XZ_UpY,
    View_XY_DownZ,
    View_Count
};


//-------------------------------------------------------------------------------------

class InputTestApp : public Application
{
    RenderDevice*      pRender;

    Ptr<DeviceManager> pManager;
    Ptr<HMDDevice>     pHMD;
    Ptr<SensorDevice>  pSensor;
    Ptr<SensorDevice>  pSensor2;    

    SensorFusion       SFusion;
    SensorFusion       SFusion2;
    
    double          LastUpdate;
    ViewType        CurrentView;

    double          LastTitleUpdate;

    Matrix4f        Proj;
    Matrix4f        View;
    Scene           Sc;
    Ptr<Model>      pAxes;   // Model of the coordinate system
    Ptr<Container>  pBox;    // Rendered box
    Ptr<Container>  pBox2;   // Second model (right now just lines)

    // Applies specified projection/lookAt direction to the scene.
    void            SetView(ViewType view);

public:

    InputTestApp();
    ~InputTestApp();

    virtual int  OnStartup(int argc, const char** argv);
    virtual void OnIdle();

    virtual void OnMouseMove(int x, int y, int modifiers);
    virtual void OnKey(KeyCode key, int chr, bool down, int modifiers);
};

InputTestApp::InputTestApp()
    : pRender(0), CurrentView(View_Perspective),
      LastUpdate(0), LastTitleUpdate(0), pAxes(0), pBox(0)
{

}


/*
void UseCase()
{
    using namespace OVR;

    OVR::System::Init();

    Ptr<DeviceManager> pManager = 0;
    Ptr<HMDDevice>     pHMD = 0;
    Ptr<SensorDevice>  pSensor = 0;
    SensorFusion       FusionResult;

     
    // *** Initialization - Create the first available HMD Device
    pManager = *DeviceManager::Create();
    pHMD     = *pManager->EnumerateDevices<HMDDevice>().CreateDevice();
    if (!pHMD)
        return;
    pSensor  = *pHMD->GetSensor();

    // Get DisplayDeviceName, ScreenWidth/Height, etc..
    HMDInfo hmdInfo;
    pHMD->GetDeviceInfo(&hmdInfo);
    
    if (pSensor)
        FusionResult.AttachToSensor(pSensor);

    // *** Per Frame
    // Get orientation quaternion to control view
    Quatf q = FusionResult.GetOrientation();

    // Create a matrix from quaternion,
    // where elements [0][0] through [3][3] contain rotation.
    Matrix4f bodyFrameMatrix(q); 

    // Get Euler angles from quaternion, in specified axis rotation order.
    float yaw, pitch, roll;
    q.GetEulerAngles<Axis_Y, Axis_X, Axis_Z>(&yaw, &pitch, &roll);

    // *** Shutdown
    pSensor.Clear();
    pHMD.Clear();
    pManager.Clear();

    OVR::System::Destroy();
}
*/

InputTestApp::~InputTestApp()
{
    pSensor.Clear();
    pManager.Clear();
    
}


int InputTestApp::OnStartup(int argc, const char** argv)
{
    if (!pPlatform->SetupWindow(1200,800))
        return 1;
    

    pManager = *DeviceManager::Create();
    
    // This initialization logic supports running two sensors at the same time.
   
    DeviceEnumerator<SensorDevice> isensor = pManager->EnumerateDevices<SensorDevice>();
    DeviceEnumerator<SensorDevice> oculusSensor;
    DeviceEnumerator<SensorDevice> oculusSensor2;
    
    while(isensor)
    {
        DeviceInfo di;
        if (isensor.GetDeviceInfo(&di))
        {
            if (strstr(di.ProductName, "Tracker"))
            {
                if (!oculusSensor)
                    oculusSensor = isensor;
                else if (!oculusSensor2)
                    oculusSensor2 = isensor;
            }
        }

        isensor.Next();
    }

    if (oculusSensor)
    {
        pSensor = *oculusSensor.CreateDevice();

        if (pSensor)
            pSensor->SetRange(SensorRange(4 * 9.81f, 8 * Math<float>::Pi, 1.0f), true);

        if (oculusSensor2)
        {
            // Second Oculus sensor, useful for comparing firmware behavior & settings.
            pSensor2 = *oculusSensor2.CreateDevice();

            if (pSensor2)
                pSensor2->SetRange(SensorRange(4 * 9.81f, 8 * Math<float>::Pi, 1.0f), true);
        }
    }

    oculusSensor.Clear();
    oculusSensor2.Clear();
       
    
    /*
    DeviceHandle hHMD = pManager->EnumerateDevices<HMDDevice>();
    HMDInfo      hmdInfo;
    if (hHMD)
    {        
        hHMD.GetDeviceInfo(&hmdInfo);
    }
    */

    if (pSensor)
        SFusion.AttachToSensor(pSensor);
    if (pSensor2)
        SFusion2.AttachToSensor(pSensor2);

    /*
    // Test rotation: This give rotations clockwise (CW) while looking from
    // origin in the direction of the axis.

    Vector3f xV(1,0,0);
    Vector3f zV(0,0,1);

    Vector3f rxV = Matrix4f::RotationZ(DegreeToRad(10.0f)).Transform(xV);
    Vector3f ryV = Matrix4f::RotationY(DegreeToRad(10.0f)).Transform(xV);
    Vector3f rzV = Matrix4f::RotationX(DegreeToRad(10.0f)).Transform(zV);
    */

    // Report relative mouse motion (not absolute position)
   // pPlatform->SetMouseMode(Mouse_Relative);

    const char* graphics = "d3d10";
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "-r") && i < argc-1)
            graphics = argv[i+1];

    pRender = pPlatform->SetupGraphics(OVR_DEFAULT_RENDER_DEVICE_SET, graphics,
                                       RendererParams());
  
    //WireframeFill = pRender->CreateSimpleFill(Fill::F_Wireframe);


    
    // *** Rotating Box
    
    pBox = *new Container;
    pBox->Add(Ptr<Model>(        
       *Model::CreateAxisFaceColorBox(-2.0f, 2.0f, Color(0,   0xAA, 0),        // x = green
                                      -1.0f, 1.0f, Color(0xAA,0,    0),        // y = red
                                      -1.0f, 1.0f, Color(0,   0,    0xAA)) )); // z = blue 
    // Drop-down line from box, to make it easier to see differences in angle.
    Ptr<Model> downLine = *new Model(Prim_Lines);
    downLine->AddLine(Vertex(0.0f,-4.5f, 0.0f, 0xFFE0B0B0),
                      Vertex(0.0f, 0.0f, 0.0f, 0xFFE0B0B0));
    pBox->Add(downLine);
    Sc.World.Add(pBox);

    
    // Secondary rotating coordinate object, if we have two values.
    if (pSensor2)
    {
        pBox2 = *new Container;

        // Drop-down line from box, to make it easier to see differences in angle.
        Ptr<Model> lines = *new Model(Prim_Lines);
        lines->AddLine(Vertex( 0.0f,-4.0f, 0.0f, 0xFFA07070),  // -Y
                       Vertex( 0.0f, 0.0f, 0.0f, 0xFFA07070));
        lines->AddLine(Vertex(-4.0f, 0.0f, 0.0f, 0xFF70A070),  // -X
                       Vertex( 0.0f, 0.0f, 0.0f, 0xFF70A070));
        lines->AddLine(Vertex( 0.0f, 0.0f,-4.0f, 0xFF7070A0),  // -Z
                       Vertex( 0.0f, 0.0f, 0.0f, 0xFF7070A0));
        pBox2->Add(lines);
        Sc.World.Add(pBox2);
    }


    // *** World axis X,Y,Z rendering.

    pAxes = *new Model(Prim_Lines);
    pAxes->AddLine(Vertex(-8.0f, 0.0f, 0.0f, 0xFF40FF40),
                   Vertex( 8.0f, 0.0f, 0.0f, 0xFF40FF40)); // X
    pAxes->AddLine(Vertex( 7.6f, 0.4f, 0.4f, 0xFF40FF40),
                   Vertex( 8.0f, 0.0f, 0.0f, 0xFF40FF40)); // X - arrow
    pAxes->AddLine(Vertex( 7.6f,-0.4f,-0.4f, 0xFF40FF40),
                   Vertex( 8.0f, 0.0f, 0.0f, 0xFF40FF40)); // X - arrow

    pAxes->AddLine(Vertex( 0.0f,-8.0f, 0.0f, 0xFFFF4040),
                   Vertex( 0.0f, 8.0f, 0.0f, 0xFFFF4040)); // Y
    pAxes->AddLine(Vertex( 0.4f, 7.6f, 0.0f, 0xFFFF4040),
                   Vertex( 0.0f, 8.0f, 0.0f, 0xFFFF4040)); // Y - arrow
    pAxes->AddLine(Vertex(-0.4f, 7.6f, 0.0f, 0xFFFF4040),
                   Vertex( 0.0f, 8.0f, 0.0f, 0xFFFF4040)); // Y
    
    pAxes->AddLine(Vertex( 0.0f, 0.0f,-8.0f, 0xFF4040FF),
                   Vertex( 0.0f, 0.0f, 8.0f, 0xFF4040FF)); // Z
    pAxes->AddLine(Vertex( 0.4f, 0.0f, 7.6f, 0xFF4040FF),
                   Vertex( 0.0f, 0.0f, 8.0f, 0xFF4040FF)); // Z - arrow
    pAxes->AddLine(Vertex(-0.4f, 0.0f, 7.6f, 0xFF4040FF),
                   Vertex( 0.0f, 0.0f, 8.0f, 0xFF4040FF)); // Z - arrow
    Sc.World.Add(pAxes);
   

    SetView(CurrentView);


    LastUpdate = pPlatform->GetAppTime();
    return 0;
}

void InputTestApp::SetView(ViewType type)
{
    switch(type)
    {
    case View_Perspective:
        View = Matrix4f::LookAtRH(Vector3f(5.0f, 4.0f, 10.0f), // eye
                                  Vector3f(0.0f, 1.5f, 0.0f),   // at
                                  Vector3f(0.0f, 1.0f, 0.0f));
        break;

    case View_XY_DownZ: // F2
        View = Matrix4f::LookAtRH(Vector3f(0.0f, 0.0f, 10.0f),  // eye
                                  Vector3f(0.0f, 0.0f, 0.0f),   // at
                                  Vector3f(0.0f, 1.0f, 0.0f));
        break;

     case View_XZ_UpY:
        View = Matrix4f::LookAtRH(Vector3f(0.0f,-10.0f, 0.0f),   // eye
                                  Vector3f(0.0f,  0.0f, 0.0f),   // at
                                  Vector3f(0.0f,  0.0f, 1.0f));
        
        break;
        default:
            break;
    }

    Proj = Matrix4f::PerspectiveRH(DegreeToRad(70.0f), 1280 / (float)800,
                                   0.3f, 1000.0f);  // LH
}


void InputTestApp::OnMouseMove(int x, int y, int modifiers)
{
    OVR_UNUSED3(x, y, modifiers);
}


static float CalcDownAngleDegrees(Quatf q)
{
    Vector3f downVector(0.0f, -1.0f, 0.0f);    
    Vector3f val= q.Rotate(downVector);
    return RadToDegree(downVector.Angle(val));
}

void InputTestApp::OnKey(KeyCode key, int chr, bool down, int modifiers)
{
    OVR_UNUSED2(chr, modifiers);

    switch (key)
    {
    case Key_Q:
        if (!down)
            pPlatform->Exit(0);
        break;

    case Key_F1:
        CurrentView = View_Perspective;
        SetView(CurrentView);
        //UpdateWindowTitle();
        break;
    case Key_F2:
        CurrentView = View_XY_DownZ;
        SetView(CurrentView);
        break;
    case Key_F3:
        CurrentView = View_XZ_UpY;
        SetView(CurrentView);
        break;

    case Key_R:
        if (down)
        {
            SFusion.Reset();
            SFusion2.Reset();
        }
        break;

    case Key_H:
        if (down && pSensor)
        {
            SensorDevice::CoordinateFrame coord = pSensor->GetCoordinateFrame();
            pSensor->SetCoordinateFrame(
                (coord == SensorDevice::Coord_Sensor) ?
                SensorDevice::Coord_HMD : SensorDevice::Coord_Sensor);
            SFusion.Reset();
            SFusion2.Reset();
        }
        break;

    case Key_G:
        if (down)
        {
            SFusion.SetGravityEnabled(!SFusion.IsGravityEnabled());
            SFusion2.SetGravityEnabled(SFusion.IsGravityEnabled());
        }
        break;

    case Key_A:

        if (down)
        {
            if (!pSensor2)
            {
                LogText("Angle: %2.3f\n", CalcDownAngleDegrees(SFusion.GetOrientation()));
            }
            else
            {
                LogText("Angle: %2.3f Secondary Sensor Angle: %2.3f\n",
                        CalcDownAngleDegrees(SFusion.GetOrientation()),
                        CalcDownAngleDegrees(SFusion2.GetOrientation()));
            }                        
        }
        break;

        /*
    case Key_End:
        if (!down)
        {
            OriAdjust = OriSensor.Conj();
            Sc.ViewPoint.SetOrientation(Quatf());
        }
        break; */
        default:
            break;
    }
}

void InputTestApp::OnIdle()
{
    double curtime = pPlatform->GetAppTime();
 //   float  dt      = float(LastUpdate - curtime);
    LastUpdate     = curtime;
    
    if (pBox)
    {
        Quatf q = SFusion.GetOrientation();
        pBox->SetOrientation(q);

   // Test Euler conversion, alternative to the above:
   //     Vector3f euler;
   //     SFusion.GetOrientation().GetEulerABC<Axis_Y, Axis_X, Axis_Z, Rotate_CCW, Handed_R>(&euler.y, &euler.x, &euler.z);
   //     Matrix4f mat = Matrix4f::RotationY(euler.y) * Matrix4f::RotationX(euler.x) * Matrix4f::RotationZ(euler.z);
   //  pBox->SetMatrix(mat);    

        // Update titlebar every 20th of a second.
        if ((curtime - LastTitleUpdate) > 0.05f)
        {
            char                          titleBuffer[512];
            SensorDevice::CoordinateFrame coord = SensorDevice::Coord_Sensor;
            if (pSensor)
                coord = pSensor->GetCoordinateFrame();

            OVR_sprintf(titleBuffer, 512, "OVR SensorBox %s %s  Ang: %0.3f",
                        (SFusion.IsGravityEnabled() ?  "" : "[Grav Off]"),
                        (coord == SensorDevice::Coord_HMD) ? "[HMD Coord]" : "",
                        CalcDownAngleDegrees(q));
            pPlatform->SetWindowTitle(titleBuffer);
            LastTitleUpdate = curtime;
        }
    }

    if (pBox2)
    {
        pBox2->SetOrientation(SFusion2.GetOrientation());
    }

    // Render
    int w, h;
    pPlatform->GetWindowSize(&w, &h);

    pRender->SetViewport(0, 0, w, h);

    pRender->Clear();
    pRender->BeginScene();

    pRender->SetProjection(Proj);
    pRender->SetDepthMode(1,1);
    
    Sc.Render(pRender, View);

    pRender->Present();

}

OVR_PLATFORM_APP(InputTestApp);
