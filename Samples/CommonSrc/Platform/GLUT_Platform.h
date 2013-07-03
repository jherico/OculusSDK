/************************************************************************************

Filename    :   Win32_Platform.h
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

#ifndef OVR_Win32_Platform_h
#define OVR_Win32_Platform_h

#include "Platform.h"
#include <GL/glut.h>

namespace OVR { namespace Render {
    class RenderDevice;
    struct DisplayId;
}}

namespace OVR { namespace Platform { namespace Glut {

class PlatformCore : public Platform::PlatformCore
{
    bool        Quit;
    int         ExitCode;
    int         Width, Height;
    int         windowId;

    MouseMode   MMode;
    int         Modifiers;
    String      WindowTitle;

public:
    PlatformCore(Application* app);
    ~PlatformCore();

    bool      SetupWindow(int w, int h);
    void      DestroyWindow();
    void      ShowWindow(bool visible);
    void      Exit(int exitcode) { Quit = 1; ExitCode = exitcode; }

    RenderDevice* SetupGraphics(const SetupGraphicsDeviceSet& setupGraphicsDesc,
                                const char* type,
                                const Render::RendererParams& rp);

    void      SetMouseMode(MouseMode mm);
    void      GetWindowSize(int* w, int* h) const;

    void      SetWindowTitle(const char*title);
    void      PlayMusicFile(const char *fileName);
    int       GetDisplayCount();
    Render::DisplayId    GetDisplay(int screen);

    int       Run();

    static void displayFunc();
    static void idleFunc();
    static void timerFunc(int timer);
    static void reshapeFunc(int w, int h);
    static void mouseFunc(int button, int state, int x, int y);
    static void keyFunc(unsigned char key, int x, int y);
};


}}}



// OVR_PLATFORM_APP_ARGS specifies the Application class to use for startup,
// providing it with startup arguments.
#define OVR_PLATFORM_APP_ARGS(AppClass, args)                                            \
OVR::Platform::Application* OVR::Platform::Application::CreateApplication()          \
{ OVR::System::Init(OVR::Log::ConfigureDefaultLog(OVR::LogMask_All));                \
return new AppClass args; }                                                        \
void OVR::Platform::Application::DestroyApplication(OVR::Platform::Application* app) \
{ OVR::Platform::PlatformCore* platform = app->pPlatform;                            \
delete app; delete platform; OVR::System::Destroy(); };

// OVR_PLATFORM_APP_ARGS specifies the Application startup class with no args.
#define OVR_PLATFORM_APP(AppClass) OVR_PLATFORM_APP_ARGS(AppClass, ())


#endif // OVR_Win32_Platform_h
