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
    swa.event_mask = ExposureMask | StructureNotifyMask | PointerMotionMask | KeyPressMask;
    // FIXME don't hardcode the dimensions
    hWnd = XCreateWindow(x_display, x_root, 0, 0, 1280, 800, 0,
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

//    {
//        //code to remove decoration
//        MotifHints hints;
//        hints.flags = 2;
//        hints.decorations = 0;
//        Atom property = XInternAtom(x_display, "_MOTIF_WM_HINTS", true);
//        XChangeProperty(x_display, win, property, property, 32, PropModeReplace, (unsigned char *) &hints, 5);
//    }

    // make the window visible on the screen
    XMapWindow(x_display, hWnd);
    XStoreName(x_display, hWnd, "OculusRoomTiny");
    XMoveWindow(x_display, hWnd, 0, 0);

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

bool OculusRoomTinyApp::processXEvents() {
    bool exit = false;

    // Pump all messages from X server. Keypresses are directed to keyfunc (if defined)
    while (XPending(x_display)) {
        XEvent xev;
        XNextEvent(x_display, &xev);
        if (xev.type == KeyPress) {
            KeySym key;
            char text;
            if (XLookupString(&xev.xkey, &text, 1, &key, 0) == 1) {
//                if (!keys(text, 0, 0)) {
//                    exit = true;
//                }
            }
        }
        if (xev.type == DestroyNotify)
            exit = true;
    }
    return !exit;
}

//LRESULT OculusRoomTinyApp::windowProc(UINT msg, WPARAM wp, LPARAM lp)
//{
//    switch (msg)
//    {
//    case WM_MOUSEMOVE:
//        {
//            if (MouseCaptured)
//            {
//                // Convert mouse motion to be relative (report the offset and re-center).
//                POINT newPos = { LOWORD(lp), HIWORD(lp) };
//                ::ClientToScreen(hWnd, &newPos);
//                if ((newPos.x == WindowCenter.x) && (newPos.y == WindowCenter.y))
//                    break;
//                ::SetCursorPos(WindowCenter.x, WindowCenter.y);
//
//                LONG dx = newPos.x - WindowCenter.x;
//                LONG dy = newPos.y - WindowCenter.y;
//                pApp->OnMouseMove(dx, dy, 0);
//            }
//        }
//        break;
//
//    case WM_MOVE:
//        {
//            RECT r;
//            GetClientRect(hWnd, &r);
//            WindowCenter.x = r.right/2;
//            WindowCenter.y = r.bottom/2;
//            ::ClientToScreen(hWnd, &WindowCenter);
//        }
//        break;
//
//    case WM_KEYDOWN:
//        OnKey((unsigned)wp, true);
//        break;
//    case WM_KEYUP:
//        OnKey((unsigned)wp, false);
//        break;
//
//    case WM_SETFOCUS:
//        giveUsFocus(true);
//        break;
//
//    case WM_KILLFOCUS:
//        giveUsFocus(false);
//        break;
//
//    case WM_CREATE:
//        // Hack to position mouse in fullscreen window shortly after startup.
//        SetTimer(hWnd, 0, 100, NULL);
//        break;
//
//    case WM_TIMER:
//        KillTimer(hWnd, 0);
//        giveUsFocus(true);
//        break;
//
//    case WM_QUIT:
//    case WM_CLOSE:
//        Quit = true;
//        return 0;
//    }
//
//    return DefWindowProc(hWnd, msg, wp, lp);
//}

static inline float GamepadStick(short in)
{
    float v;
    if (abs(in) < 9000)
        return 0;
    else if (in > 9000)
        v = (float) in - 9000;
    else
        v = (float) in + 9000;
    return v / (32767 - 9000);
}

//static inline float GamepadTrigger(BYTE in)
//{
//    return (in < 30) ? 0.0f : (float(in-30) / 225);
//}


int OculusRoomTinyApp::Run()
{
    pRender->SetWindowSize(1280, 800);
    pRender->SetViewport(0, 0, 1280, 800);
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

    OVR_DEBUG_STATEMENT(_CrtDumpMemoryLeaks());
    return exitCode;
}
