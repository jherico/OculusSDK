/************************************************************************************

Filename    :   Platform_Linux.cpp
Content     :   Linux (X11) implementation of Platform app infrastructure
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
#include "Kernel/OVR_Timer.h"
#include "OVR_CAPI_GL.h"

#include "Linux_Platform.h"
#include "Linux_Gamepad.h"

// Renderers
#include "../Render/Render_GL_Device.h"

#include "../../3rdParty/EDID/edid.h"
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/Xrandr.h>


namespace OVR { namespace Platform { namespace Linux {

static const char *AtomNames[] = {"WM_PROTOCOLS", "WM_DELETE_WINDOW"};

PlatformCore::PlatformCore(Application* app)
    : Platform::PlatformCore(app), Disp(NULL), Win(0), Vis(NULL), Quit(0), MMode(Mouse_Normal)    
    , StartVP(0, 0, 0, 0)
{
    pGamepadManager = *new Linux::GamepadManager();
}
PlatformCore::~PlatformCore()
{
    XFreeCursor(Disp, InvisibleCursor);

    if (Disp)
        XCloseDisplay(Disp);
}

// Setup an X11 window in windowed mode.
bool PlatformCore::SetupWindow(int w, int h)
{

    if (!Disp)
    {
        XInitThreads();

        Disp = XOpenDisplay(NULL);
        if (!Disp)
        {
            OVR_DEBUG_LOG(("XOpenDisplay failed."));
            return false;
        }

        XInternAtoms(Disp, const_cast<char**>(AtomNames), NumAtoms, false, Atoms);
    }

    XSetWindowAttributes winattr;
    unsigned attrmask = CWEventMask | CWBorderPixel;

    winattr.event_mask = ButtonPressMask|ButtonReleaseMask|KeyPressMask|KeyReleaseMask|ButtonMotionMask|PointerMotionMask|
        /*PointerMotionHintMask|*/StructureNotifyMask;//|ExposureMask;
    winattr.border_pixel = 0;

    int screenNumber = DefaultScreen(Disp);

    if (!Vis)
    {
        int attr[16];
        int nattr = 2;

        attr[0] = GLX_RGBA;
        attr[1] = GLX_DOUBLEBUFFER;
        attr[nattr++] = GLX_DEPTH_SIZE;
        attr[nattr++] = 24;
        attr[nattr] = 0;

        Vis = glXChooseVisual(Disp, screenNumber, attr);

        if (!Vis)
        {
            OVR_DEBUG_LOG(("glXChooseVisual failed."));
            return false;
        }
    }

    Window rootWindow = XRootWindow(Disp, Vis->screen);

    winattr.colormap = XCreateColormap(Disp, rootWindow, Vis->visual, AllocNone);
    attrmask |= CWColormap;


    Win = XCreateWindow(Disp, rootWindow, 0, 0, w, h, 0, Vis->depth,
                        InputOutput, Vis->visual, attrmask, &winattr);

    if (!Win)
    {
        OVR_DEBUG_LOG(("XCreateWindow failed."));
        return false;
    }


    XStoreName(Disp, Win, "OVR App");
    XSetWMProtocols(Disp, Win, &Atoms[WM_DELETE_WINDOW], 1);

    // Intialize empty cursor for show/hide.
    XColor black;
    static char noData[] = { 0,0,0,0,0,0,0,0 };
    black.red = black.green = black.blue = 0;

    Pixmap bitmapNoData = XCreateBitmapFromData(Disp, Win, noData, 8, 8);
    InvisibleCursor = XCreatePixmapCursor(Disp, bitmapNoData, bitmapNoData,
                                         &black, &black, 0, 0);
    XDefineCursor(Disp, Win, InvisibleCursor);

    Width = w;
    Height = h;

    return true;
}

