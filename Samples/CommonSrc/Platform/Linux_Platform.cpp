/************************************************************************************

Filename    :   Platform_Linux.cpp
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

#include "Kernel/OVR_System.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_Timer.h"
#include "Kernel/OVR_Threads.h"

//#include "Displays/OVR_Linux_SDKWindow.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <signal.h>

#include <GL/CAPI_GLE.h>

#include "OVR_CAPI_GL.h"

#include "Linux_Platform.h"
#include "Linux_Gamepad.h"

#include "../../3rdParty/EDID/edid.h"
#include <X11/extensions/Xrandr.h>


// The following code is temporarily copied from LibOVR, as this source file no longer has access to the 
// internals of LibOVR. The need for this access will go away in a future version of Linux LibOVR.
namespace OVR
{
	enum DistortionRotation
	{
	    DistRotateNone,
	    DistRotateCCW90
	};

	struct LinuxDeviceScreen
	{
	    LinuxDeviceScreen() :
		screen(-1),
		crtcid(0),
		rotation(DistRotateNone),
		productCode(-1),
		width(-1),
		height(-1),
		offsetX(-1),
		offsetY(-1)
	    {}

	    void set(int xScreen, XID xid, DistortionRotation rot, int prodCode,
		     int w, int h, int x, int y)
	    {
		screen      = xScreen;
		crtcid      = xid;
		rotation    = rot;
		productCode = prodCode;

		width       = w;
		height      = h;
		offsetX     = x;
		offsetY     = y;
	    }

	    bool isValid()      {return (screen != -1);}

	    int                 screen;   ///< X Screen this device occupies.
	    XID                 crtcid;   ///< XID uniquely identifying this device on XDisplay.
	    DistortionRotation  rotation;
	    int                 productCode;

	    // Actual width and height of screen.
	    int                 width;
	    int                 height;

	    // Offset if using twinview
	    int                 offsetX;
	    int                 offsetY;
	};

	class SDKWindow
	{
	public:
	    // Choose frame buffer configuration and return fbConfigID. Returns -1 if
	    // a failure occurs.
	    static int chooseFBConfigID(struct _XDisplay* display, int xscreen);

	    // Obtain visual from frame buffer configuration ID. You must call XFree
	    // on the XVisualInfo* pointer.
	    static XVisualInfo* getVisual(struct _XDisplay* display,
		                          int fbConfigID, int xscreen);

	    // GLXFBConfig pointer from frame buffer configuration ID. You must call
	    // XFree on the GLXFBConfig pointer.
	    static GLXFBConfig* getGLXFBConfig(struct _XDisplay* display,
		                               int fbConfigID, int xscreen);

	    static LinuxDeviceScreen findDevScreenForDevID(const char* deviceID);
	};

	// Used in chooseVisual. May need to expose this to the end use so they can
	// choose an appropriate framebuffer configuration.
	struct FBConfig
	{
	  FBConfig() :
	      redBits(8),
	      greenBits(8),
	      blueBits(8),
	      alphaBits(8),
	      depthBits(8),
	      stencilBits(-1),
	      doubleBuffer(true),
	      auxBuffers(-1)
	  {}

	  int  redBits;
	  int  greenBits;
	  int  blueBits;
	  int  alphaBits;
	  int  depthBits;
	  int  stencilBits;
	  bool doubleBuffer;
	  int  auxBuffers;

	  int xcfg;
	};

	static int fbCalcContrib(int desired, int current)
	{
	    int diff = desired - current;
	    if (current != -1) { return diff * diff; }
	    else               { return 0; }
	}

	// Choose frame buffer configuration and return fbConfigID.
	int SDKWindow::chooseFBConfigID(struct _XDisplay* display, int xscreen)
	{
	    int nativeCount = 0;
	    GLXFBConfig* nativeConfigs =
		glXGetFBConfigs(display, xscreen, &nativeCount);
	    if (!nativeCount)
	    {
		OVR::LogError("[SDKWindow] No valid frame buffer configurations found.");
		return 0;
	    }

	    FBConfig* usables = static_cast<FBConfig*>(calloc(nativeCount, sizeof(FBConfig)));
	    int numUsables = 0;

	    for (int i = 0; i < nativeCount; ++i)
	    {
		GLXFBConfig native = nativeConfigs[i];
		FBConfig* usable   = &usables[numUsables];
		int v              = 0;

		// Only frame buffer configcs with attached visuals.
		glXGetFBConfigAttrib(display, native, GLX_VISUAL_ID, &v);
		if (!v) { continue; }

		// Only RGBA frame buffers.
		glXGetFBConfigAttrib(display, native, GLX_RENDER_TYPE, &v);
		if (!(v & GLX_RGBA_BIT)) { continue; }

		glXGetFBConfigAttrib(display, native, GLX_DRAWABLE_TYPE, &v);
		if (!(v & GLX_WINDOW_BIT)) { continue; }

		glXGetFBConfigAttrib(display, native, GLX_DEPTH_SIZE,   &usable->depthBits);
		glXGetFBConfigAttrib(display, native, GLX_STENCIL_SIZE, &usable->stencilBits);

		glXGetFBConfigAttrib(display, native, GLX_RED_SIZE,     &usable->redBits);
		glXGetFBConfigAttrib(display, native, GLX_GREEN_SIZE,   &usable->greenBits);
		glXGetFBConfigAttrib(display, native, GLX_BLUE_SIZE,    &usable->blueBits);
		glXGetFBConfigAttrib(display, native, GLX_ALPHA_SIZE,   &usable->alphaBits);

		glXGetFBConfigAttrib(display, native, GLX_ALPHA_SIZE,   &usable->auxBuffers);

		glXGetFBConfigAttrib(display, native, GLX_DOUBLEBUFFER, &v);
		usable->doubleBuffer = v ? true : false;

		glXGetFBConfigAttrib(display, native, GLX_FBCONFIG_ID, &usable->xcfg);

		++numUsables;
	    }

	    // We really want std::numeric_limits<int>::max() instead of hardcoded vals.
	    const int MostMissing = 100;
	    int leastMissing      = MostMissing;
	    int leastBias         = MostMissing;

	    const FBConfig* closest = NULL;
	    
	    // Desired is currently the default config built by constructor.
	    FBConfig desired;

	    for (int i = 0; i < numUsables; ++i)
	    {
		const FBConfig* cur = &usables[i];

		if (desired.doubleBuffer != cur->doubleBuffer) { continue; }

		int missing = 0;
		if (desired.alphaBits > 0 && cur->alphaBits == 0) { ++missing; }
		if (desired.depthBits > 0 && cur->depthBits == 0) { ++missing; }
		if (desired.stencilBits > 0 && cur->stencilBits == 0) { ++missing; }
		if (desired.redBits > 0 && desired.redBits != cur->redBits) { ++missing; }
		if (desired.greenBits > 0 && desired.greenBits != cur->greenBits) { ++missing; }
		if (desired.blueBits > 0 && desired.blueBits != cur->blueBits) { ++missing; }

		int bias = fbCalcContrib(desired.redBits,     cur->redBits)
		         + fbCalcContrib(desired.greenBits,   cur->greenBits)
		         + fbCalcContrib(desired.blueBits,    cur->blueBits)
		         + fbCalcContrib(desired.alphaBits,   cur->alphaBits)
		         + fbCalcContrib(desired.depthBits,   cur->depthBits)
		         + fbCalcContrib(desired.stencilBits, cur->stencilBits);

		if (missing < leastMissing)
		{
		    closest = cur;
		}
		else if (missing == leastMissing)
		{
		    // Now select against squared differences.
		    if (bias < leastBias)
		    {
		        closest = cur;
		    }
		}

		if (closest == cur)
		{
		    leastMissing = missing;
		    leastBias    = bias;
		}
	    }

	    if (closest == NULL)
	    {
		OVR::LogError("[SDKWindow] Failed to select appropriate frame buffer.");
		XFree(nativeConfigs);
		free(usables);
		return 0;
	    }

	    int retVal = closest->xcfg;

	    XFree(nativeConfigs);
	    free(usables);

	    return retVal;
	}


	// Obtain visual from frame buffer configuration ID.
	XVisualInfo* SDKWindow::getVisual(struct _XDisplay* display,
		                          int fbConfigID, int xscreen)
	{
	    GLXFBConfig* cfg = getGLXFBConfig(display, fbConfigID, xscreen);
	    XVisualInfo* viOut = NULL;
	    if (cfg != NULL)
	    {
		viOut = glXGetVisualFromFBConfig(display, *cfg);
		XFree(cfg);
		cfg = NULL;
	    }
	    else
	    {
		OVR::LogError("Unable to find fb config ID.");
	    }
	    return viOut;
	}


	// GLXFBConfig pointer from frame buffer configuration ID. You must call
	// XFree on the GLXFBConfig pointer.
	GLXFBConfig* SDKWindow::getGLXFBConfig(struct _XDisplay* display,
		                               int fbConfigID, int xscreen)
	{
	    const int attribs[] = {GLX_FBCONFIG_ID, (int)fbConfigID, None};
	    int numElems;

	    GLXFBConfig* config = glXChooseFBConfig(display, xscreen, attribs, &numElems);
	    if (numElems > 0)
	    {
		return config;
	    }
	    else
	    {
		return NULL;
	    }
	}


	static XRRModeInfo* findModeByXID(XRRScreenResources* screen, RRMode xid)
	{
	  for (int m = 0; m < screen->nmode; ++m)
	  {
	    XRRModeInfo* mode = &screen->modes[m];
	    if (xid == mode->id)
	    {
	      return mode;
	    }
	  }
	  return NULL;
	}

	/// Retrieves a list of available device screens on which we can build
	/// SDK windows. Returns number of devices found.
	///   screens        Array which this function will populate.
	///   maxNumScreens  Maximum number of screens to store in screens.
	static int getDeviceScreens(LinuxDeviceScreen* screens, int maxNumDevices)
	{
	    struct _XDisplay* disp = XOpenDisplay(NULL);
	    if (!disp)
	    {
		OVR::LogError("[SDKWindow] Unable to open X Display.");
		return 0;
	    }

	    int numDevices = 0;
	    int numScreens = XScreenCount(disp);
	    for (int i = 0; i < numScreens; ++i)
	    {
		// Screen root is used to detect what video output the crtc is using.
		Window sr                  = XRootWindow(disp, i);
		XRRScreenResources* screen = XRRGetScreenResources(disp, sr);

		for (int ii = 0; ii < screen->ncrtc; ++ii)
		{
		    XRRCrtcInfo* crtcInfo = XRRGetCrtcInfo(disp, screen, screen->crtcs[ii]);

		    if (0 == crtcInfo->noutput)
		    {
		        XRRFreeCrtcInfo(crtcInfo);
		        continue;
		    }

		    bool foundOutput = false;
		    RROutput output = crtcInfo->outputs[0];
		    for (int k = 0; k < crtcInfo->noutput; ++k)
		    {
		        XRROutputInfo* outputInfo =
		            XRRGetOutputInfo(disp, screen, crtcInfo->outputs[k]);
		        for (int kk = 0 ; kk < outputInfo->nmode; ++kk)
		        {
		            if (outputInfo->modes[kk] == crtcInfo->mode)
		            {
		                output = crtcInfo->outputs[k];
		                foundOutput = true;
		                break;
		            }
		        }
		        XRRFreeOutputInfo(outputInfo);
		        if (foundOutput) { break; }
		    }

		    if (!foundOutput)
		    {
		        XRRFreeCrtcInfo(crtcInfo);
		        continue;
		    }

		    XRROutputInfo* outputInfo = XRRGetOutputInfo(disp, screen, output);
		    if (RR_Connected != outputInfo->connection)
		    {
		        XRRFreeOutputInfo(outputInfo);
		        XRRFreeCrtcInfo(crtcInfo);
		        continue;
		    }

		    // Read EDID associated with crtc.
		    MonitorInfo* mi = read_edid_data(disp, output);
		    if (mi == NULL)
		    {
		        XRRFreeOutputInfo(outputInfo);
		        XRRFreeCrtcInfo(crtcInfo);
		        continue;
		    }

		    if (strcmp(mi->manufacturer_code, "OVR") == 0)
		    {
		        XRRModeInfo* modeInfo = findModeByXID(screen, crtcInfo->mode);

		        DistortionRotation desiredRot = DistRotateNone;
		        if (mi->product_code == 3)
		        {
		            // This is a DK2, we may have to rotate our output.
		            // If we don't have to, we should alert the user that
		            // rotating the display using the DM or graphics
		            // card settings is highly non-optimal.
		            desiredRot = DistRotateCCW90;
		            if (crtcInfo->rotation != RR_Rotate_0)
		            {
		                OVR::LogError("Please do not rotate your rift's screen.");

		                if (crtcInfo->rotation == RR_Rotate_90)
		                {
		                    // The user has manually rotated the screen.
		                    // So apply no rotation on our end.
		                    desiredRot = DistRotateNone;
		                }
		            }
		        }
		        else
		        {
		            if (crtcInfo->rotation != RR_Rotate_0)
		            {
		                OVR::LogError("Please do not rotate your rift's screen.");
		            }
		        }

		        int width = modeInfo->width;
		        int height = modeInfo->height;

		        // Swap width / height if display is rotated (shouldn't be on linux).
		        if (   crtcInfo->rotation == RR_Rotate_90
		            || crtcInfo->rotation == RR_Rotate_270)
		        {
		            width  = modeInfo->height;
		            height = modeInfo->width;
		        }

		        // Push detected monitor.
		        screens[numDevices].set(i, screen->crtcs[ii], desiredRot,
		                                mi->product_code, width, height,
		                                crtcInfo->x, crtcInfo->y);
		        ++numDevices;
		    }

		    delete mi;

		    if (numDevices == maxNumDevices)
		    {
		        XRRFreeOutputInfo(outputInfo);
		        XRRFreeCrtcInfo(crtcInfo);
		        XRRFreeScreenResources(screen);
		        OVR::LogError("[SDKWindow] Maxed out number of devices..");
		        XCloseDisplay(disp);
		        return numDevices;
		    }

		    XRRFreeOutputInfo(outputInfo);
		    XRRFreeCrtcInfo(crtcInfo);
		}

		XRRFreeScreenResources(screen);
	    }
	    XCloseDisplay(disp);
	    return numDevices;
	}


	LinuxDeviceScreen SDKWindow::findDevScreenForDevID(const char* deviceIDIn)
	{
	    const int maxNumDevices = 5;
	    LinuxDeviceScreen screens[maxNumDevices];
	    int numDevices = getDeviceScreens(screens, maxNumDevices);

	    if (numDevices > 0)
	    {
		// Identify target for SDK window via hmd info.
		for (int i = 0; i < numDevices; ++i)
		{
		    LinuxDeviceScreen& screen = screens[i];

		    char deviceID[32];
		    OVR_sprintf(deviceID, 32, "OVR%04d-%d",
		                screen.productCode, screen.crtcid);

		    if (strcmp(deviceIDIn, deviceID) == 0)
		    {
		        return screen;
		    }
		}
	    }

	    return LinuxDeviceScreen();
	}


} // namespace OVR (temporary code)






namespace OVR { namespace OvrPlatform { namespace Linux {


OVR::GLEContext gleContext;

OVR::GLEContext* GetGLEContext()
{
    return &gleContext;
}

struct XDisplayInfo
{
    XDisplayInfo() :
        valid(false),
        output(0),
        crtc(0)
    {}

    bool        valid;
    RROutput    output;
    RRCrtc      crtc;
    int         product;
};

static const char *AtomNames[] = {"WM_PROTOCOLS", "WM_DELETE_WINDOW"};

PlatformCore::PlatformCore(Application* app)
    : OvrPlatform::PlatformCore(app),
	  StartVP(0, 0, 0, 0),
      HasWM(false), Disp(NULL), Vis(NULL), Win(0), FBConfigID(-1), GLXWin(0),
      Quit(0), ExitCode(0), Width(0), Height(0), MMode(Mouse_Normal),
      InvisibleCursor(), Atoms()
{
    pGamepadManager = *new Linux::GamepadManager();
}

PlatformCore::~PlatformCore()
{
    if (Disp)
    {
        XFreeCursor(Disp, InvisibleCursor);
        XCloseDisplay(Disp);
        Disp = NULL;
    }

    if (Vis)
    {
        XFree(Vis);
        Vis = NULL;
    }
}

// Setup an X11 window in windowed mode.
void* PlatformCore::SetupWindow(int w, int h)
{
    if (!Disp)
    {
        XInitThreads();

        Disp = XOpenDisplay(NULL);
        if (!Disp)
        {
            OVR_DEBUG_LOG(("XOpenDisplay failed."));
            return NULL;
        }

        XInternAtoms(Disp, const_cast<char**>(AtomNames), NumAtoms, false, Atoms);
    }

    int screenNumber = DefaultScreen(Disp);

    // Determine if we are running under a WM. Window managers will set the
    // substructure redirect mask on the root window.
    XWindowAttributes rootWA;
    XGetWindowAttributes(Disp, RootWindow(Disp, screenNumber), &rootWA);
    HasWM = (rootWA.all_event_masks & SubstructureRedirectMask);

    XSetWindowAttributes winattr;
    unsigned attrmask = CWEventMask | CWBorderPixel;

    winattr.event_mask =
        ButtonPressMask|ButtonReleaseMask|KeyPressMask|KeyReleaseMask|
        ButtonMotionMask|PointerMotionMask|StructureNotifyMask|
        SubstructureNotifyMask;
    winattr.border_pixel = 0;

    OVR::GLEContext* pGLEContext = GetGLEContext();
    pGLEContext->PlatformInit();

    // Choose frame buffer configuration and obtain visual associated with
    // frame buffer ID. We will use this again when building the device
    // context.
    if (FBConfigID == -1)
    {
        FBConfigID = SDKWindow::chooseFBConfigID(Disp, screenNumber);
    }

    if (!Vis)
    {
        Vis = SDKWindow::getVisual(Disp, FBConfigID, screenNumber);
        if (!Vis)
        {
            OVR_DEBUG_LOG(("glXChooseVisual failed."));
            return NULL;
        }
    }

    Window rootWindow = XRootWindow(Disp, Vis->screen);

    winattr.colormap = XCreateColormap(Disp, rootWindow, Vis->visual, AllocNone);
    attrmask |= CWColormap;

    // We are forcing the creation of a slightly smaller window. Depending on
    // the user's WM, the maximum screen size is not the maximum window size on
    // a particular screen. This accounts for various decorations. Screen
    // positioning bugs crop up if this is not handled and the HMD view is
    // to the left of the primary monitor (0,0).
    Win = XCreateWindow(Disp, rootWindow, 0, 0, w - w/6, h - h/6, 0, Vis->depth,
                        InputOutput, Vis->visual, attrmask, &winattr);

    if (!Win)
    {
        OVR_DEBUG_LOG(("XCreateWindow failed."));
        return NULL;
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

    return (void*)&Win;
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
    {
        XRaiseWindow(Disp, Win);
    }
    else
    {
        XIconifyWindow(Disp, Win, 0);
    }
}

void PlatformCore::DestroyWindow()
{
    if (Win)
    {
        XDestroyWindow(Disp, Win);
    }
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
    int key = Key_None;

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
        for (size_t i = 0; i< (sizeof(KeyMap) / sizeof(KeyMap[1])); i++)
        {
            if ((int)vk == KeyMap[i][0])
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
    bool foundScreen = false;

    RROutput primaryOutput     = XRRGetOutputPrimary(Disp, DefaultRootWindow(Disp));
    XRRScreenResources* screen = XRRGetScreenResources(Disp, Win);

    int screenIndex = 0;
    for (int i = 0; i < screen->ncrtc; ++i)
    {
        XRRCrtcInfo* crtcInfo = XRRGetCrtcInfo(Disp, screen, screen->crtcs[i]);

        if (0 == crtcInfo->noutput)
        {
            XRRFreeCrtcInfo(crtcInfo);
            // We intentionally do not increment screenIndex. We do not
            // consider this a valid display.
            continue;
        }

        RROutput output = crtcInfo->outputs[0];
        for (int ii = 0; ii < crtcInfo->noutput; ++ii)
        {
            if (primaryOutput == crtcInfo->outputs[ii])
            {
                output = primaryOutput;
                break;
            }
        }

        XRROutputInfo* outputInfo = XRRGetOutputInfo(Disp, screen, output);
        if (RR_Connected != outputInfo->connection)
        {
            XRRFreeOutputInfo(outputInfo);
            XRRFreeCrtcInfo(crtcInfo);
            continue;
        }

        if (screenId == screenIndex)
        {
            *screenOffsetX = crtcInfo->x;
            *screenOffsetY = crtcInfo->y;
            foundScreen = true;
            XRRFreeOutputInfo(outputInfo);
            XRRFreeCrtcInfo(crtcInfo);
            break;
        }

        XRRFreeOutputInfo(outputInfo);
        XRRFreeCrtcInfo(crtcInfo);

        ++screenIndex;
    }
    XRRFreeScreenResources(screen);

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
    int numScreens = GetDisplayCount();
    for (int i = 0; i < numScreens; i++)
    {
        if (GetDisplay(i).MonitorName == id.MonitorName)
        {
            return i;
        }
    }
    return -1;
}

bool PlatformCore::SetFullscreen(const Render::RendererParams& rp, int fullscreen)
{
    if (fullscreen == pRender->GetParams().Fullscreen)
        return false;

    // Consume any X Configure Notify event. We will wait for a configure
    // notify event after modifying our window.
    XEvent report;
    long eventMask = StructureNotifyMask | SubstructureNotifyMask;
    while (XCheckWindowEvent(Disp, Win, eventMask, &report));

    int displayIndex = IndexOf(rp.Display);
    if (pRender->GetParams().Fullscreen == Render::Display_Window)
    {
        // Save the original window size and position so we can restore later.

        XWindowAttributes xwa;
        XGetWindowAttributes(Disp, Win, &xwa);
        int x, y;
        Window unused;
        XTranslateCoordinates(Disp, Win, DefaultRootWindow(Disp), xwa.x, xwa.y, &x, &y, &unused);

        StartVP.w = xwa.width;//Width;
        StartVP.h = xwa.height;//Height;
        StartVP.x = x;
        StartVP.y = y;
    }
    else if (pRender->GetParams().Fullscreen == Render::Display_Fullscreen)
    {
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
        // Obtain display information so that we can make and informed
        // decision about display modes.
        XDisplayInfo displayInfo = getXDisplayInfo(rp.Display);

      //XRRScreenResources* screen = XRRGetScreenResources(Disp, DefaultRootWindow(Disp));
      //XRRCrtcInfo* crtcInfo      = XRRGetCrtcInfo(Disp, screen, displayInfo.crtc);
      //XRROutputInfo* outputInfo  = XRRGetOutputInfo(Disp, screen, displayInfo.output);

        int xOffset;
        int yOffset;

        if (!determineScreenOffset(displayIndex, &xOffset, &yOffset))
            return false;

        // We should probably always be fullscreen if we don't have a WM.
        if (!HasWM)
        {
            // Determine if we are entering fullscreen on a rift device.
            char deviceID[32];
            OVR_sprintf(deviceID, 32, "OVR%04d-%d",
                        displayInfo.product, displayInfo.crtc);

            LinuxDeviceScreen devScreen = SDKWindow::findDevScreenForDevID(deviceID);
            if (devScreen.isValid())
            {
                XMoveResizeWindow(Disp, Win,
                                  devScreen.offsetX, devScreen.offsetY,
                                  devScreen.width, devScreen.height);
            }
        }

        showWindowDecorations(false);

        XWindowChanges wc;
        wc.x = xOffset;
        wc.y = yOffset;
        wc.stack_mode = 0;

        XConfigureWindow(Disp, Win, CWX | CWY | CWStackMode, &wc);

        // Make the window fullscreen in the window manager.
        // If we are using override redirect, or are on a separate screen
        // with no WM, the following code will have no effect.
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
    XFlush(Disp);

    OvrPlatform::PlatformCore::SetFullscreen(rp, fullscreen);

    // Wait until we receive a configure notify event. If the WM redirected our
    // structure, then WM should synthesize a configure notify event even
    // if there was no change in the window layout.
    XWindowEvent(Disp, Win, eventMask, &report);

    // Process the resize event.
    processEvent(report);

    return true;
}

int PlatformCore::GetDisplayCount()
{
    XRRScreenResources* screen = XRRGetScreenResources(Disp, Win);
    RROutput primaryOutput     = XRRGetOutputPrimary(Disp, DefaultRootWindow(Disp));

    // Iterate through displays and ensure that they have valid outputs.
    int numDisplays = 0;
    for (int i = 0; i < screen->ncrtc; ++i)
    {
        XRRCrtcInfo* crtcInfo = XRRGetCrtcInfo(Disp, screen, screen->crtcs[i]);
        if (0 == crtcInfo->noutput)
        {
            XRRFreeCrtcInfo(crtcInfo);
            continue;
        }

        RROutput output = crtcInfo->outputs[0];
        for (int ii = 0; ii < crtcInfo->noutput; ++ii)
        {
            if (primaryOutput == crtcInfo->outputs[ii])
            {
                output = primaryOutput;
                break;
            }
        }

        XRROutputInfo* outputInfo = XRRGetOutputInfo(Disp, screen, output);
        if (RR_Connected != outputInfo->connection)
        {
            XRRFreeOutputInfo(outputInfo);
            XRRFreeCrtcInfo(crtcInfo);
            continue;
        }

        if (RR_Connected == outputInfo->connection)
        {
            ++numDisplays;
        }

        XRRFreeOutputInfo(outputInfo);
        XRRFreeCrtcInfo(crtcInfo);
    }

    XRRFreeScreenResources(screen);
    return numDisplays;
}

Render::DisplayId PlatformCore::GetDisplay(int screenId)
{
    char device_id[32] = "";

    RROutput primaryOutput = XRRGetOutputPrimary(Disp, DefaultRootWindow(Disp));
    XRRScreenResources* screen = XRRGetScreenResources(Disp, Win);

    int screenIndex = 0;
    for (int i = 0; i < screen->ncrtc; ++i)
    {
        XRRCrtcInfo* crtcInfo = XRRGetCrtcInfo(Disp, screen, screen->crtcs[i]);

        if (0 == crtcInfo->noutput)
        {
            XRRFreeCrtcInfo(crtcInfo);
            // We intentionally do not increment screenIndex. We do not
            // consider this a valid display.
            continue;
        }

        RROutput output = crtcInfo->outputs[0];
        for (int ii = 0; ii < crtcInfo->noutput; ++ii)
        {
            if (primaryOutput == crtcInfo->outputs[ii])
            {
                output = primaryOutput;
                break;
            }
        }

        XRROutputInfo* outputInfo = XRRGetOutputInfo(Disp, screen, output);
        if (RR_Connected != outputInfo->connection)
        {
            XRRFreeOutputInfo(outputInfo);
            XRRFreeCrtcInfo(crtcInfo);
            continue;
        }

        MonitorInfo * mi = read_edid_data(Disp, output);
        if (mi == NULL)
        {
            ++screenIndex;
            XRRFreeOutputInfo(outputInfo);
            XRRFreeCrtcInfo(crtcInfo);
            continue;
        }

        if (screenIndex == screenId)
        {
            OVR_sprintf(device_id, 32, "%s%04d-%d",
                        mi->manufacturer_code, mi->product_code,
                        screen->crtcs[i]);

            XRRFreeOutputInfo(outputInfo);
            XRRFreeCrtcInfo(crtcInfo);

            delete mi;
            break;
        }

        XRRFreeOutputInfo(outputInfo);
        XRRFreeCrtcInfo(crtcInfo);

        delete mi;
        ++screenIndex;
    }
    XRRFreeScreenResources(screen);

    return Render::DisplayId(device_id);
}

XDisplayInfo PlatformCore::getXDisplayInfo(Render::DisplayId id)
{
    int screenId = IndexOf(id);

    // Locate and return XDisplayInfo
    RROutput primaryOutput = XRRGetOutputPrimary(Disp, DefaultRootWindow(Disp));
    XRRScreenResources* screen = XRRGetScreenResources(Disp, Win);

    int screenIndex = 0;
    for (int i = 0; i < screen->ncrtc; ++i)
    {
        XRRCrtcInfo* crtcInfo = XRRGetCrtcInfo(Disp, screen, screen->crtcs[i]);

        if (0 == crtcInfo->noutput)
        {
            XRRFreeCrtcInfo(crtcInfo);
            // We intentionally do not increment screenIndex. We do not
            // consider this a valid display.
            continue;
        }

        RROutput output = crtcInfo->outputs[0];
        for (int ii = 0; ii < crtcInfo->noutput; ++ii)
        {
            if (primaryOutput == crtcInfo->outputs[ii])
            {
                output = primaryOutput;
                break;
            }
        }

        XRROutputInfo* outputInfo = XRRGetOutputInfo(Disp, screen, output);
        if (RR_Connected != outputInfo->connection)
        {
            XRRFreeOutputInfo(outputInfo);
            XRRFreeCrtcInfo(crtcInfo);
            continue;
        }

        MonitorInfo * mi = read_edid_data(Disp, output);
        if (mi == NULL)
        {
            ++screenIndex;
            XRRFreeOutputInfo(outputInfo);
            XRRFreeCrtcInfo(crtcInfo);
            continue;
        }

        if (screenIndex == screenId)
        {
            XDisplayInfo dinfo;
            dinfo.valid  = true;
            dinfo.output = output;
            dinfo.crtc   = outputInfo->crtc;
            dinfo.product= mi->product_code;

            XRRFreeOutputInfo(outputInfo);
            XRRFreeCrtcInfo(crtcInfo);
            XRRFreeScreenResources(screen);

            delete mi;
            return dinfo;
        }

        XRRFreeOutputInfo(outputInfo);
        XRRFreeCrtcInfo(crtcInfo);

        delete mi;
        ++screenIndex;
    }
    XRRFreeScreenResources(screen);

    return XDisplayInfo();
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


// To do: We can do away with this function by using our GLEContext class, which already tracks extensions.
static bool IsGLXExtensionSupported(const char* extension, struct _XDisplay* pDisplay, int /*screen*/ = 0)
{
    // const char* extensionList = glXQueryExtensionsString(pDisplay, screen);
    const char* extensionList = glXGetClientString(pDisplay, GLX_EXTENSIONS);  // This is returning more extensions than glXQueryExtensionsString does.

    if(extensionList)
    {
        const char* start = extensionList;
        const char* where;

        static bool printed = false;
        if(!printed)
        {
            OVR_DEBUG_LOG(("glX extensions: %s", extensionList));
            printed = true;
        }

        while((where = (char*)strstr(start, extension)) != NULL)
        {
            if (where)
            {
                const char* term = (where + strlen(extension));

                if ((where == start) || (where[-1] == ' '))
                {
                    if ((term[0] == ' ') || (term[0] == '\0'))
                        return true;
                }

                start = term;
            }
        }
    }

    return false;
}

