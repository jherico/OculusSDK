/************************************************************************************

Filename    :   Platform_Linux.h
Content     :   Linux (X11) implementation of Platform app infrastructure
Created     :   September 6, 2012
Authors     :   Andrew Reisse

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

************************************************************************************/

#ifndef OVR_Linux_Platform_h
#define OVR_Linux_Platform_h

#include "Platform.h"
#include "../Render/Render_GL_Device.h"

#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace OVR { namespace Render {
    class RenderDevice;
}}

namespace OVR { namespace OvrPlatform { namespace Linux {

struct XDisplayInfo;

class PlatformCore : public OvrPlatform::PlatformCore
{
    Recti StartVP;
    bool  HasWM;

    int             IndexOf(Render::DisplayId id);
    XDisplayInfo    getXDisplayInfo(Render::DisplayId id);

public:
    struct _XDisplay* Disp;
    XVisualInfo*      Vis;
    Window            Win;
    int               FBConfigID;
    GLXWindow         GLXWin;

    bool         Quit;
    int          ExitCode;
    int          Width, Height;

    MouseMode    MMode;
    Cursor       InvisibleCursor;

    enum
    {
        WM_PROTOCOLS,
        WM_DELETE_WINDOW,
        NumAtoms
    };
    Atom Atoms[NumAtoms];

    void processEvent(XEvent& event);

    Render::RenderDevice* SetupGraphics_GL(const Render::RendererParams& rp);

    void showCursor(bool show);
    bool determineScreenOffset(int screenId, int* screenOffsetX, int* screenOffsetY);
    void showWindowDecorations(bool show);

public:
    PlatformCore(Application* app);
    ~PlatformCore();

    void*     SetupWindow(int w, int h);
    void      Exit(int exitcode) { Quit = 1; ExitCode = exitcode; }

    RenderDevice* SetupGraphics(const SetupGraphicsDeviceSet& setupGraphicsDesc,
                                const char* gtype, const Render::RendererParams& rp);

    void      SetMouseMode(MouseMode mm);
    void      GetWindowSize(int* w, int* h) const;

    void      SetWindowTitle(const char*title);

    void      ShowWindow(bool show);
    void      DestroyWindow();
    bool      SetFullscreen(const Render::RendererParams& rp, int fullscreen);
    int       GetDisplayCount();
    Render::DisplayId GetDisplay(int screen);

    int  Run();
};

}}
namespace Render { namespace GL { namespace Linux {

class RenderDevice : public Render::GL::RenderDevice
{
protected:
    struct _XDisplay* Disp;
    Window            Win;
    GLXContext        Context;

public:
    RenderDevice(const Render::RendererParams& p, struct _XDisplay* disp, Window w, GLXContext gl)
    : GL::RenderDevice(p), Disp(disp), Win(w), Context(gl) {}

    virtual void Shutdown();
    virtual void Present(bool withVsync);
    virtual ovrRenderAPIConfig Get_ovrRenderAPIConfig() const;

    // oswnd = Linux::PlatformCore*
    static Render::RenderDevice* CreateDevice(const RendererParams& rp, void* oswnd);
};

}}}}


#endif // OVR_Linux_Platform_h