void PlatformCore::SetMouseMode(MouseMode mm)
{
    if (mm == MMode)
        return;

    if (Win)
    {
        if (mm == Mouse_Relative)
        {
            XWarpPointer(Disp, Win, Win, 0,0,Width,Height, Width/2, Height/2);
        }
        else
        {
            //if (MMode == Mouse_Relative)
            //  ShowCursor(TRUE);
        }
    }
    MMode = mm;
}

void PlatformCore::GetWindowSize(int* w, int* h) const
{
    *w = Width;
    *h = Height;
}

void PlatformCore::SetWindowTitle(const char* title)
{
    XStoreName(Disp, Win, title);
}
    
void PlatformCore::ShowWindow(bool show)
{
    if (show)
        XRaiseWindow(Disp, Win);
    else
        XIconifyWindow(Disp, Win, 0);
}

void PlatformCore::DestroyWindow()
{
    if (Win)
        XDestroyWindow(Disp, Win);
    Win = 0;
}


static int KeyMap[][2] =
{
    { XK_BackSpace,      Key_Backspace },
    { XK_Tab,       Key_Tab },
    { XK_Clear,     Key_Clear },
    { XK_Return,    Key_Return },
    { XK_Shift_L,     Key_Shift },
    { XK_Control_L,   Key_Control },
    { XK_Alt_L,      Key_Alt },
    { XK_Shift_R,     Key_Shift },
    { XK_Control_R,   Key_Control },
    { XK_Alt_R,      Key_Alt },
    { XK_Pause,     Key_Pause },
    { XK_Caps_Lock,   Key_CapsLock },
    { XK_Escape,    Key_Escape },
    { XK_space,     Key_Space },
    { XK_Page_Up,     Key_PageUp },
    { XK_Page_Down,      Key_PageDown },
    { XK_Prior,     Key_PageUp },
    { XK_Next,      Key_PageDown },
    { XK_End,       Key_End },
    { XK_Home,      Key_Home },
    { XK_Left,      Key_Left },
    { XK_Up,        Key_Up },
    { XK_Right,     Key_Right },
    { XK_Down,      Key_Down },
    { XK_Insert,    Key_Insert },
    { XK_Delete,    Key_Delete },
    { XK_Help,      Key_Help },
    { XK_Num_Lock,   Key_NumLock },
    { XK_Scroll_Lock,    Key_ScrollLock },
};


static KeyCode MapXKToKeyCode(unsigned vk)
{
    unsigned key = Key_None;

    if ((vk >= 'a') && (vk <= 'z'))
    {
        key = vk - 'a' + Key_A;
    }
    else if ((vk >= ' ') && (vk <= '~'))
    {
        key = vk;
    }
    else if ((vk >= XK_KP_0) && (vk <= XK_KP_9))
    {
        key = vk - XK_KP_0 + Key_KP_0;
    }
    else if ((vk >= XK_F1) && (vk <= XK_F15))
    {
        key = vk - XK_F1 + Key_F1;
    }
    else 
    {
        for (unsigned i = 0; i< (sizeof(KeyMap) / sizeof(KeyMap[1])); i++)
        {
            if (vk == KeyMap[i][0])
            {                
                key = KeyMap[i][1];
                break;
            }
        }
    }

    return (KeyCode)key;
}

static int MapModifiers(int xmod)
{
    int mod = 0;
    if (xmod & ShiftMask)
        mod |= Mod_Shift;
    if (xmod & ControlMask)
        mod |= Mod_Control;
    if (xmod & Mod1Mask)
        mod |= Mod_Alt;
    if (xmod & Mod4Mask)
        mod |= Mod_Meta;
    return mod;
}

