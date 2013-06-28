/************************************************************************************

Filename    :   Win32_Platform.cpp
Content     :   Win32 implementation of Platform app infrastructure
Created     :   September 6, 2012
Authors     :   Andrew Reisse

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

************************************************************************************/

#include "Kernel/OVR_System.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_String.h"

#include "GLUT_Platform.h"
#include "../Render/Render_Device.h"
#include "SDL_Gamepad.h"

namespace OVR { namespace Platform { namespace Glut {


PlatformCore* INSTANCE;

PlatformCore::PlatformCore(Application* app)
  : Platform::PlatformCore(app), Quit(0), MMode(Mouse_Normal), windowId(0), Modifiers(0), WindowTitle("App")
{
    INSTANCE = this;
    pGamepadManager = *new SDL::GamepadManager();
}

PlatformCore::~PlatformCore()
{
}

void PlatformCore::displayFunc() {
    INSTANCE->pApp->OnDisplay();
}

void PlatformCore::idleFunc() {
    INSTANCE->pApp->OnIdle();
}

void PlatformCore::timerFunc(int value) {
    INSTANCE->pApp->OnIdle();
    glutTimerFunc(100, timerFunc, 0);
}

void PlatformCore::reshapeFunc(int w, int h) {
    INSTANCE->pApp->OnResize(w, h);
}

void PlatformCore::mouseFunc(int button, int state, int x, int y) {
    //INSTANCE->pApp->OnResize(w, h);
}

void PlatformCore::keyFunc(unsigned char key, int x, int y) {
    //INSTANCE->pApp->OnResize(w, h);
}



bool PlatformCore::SetupWindow(int w, int h)
{
	Width = w;
	Height = h;
    glutInitWindowSize(w, h);
    windowId = glutCreateWindow("SensorBox");
    glutDisplayFunc(displayFunc);
    glutReshapeFunc(reshapeFunc);
//    glutIdleFunc(idleFunc);
    glutTimerFunc(100, timerFunc, 0);
    glutKeyboardFunc(keyFunc);
    glutMouseFunc(mouseFunc);
    glutShowWindow();
    return true;
}


void PlatformCore::DestroyWindow()
{
    // Release renderer.
    pRender.Clear();

    // Release gamepad.
    pGamepadManager.Clear();

    glutDestroyWindow( windowId );

    Width = Height = 0;
}

void PlatformCore::ShowWindow(bool visible)
{
    if (visible) {
        glutShowWindow( );
    } else {
        glutHideWindow();
    }
}

void PlatformCore::SetMouseMode(MouseMode mm)
{
    if (mm == MMode)
        return;

    MMode = mm;
}


void PlatformCore::GetWindowSize(int* w, int* h) const
{
    *w = Width;
    *h = Height;
}


void PlatformCore::SetWindowTitle(const char* title)
{
    WindowTitle = title;
    glutSetWindowTitle( title );
    glutSetIconTitle( title );
}

int PlatformCore::Run()
{
    glutMainLoop();
    return ExitCode;
}



RenderDevice* PlatformCore::SetupGraphics(const SetupGraphicsDeviceSet& setupGraphicsDesc,
                                          const char* type, const Render::RendererParams& rp)
{
    const SetupGraphicsDeviceSet* setupDesc = setupGraphicsDesc.PickSetupDevice(type);
    OVR_ASSERT(setupDesc);

    pRender = *setupDesc->pCreateDevice(rp, &windowId);
    if (pRender)
        pRender->SetWindowSize(Width, Height);

    return pRender.GetPtr();
}


void PlatformCore::PlayMusicFile(const char *fileName)
{
}


// Returns the number of active screens for extended displays and 1 for mirrored display
int PlatformCore::GetDisplayCount()
{
    return 1;
}

//-----------------------------------------------------------------------------
// Returns the device name for the given screen index or empty string for invalid index
// The zero index will always return the primary screen name
Render::DisplayId PlatformCore::GetDisplay(int screen)
{
    Render::DisplayId did;
    return did;
}


}}}


int main(int argc, char ** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    using namespace OVR;
    using namespace OVR::Platform;

    // CreateApplication must be the first call since it does OVR::System::Initialize.
    Application*     app = Application::CreateApplication();
    Glut::PlatformCore* platform = new Glut::PlatformCore(app);

    // The platform attached to an app will be deleted by DestroyApplication.
    app->SetPlatformCore(platform);

    int exitCode = 0;
    exitCode = app->OnStartup(argc, argv);
    if (!exitCode)
        exitCode = platform->Run();

    // No OVR functions involving memory are allowed after this.
    Application::DestroyApplication(app);
    app = 0;

    OVR_DEBUG_STATEMENT(_CrtDumpMemoryLeaks());
    return exitCode;
}
