/************************************************************************************

Filename    :   OculusRoomTiny_Linux.cpp
Content     :   First-person view test application for Oculus Rift
Created     :   November 6, 2013
Authors     :   Brad Davis

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

#include "OculusRoomTiny.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/Xrandr.h>
#include <X11/keysymdef.h>
#include <X11/XF86keysym.h>
#include "OVR_KeyCodes.h"

Display* x_display = XOpenDisplay(NULL);
Window x_root = DefaultRootWindow(x_display);

//-------------------------------------------------------------------------------------
// ***** Win32-Specific Logic

typedef struct MotifHints {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long inputMode;
    unsigned long status;
} Hints;

bool OculusRoomTinyApp::setupWindow()
{
    XSetWindowAttributes swa;
    swa.event_mask = ExposureMask | StructureNotifyMask | PointerMotionMask | KeyPressMask | KeyReleaseMask;
    hWnd = XCreateWindow(x_display, x_root, 0, 0, HMDInfo.HResolution, HMDInfo.VResolution, 0,
            CopyFromParent, InputOutput, CopyFromParent, CWEventMask, &swa);

    XSetWindowAttributes xattr;
    xattr.override_redirect = 0;
    XChangeWindowAttributes(x_display, hWnd, CWOverrideRedirect, &xattr);

    {
        XWMHints hints;
        hints.input = 1;
        hints.flags = InputHint;
        XSetWMHints(x_display, hWnd, &hints);
    }

    {
        //code to remove decoration
        MotifHints hints;
        hints.flags = 2;
        hints.decorations = 0;
        Atom property = XInternAtom(x_display, "_MOTIF_WM_HINTS", true);
        XChangeProperty(x_display, hWnd, property, property, 32, PropModeReplace, (unsigned char *) &hints, 5);
    }

    // make the window visible on the screen
    XMapWindow(x_display, hWnd);
    XStoreName(x_display, hWnd, "OculusRoomTiny");
    XMoveWindow(x_display, hWnd, HMDInfo.DesktopX, HMDInfo.DesktopY);

    // get identifiers for the provided atom name strings
    Atom wm_state = XInternAtom(x_display, "_NET_WM_STATE", 0);

    XEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.xclient.window = hWnd;
    xev.xclient.message_type = wm_state;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = 1;
    xev.xclient.data.l[1] = 0;
    XSendEvent(x_display, x_root, 0, SubstructureNotifyMask, &xev);
    return (hWnd != 0);
}

void OculusRoomTinyApp::destroyWindow()
{
    pRender.Clear();

    if (hWnd)
    {
        // Release window resources.
        XDestroyWindow(x_display, hWnd);
        hWnd = 0;
        Width = Height = 0;
    }
}

unsigned int ovrKeyForX11Key(KeySym key) {
    if (key >= XK_A && key <= XK_Z) {
        int offset = (key - XK_A);
        return Key_A + offset;
    }
    if (key >= XK_a && key <= XK_z) {
        int offset = (key - XK_a);
        return Key_A + offset;
    }

    if (key >= XK_0 && key <= XK_9) {
        return Key_Num0 + (key - XK_0);
    }
    if (key >= XK_F1 && key <= XK_F12) {
        return Key_F1 + (key - XK_F1);
    }
    if (key >= XK_Left && key <= XK_Down) {
        return Key_Left + (key - XK_Left);
    }

    switch (key) {
    case XK_Escape:
        return Key_Escape;

    case XK_Shift_L:
    case XK_Shift_R:
        return Key_Shift;

    case XK_Control_L:
    case XK_Control_R:
        return Key_Shift;

    case XK_KP_Add:
        return Key_KP_Add;

    case XK_KP_Subtract:
        return Key_KP_Subtract;

    case XK_Page_Up:
    case XK_KP_Page_Up:
        return Key_PageUp;

    case XK_Page_Down:
    case XK_KP_Page_Down:
        return Key_PageUp;

    case XK_Home:
        return Key_Home;
    case XK_End:
        return Key_End;
    case XK_Delete:
        return Key_Delete;
    case XK_Insert:
        return Key_Insert;
    }
    return Key_None;
}

bool OculusRoomTinyApp::processXEvents() {
    bool exit = false;

    // Pump all messages from X server. Keypresses are directed to keyfunc (if defined)
    while (XPending(x_display)) {
        XEvent xev;
        XNextEvent(x_display, &xev);
        if (xev.type == KeyPress || xev.type == KeyRelease) {
            KeySym key = XLookupKeysym(&xev.xkey, 0);
            // Convert the platform key code to the OVR keycode
            unsigned int ovrKey = ovrKeyForX11Key(key);
            OnKey(ovrKey, xev.type == KeyPress);
        }
        if (xev.type == DestroyNotify)
            exit = true;
    }
    return !exit;
}

int OculusRoomTinyApp::Run()
{
    pRender->SetWindowSize(HMDInfo.HResolution, HMDInfo.VResolution);
    pRender->SetViewport(0, 0, HMDInfo.HResolution, HMDInfo.VResolution);
    while (!Quit) {
        processXEvents();
        pApp->OnIdle();
    }
    return 0;
}


//-------------------------------------------------------------------------------------
// ***** Program Startup

int main(int argc, char ** argv)
{
    int exitCode = 0;

    // Initializes LibOVR. This LogMask_All enables maximum logging.
    // Custom allocator can also be specified here.
    OVR::System::Init(OVR::Log::ConfigureDefaultLog(OVR::LogMask_All));

    // Scope to force application destructor before System::Destroy.
    {
        OculusRoomTinyApp app(0);
        //app.hInstance = hinst;

        exitCode = app.OnStartup(0);
        if (!exitCode)
        {
            // Processes messages and calls OnIdle() to do rendering.
            exitCode = app.Run();
        }
    }

    // No OVR functions involving memory are allowed after this.
    OVR::System::Destroy();

    return exitCode;
}