void PlatformCore::processEvent(XEvent& event)
{
    switch (event.xany.type)
    {
    case ConfigureNotify:
        if (event.xconfigure.width != Width || event.xconfigure.height != Height)
        {
            Width = event.xconfigure.width;
            Height = event.xconfigure.height;
            pApp->OnResize(Width, Height);

            if (pRender)
                pRender->SetWindowSize(Width, Height);
        }
        break;

    case KeyPress:
    case KeyRelease:
        {
            char chars[8] = {0};
            KeySym xk;
            XComposeStatus comp;
            XLookupString(&event.xkey, chars, sizeof(chars), &xk, &comp);
            if (xk != XK_VoidSymbol)
                pApp->OnKey(MapXKToKeyCode((unsigned)xk), chars[0], event.xany.type == KeyPress, MapModifiers(event.xkey.state));
            if (xk == XK_Escape && MMode == Mouse_Relative)
            {
                //ungrab
                MMode = Mouse_RelativeEscaped;
                showCursor(true);
            }
        }
        break;

    case MotionNotify:
        if (MMode == Mouse_Relative)
        {
            int dx = event.xmotion.x - Width/2;
            int dy = event.xmotion.y - Height/2;

            // do not remove this check, WarpPointer generates events too.
            if (dx == 0 && dy == 0)
                break;

            XWarpPointer(Disp, Win, Win, 0,0,Width,Height, Width/2, Height/2);
            pApp->OnMouseMove(dx, dy, Mod_MouseRelative|MapModifiers(event.xmotion.state));
        }
        else
        {
            pApp->OnMouseMove(event.xmotion.x, event.xmotion.y, MapModifiers(event.xmotion.state));
        }
        break;

    case MapNotify:
        if (MMode == Mouse_Relative)
        {            
            XWarpPointer(Disp, Win, Win, 0,0,Width,Height, Width/2, Height/2);
            showCursor(false);
        }
        break;

    case ButtonPress:
        if (event.xbutton.button == 1)
        {
            //grab

            if (MMode == Mouse_RelativeEscaped)
            {            
                XWarpPointer(Disp, Win, Win, 0,0,Width,Height, Width/2, Height/2);
                showCursor(false);
                MMode = Mouse_Relative;
            }
        }
        break;

    case FocusOut:
        if (MMode == Mouse_Relative)
        {            
            MMode = Mouse_RelativeEscaped;
            showCursor(true);
        }
        break;

    case ClientMessage:
        if (event.xclient.message_type == Atoms[WM_PROTOCOLS] &&
            Atom(event.xclient.data.l[0]) == Atoms[WM_DELETE_WINDOW])
            pApp->OnQuitRequest();
        break;
    }
}

int PlatformCore::Run()
{
    while (!Quit)
    {
        if (XPending(Disp))
        {
            XEvent event;
            XNextEvent(Disp, &event);

            if (pApp && event.xany.window == Win)
                processEvent(event);
        }
        else
        {
            pApp->OnIdle();
        }
    }

    return ExitCode;
}

bool PlatformCore::determineScreenOffset(int screenId, int* screenOffsetX, int* screenOffsetY)
{
    Display* display = XOpenDisplay(NULL);

    bool foundScreen = false;

    if (display)
    {
        int numberOfScreens;
        XineramaScreenInfo* screens = XineramaQueryScreens(display, &numberOfScreens);

        if (screenId < numberOfScreens)
        {
            XineramaScreenInfo screenInfo = screens[screenId];
            *screenOffsetX = screenInfo.x_org;
            *screenOffsetY = screenInfo.y_org;

            foundScreen = true;
        }

        XFree(screens);
    }

    return foundScreen;
}