ovrRenderAPIConfig RenderDevice::Get_ovrRenderAPIConfig() const
{
    static ovrGLConfig cfg;
    cfg.OGL.Header.API            = ovrRenderAPI_OpenGL;
    cfg.OGL.Header.BackBufferSize = Sizei(WindowWidth, WindowHeight);
    cfg.OGL.Header.Multisample    = Params.Multisample;
    cfg.OGL.Disp                  = NULL;
    return cfg.Config;
}

Render::RenderDevice* RenderDevice::CreateDevice(const RendererParams& rp, void* oswnd)
{
    OvrPlatform::Linux::PlatformCore* PC = (OvrPlatform::Linux::PlatformCore*)oswnd;

    GLXContext context = 0;

  //int targetScreen = DefaultScreen(PC->Disp);

    // Print some version information.
    const char * pGLXVendor = glXGetClientString(PC->Disp, GLX_VENDOR);
    OVR_DEBUG_LOG(("GLX vendor: %s", pGLXVendor)); OVR_UNUSED(pGLXVendor);

    const char * pGLXVersion = glXGetClientString(PC->Disp, GLX_VERSION);
    OVR_DEBUG_LOG(("GLX version: %s", pGLXVersion)); OVR_UNUSED(pGLXVersion);

    // http://www.opengl.org/registry/specs/ARB/glx_create_context.txt
    // Usage of glXCreateContextAttribsARB requires the GLX_ARB_create_context glX extension.
    if(IsGLXExtensionSupported("GLX_ARB_create_context", PC->Disp))
    {
        typedef GLXContext (*glXCreateContextAttribsARBProc)(struct _XDisplay*, GLXFBConfig, GLXContext, Bool, const int*);
        glXCreateContextAttribsARBProc glXCreateContextAttribsARBImpl = (glXCreateContextAttribsARBProc) glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");

        if(glXCreateContextAttribsARBImpl) // This should always succeed.
        {
            const int  MajorVersionRequested         = rp.GLMajorVersion;
            const int  MinorVersionRequested         = rp.GLMinorVersion;
            const bool DebugContextRequested         = rp.DebugEnabled;
            const bool ForwardCompatibleRequested    = rp.GLForwardCompatibleProfile;
            const bool CoreProfileRequested          = rp.GLCoreProfile && IsGLXExtensionSupported("GLX_ARB_create_context_profile", PC->Disp);
            const bool CompatibilityProfileRequested = rp.GLCompatibilityProfile && IsGLXExtensionSupported("GLX_ARB_create_context_profile", PC->Disp);
            const bool ContextRobustnessRequested    = rp.DebugEnabled && IsGLXExtensionSupported("GLX_ARB_create_context_robustness", PC->Disp);

            // Setup the attributes.
            int attribListSize = 4;
            int attribList[20] = {
                GLX_CONTEXT_MAJOR_VERSION_ARB, MajorVersionRequested,
                GLX_CONTEXT_MINOR_VERSION_ARB, MinorVersionRequested
            };

            // Usage of GLX_CONTEXT_DEBUG_BIT_ARB is always available with the GLX_ARB_create_context glX extension (though in practice it may have no effect).
            // Usage of GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB is always available with the GLX_ARB_create_context glX extension.
            if(DebugContextRequested || ForwardCompatibleRequested || ContextRobustnessRequested)
            {
                attribList[attribListSize++] = GLX_CONTEXT_FLAGS_ARB;
                int flags = (DebugContextRequested ? GLX_CONTEXT_DEBUG_BIT_ARB : 0) |
                            (ForwardCompatibleRequested ? GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB : 0) |
                            (ContextRobustnessRequested ? GLX_CONTEXT_ROBUST_ACCESS_BIT_ARB : 0);
                attribList[attribListSize++] = flags;
            }

            if(CoreProfileRequested || CompatibilityProfileRequested)
            {
                attribList[attribListSize++] = GLX_CONTEXT_PROFILE_MASK_ARB;
                attribList[attribListSize++] = CoreProfileRequested ? GLX_CONTEXT_CORE_PROFILE_BIT_ARB : GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
            }

            if(ContextRobustnessRequested)
            {
                attribList[attribListSize++] = GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB;
                attribList[attribListSize++] = GLX_LOSE_CONTEXT_ON_RESET_ARB; // or GLX_NO_RESET_NOTIFICATION_ARB
            }

            attribList[attribListSize++] = None;

            // Obtain GLXFBConfig used when building visual for Window.
            GLXFBConfig* cfg = SDKWindow::getGLXFBConfig(
                PC->Disp, PC->FBConfigID, PC->Vis->screen);

            if (cfg)
            {
                context = glXCreateContextAttribsARBImpl(PC->Disp, *cfg, 0,
                                                         GL_TRUE, attribList);
                if (context)
                {
                    // We call this to generate a valid GLXDrawable.
                    PC->GLXWin = glXCreateWindow(PC->Disp, *cfg, PC->Win, NULL);
                }

                XFree(cfg);
                cfg = NULL;
            }
        }
    }

    if(!context) // If glXCreateContextAttribsARB couldn't be used...
    {
        context = glXCreateContext(PC->Disp, PC->Vis, 0, GL_TRUE);
        // XXX: Should we use FBConfigID to generate GLXFBConfig and call
        //      glXCreateWindow like we do above (even though we do not
        //      create the context with the GLXFBConfig)?
        PC->GLXWin = PC->Win;
    }

    if (!context)
    {
        return NULL;
    }

    if (!glXMakeCurrent(PC->Disp, PC->GLXWin, context))
    {
        OVR_DEBUG_LOG(("RenderDevice::CreateDevice: glXMakeCurrent failure."));
        glXDestroyContext(PC->Disp, context);
        return NULL;
    }

    XMapRaised(PC->Disp, PC->Win); // Marks the window and any sub-windows for display, while raising the Window to the top of the hierarchy.

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

OVR::OvrPlatform::Linux::PlatformCore* gPlatform;

static void handleSigInt(int /*sig*/)
{
    signal(SIGINT, SIG_IGN);
    gPlatform->Exit(0);
}

int main(int argc, const char* argv[])
{
    using namespace OVR;
    using namespace OVR::OvrPlatform;

    // SigInt for capturing Ctrl-c.
    if (signal(SIGINT, handleSigInt) == SIG_ERR)
    {
        fprintf(stderr, "Failed setting SIGINT handler\n");
        return EXIT_FAILURE;
    }

    // CreateApplication must be the first call since it does OVR::System::Initialize.
    Application* app = Application::CreateApplication();
    gPlatform        = new Linux::PlatformCore(app);
    // The platform attached to an app will be deleted by DestroyApplication.
    app->SetPlatformCore(gPlatform);

    int exitCode = app->OnStartup(argc, argv);
    if (!exitCode)
        exitCode = gPlatform->Run();

    // No OVR functions involving memory are allowed after this.
    Application::DestroyApplication(app);
    app = 0;

    return exitCode;
}