void PlatformCore::showWindowDecorations(bool show)
{
    // Declaration of 'MOTIF_WM_HINTS' struct and flags can be found at:
    // https://people.gnome.org/~tthurman/docs/metacity/xprops_8h-source.html
    typedef struct WMHints
    {
        unsigned long   flags;
        unsigned long   functions;
        unsigned long   decorations;
        long            inputMode;
        unsigned long   status;
    } Hints;

    #define MWM_DECOR_ALL           (1L << 0)
    #define MWM_DECOR_BORDER        (1L << 1)
    #define MWM_DECOR_RESIZEH       (1L << 2)
    #define MWM_DECOR_TITLE         (1L << 3)
    #define MWM_DECOR_MENU          (1L << 4)
    #define MWM_DECOR_MINIMIZE      (1L << 5)
    #define MWM_DECOR_MAXIMIZE      (1L << 6)

    Atom property = XInternAtom(Disp, "_MOTIF_WM_HINTS", true);

    Hints hints;
    hints.flags = 2;    // We only want to specify decoration.

    if (show)
    {
        hints.decorations = MWM_DECOR_BORDER | MWM_DECOR_TITLE | MWM_DECOR_MENU | MWM_DECOR_MINIMIZE | MWM_DECOR_MAXIMIZE;
    }
    else
    {
        // Remove all window border items.
        hints.decorations = 0;
    }

    XChangeProperty(Disp,Win,property,property,32,PropModeReplace,(unsigned char *)&hints,5);
}

int PlatformCore::IndexOf(Render::DisplayId id)
{
    RROutput primaryOutput = XRRGetOutputPrimary(Disp, DefaultRootWindow(Disp));
    if (id.CgDisplayId == 0 && primaryOutput != 0)
        return 0;

    int index = (primaryOutput != 0) ? 0 : -1;
    XRRScreenResources *screen = XRRGetScreenResources(Disp, Win);
    for (int i = 0; i < screen->noutput && i <= id.CgDisplayId; ++i)
    {
        RROutput output = screen->outputs[i];
        MonitorInfo * mi = read_edid_data(Disp, output);
        if (mi != NULL && (primaryOutput == 0 || output != primaryOutput))
            ++index;

        delete mi;
    }
    XRRFreeScreenResources(screen);

    return index;
}

bool PlatformCore::SetFullscreen(const Render::RendererParams& rp, int fullscreen)
{
    if (fullscreen == pRender->GetParams().Fullscreen)
        return false;

    int displayIndex = IndexOf(rp.Display);
    OVR_DEBUG_LOG(("Display %d has index %d", rp.Display.CgDisplayId, displayIndex));

    if (pRender->GetParams().Fullscreen == Render::Display_Window)
    {
        // Save the original window size and position so we can restore later.

        XWindowAttributes xwa;
        XGetWindowAttributes(Disp, Win, &xwa);
        int x, y;
        Window unused;
        XTranslateCoordinates(Disp, Win, DefaultRootWindow(Disp), xwa.x, xwa.y, &x, &y, &unused);

        StartVP.w = Width;
        StartVP.h = Height;
        StartVP.x = x;
        StartVP.y = y;
    }
    else if (pRender->GetParams().Fullscreen == Render::Display_Fullscreen)
    {
//        if (StartMode == NULL)
//            return true;

//        XF86VidModeSwitchToMode(Disp, 0, StartMode);
//        XF86VidModeSetViewPort(Disp, Win, 0, 0);
        {
            XEvent xev;
            memset(&xev, 0, sizeof(xev));

            xev.type = ClientMessage;
            xev.xclient.window = Win;
            xev.xclient.message_type = XInternAtom( Disp, "_NET_WM_STATE", False);
            xev.xclient.format = 32;
            xev.xclient.data.l[0] = 0;
            xev.xclient.data.l[1] = XInternAtom( Disp, "_NET_WM_STATE_FULLSCREEN", False);
            xev.xclient.data.l[2] = 0;

            XSendEvent( Disp, DefaultRootWindow( Disp ), False, SubstructureNotifyMask, &xev);
        }

//        XWindowAttributes windowattr;
//        XGetWindowAttributes(Disp, Win, &windowattr);
//        Width = windowattr.width;
//        Height = windowattr.height;
//        pApp->OnResize(Width, Height);

//        if (pRender)
//            pRender->SetWindowSize(Width, Height);

        XWindowChanges wc;
        wc.width = StartVP.w;
        wc.height = StartVP.h;
        wc.x = StartVP.x;
        wc.y = StartVP.y;

        XConfigureWindow(Disp, Win, CWWidth | CWHeight | CWX | CWY, &wc);

        showWindowDecorations(false);
    }

    if (fullscreen == Render::Display_FakeFullscreen)
    {
        // Transitioning from windowed to fake fullscreen.
        int xOffset;
        int yOffset;

        if (!determineScreenOffset(displayIndex, &xOffset, &yOffset))
        {
            return false;
        }

        showWindowDecorations(false);

        XMoveWindow(Disp, Win, xOffset, yOffset);
        XMapRaised(Disp, Win);
    }
    else if (fullscreen == Render::Display_Window)
    {
        // Transitioning from fake fullscreen to windowed.
        showWindowDecorations(true);

        XMoveWindow(Disp, Win, StartVP.x, StartVP.y);
        XMapRaised(Disp, Win);
    }
    else if (fullscreen == Render::Display_Fullscreen)
    {
        // Move, size, and decorate the window for fullscreen.

        int xOffset;
        int yOffset;

        if (!determineScreenOffset(displayIndex, &xOffset, &yOffset))
            return false;

        showWindowDecorations(false);

        XWindowChanges wc;
        wc.x = xOffset;
        wc.y = yOffset;
        wc.stack_mode = 0;

        XConfigureWindow(Disp, Win, CWX | CWY | CWStackMode, &wc);

        // Change the display mode.

//        XF86VidModeModeInfo **modes = NULL;
//        int modeNum = 0;
//        if (!XF86VidModeGetAllModeLines(Disp, 0, &modeNum, &modes))
//            return false;

//        OVR_ASSERT(modeNum > 0);
//        StartMode = modes[0];

//        int bestMode = -1;
//        for (int i = 0; i < modeNum; i++)
//        {
//            if ((modes[i]->hdisplay == Width) && (modes[i]->vdisplay == Height))
//                bestMode = i;
//        }

//        if (bestMode == -1)
//            return false;

//        XF86VidModeSwitchToMode(Disp, 0, modes[bestMode]);
        //XF86VidModeSetViewPort(Disp, Win, 0, 0);

        // Make the window fullscreen in the window manager.

        {
            XEvent xev;
            memset(&xev, 0, sizeof(xev));

            xev.type = ClientMessage;
            xev.xclient.window = Win;
            xev.xclient.message_type = XInternAtom( Disp, "_NET_WM_STATE", False);
            xev.xclient.format = 32;
            xev.xclient.data.l[0] = 1;
            xev.xclient.data.l[1] = XInternAtom( Disp, "_NET_WM_STATE_FULLSCREEN", False);
            xev.xclient.data.l[2] = 0;

            XSendEvent( Disp, DefaultRootWindow( Disp ), False, SubstructureNotifyMask, &xev);
        }
    }

    XMapRaised(Disp, Win);

    Platform::PlatformCore::SetFullscreen(rp, fullscreen);
    return true;
}

int PlatformCore::GetDisplayCount()
{
    int numberOfScreens = 0;
    XineramaScreenInfo* screens = XineramaQueryScreens(Disp, &numberOfScreens);
    XFree(screens);
    return numberOfScreens;
}

Render::DisplayId PlatformCore::GetDisplay(int screenId)
{
    char device_id[32] = "";
    int displayId = 0;

    int screensPassed = 0;

    RROutput primaryOutput = XRRGetOutputPrimary(Disp, DefaultRootWindow(Disp));

    XRRScreenResources *screen = XRRGetScreenResources(Disp, Win);
    for (int i = 0; i < screen->noutput; ++i)
    {
        RROutput output = screen->outputs[i];
        MonitorInfo * mi = read_edid_data(Disp, output);
        if (mi == NULL)
            continue;

        bool isMyScreen =
                (primaryOutput == 0) ? screensPassed++ > screenId :
                                       (output == primaryOutput) ? screenId == 0 :
                                                                   ++screensPassed >= screenId;
        if (isMyScreen)
        {
            OVR_sprintf(device_id, 32, "%s%04d", mi->manufacturer_code, mi->product_code);
            displayId = (screenId == 0 && primaryOutput != 0) ? 0 : i; // Require 0 to return the primary display
            delete mi;
            break;
        }

        delete mi;
    }
    XRRFreeScreenResources(screen);

    return Render::DisplayId(device_id, displayId);
}

RenderDevice* PlatformCore::SetupGraphics(const SetupGraphicsDeviceSet& setupGraphicsDesc,
                                          const char* type, const Render::RendererParams& rp)
{
    const SetupGraphicsDeviceSet* setupDesc = setupGraphicsDesc.PickSetupDevice(type);
    OVR_ASSERT(setupDesc);
        
    pRender = *setupDesc->pCreateDevice(rp, this);
    if (pRender)
        pRender->SetWindowSize(Width, Height);
        
    return pRender.GetPtr();
}

void PlatformCore::showCursor(bool show)
{
    if (show)
    {
        XUndefineCursor(Disp, Win);
    }
    else
    {
        XDefineCursor(Disp, Win, InvisibleCursor);
    }
}

}}

// GL
namespace Render { namespace GL { namespace Linux {

ovrRenderAPIConfig RenderDevice::Get_ovrRenderAPIConfig() const
{
    static ovrGLConfig cfg;
    cfg.OGL.Header.API         = ovrRenderAPI_OpenGL;
    cfg.OGL.Header.RTSize      = Sizei(WindowWidth, WindowHeight);
    cfg.OGL.Header.Multisample = Params.Multisample;
    cfg.OGL.Win                = Win;
    cfg.OGL.Disp               = Disp;
    return cfg.Config;
}

Render::RenderDevice* RenderDevice::CreateDevice(const RendererParams& rp, void* oswnd)
{
    Platform::Linux::PlatformCore* PC = (Platform::Linux::PlatformCore*)oswnd;

    GLXContext context = glXCreateContext(PC->Disp, PC->Vis, 0, GL_TRUE);

    if (!context)
        return NULL;

    if (!glXMakeCurrent(PC->Disp, PC->Win, context))
    {
        glXDestroyContext(PC->Disp, context);
        return NULL;
    }

    XMapRaised(PC->Disp, PC->Win);

    InitGLExtensions();

    XSync(PC->Disp, 0);

    for (XWindowAttributes attribute; attribute.map_state != IsViewable; )
         XGetWindowAttributes(PC->Disp,PC->Win,&attribute);

    XSetInputFocus(PC->Disp, PC->Win, RevertToParent, CurrentTime);

    return new Render::GL::Linux::RenderDevice(rp, PC->Disp, PC->Win, context);
}

void RenderDevice::Present(bool withVsync)
{
    unsigned swapInterval = (withVsync) ? 1 : 0;
    GLuint currentSwapInterval = 0;
    glXQueryDrawable(Disp, Win, GLX_SWAP_INTERVAL_EXT, &currentSwapInterval);
    if (currentSwapInterval != swapInterval)
        glXSwapIntervalEXT(Disp, Win, swapInterval);

    glXSwapBuffers(Disp, Win);
}

void RenderDevice::Shutdown()
{
    if (Context)
    {
        glXMakeCurrent(Disp, 0, NULL);
        glXDestroyContext(Disp, Context);
        Context = NULL;
        Win = 0;
    }
}

}}}}


int main(int argc, const char* argv[])
{
    using namespace OVR;
    using namespace OVR::Platform;

    // CreateApplication must be the first call since it does OVR::System::Initialize.
    Application*       app = Application::CreateApplication();
    Linux::PlatformCore* platform = new Linux::PlatformCore(app);
    // The platform attached to an app will be deleted by DestroyApplication.
    app->SetPlatformCore(platform);

    int exitCode = app->OnStartup(argc, argv);
    if (!exitCode)
        exitCode = platform->Run();

    // No OVR functions involving memory are allowed after this.
    Application::DestroyApplication(app);
    app = 0;

    return exitCode;
}
