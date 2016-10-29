/************************************************************************************

Filename    :   Win32_Platform.h
Content     :   Win32 implementation of Platform app infrastructure
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

#ifndef OVR_Win32_Platform_h
#define OVR_Win32_Platform_h

#include "Platform.h"
#include "Kernel/OVR_Win32_IncludeWindows.h"

#include <vector>
#include <string>

namespace OVR { namespace Render {
    class RenderDevice;
    struct DisplayId;
}}

namespace OVR { namespace OvrPlatform { namespace Win32 {

class PlatformCore; 

// -----------------------------------------------------------------------------
// ***** NotificationOverlay

// Describes a notification overlay window that contains a message string.
// When used with Oculus Display Driver, allows the message to be shown
// in the monitor window that is not visible on the Rift.
class NotificationOverlay : public RefCountBase<NotificationOverlay>
{
public:
    NotificationOverlay(PlatformCore* core,
                        int fontHeightPixels, int yoffset, const char* text);
    ~NotificationOverlay();

    void UpdateOnWindowSize();

private:
    PlatformCore* pCore;
    HWND          hWnd;
    HFONT         hFont;
    SIZE          TextSize;
    int           YOffest; // Negative if counting from the bottom
}; 


// -----------------------------------------------------------------------------

class PlatformCore : public OvrPlatform::PlatformCore
{
    HWND             hWnd;
    HINSTANCE        hInstance;
    bool             Quit;
    int              ExitCode;
    int              Width, Height;

    MouseMode        MMode;
    UINT_PTR         MouseWheelTimer;
    POINT            WindowCenter; // In desktop coordinates
    HCURSOR          Cursor;
    int              Modifiers;
    std::string      WindowTitle;

    friend class NotificationOverlay;

    // Win32 static function that delegates to WindowProc member function.
    static LRESULT CALLBACK systemWindowProc(HWND window, UINT msg, WPARAM wp, LPARAM lp);

    LRESULT     WindowProc(UINT msg, WPARAM wp, LPARAM lp);

    std::vector<OVR::Ptr<NotificationOverlay> > NotificationOverlays;

public:
    PlatformCore(Application* app, HINSTANCE hinst);
    ~PlatformCore();

    void*     SetupWindow(int w, int h) override;
    void      DestroyWindow() override;
    void      ShowWindow(bool visible) override;
    void      Exit(int exitcode) override
	{
        // On some AMD cards, additional events may cause crashing after exit.
		//for (MSG msg; PeekMessage(&msg, NULL, 0, 0, PM_REMOVE); )
		//	;
		Quit = 1; ExitCode = exitcode;
	}

    RenderDevice* SetupGraphics(ovrSession session, const SetupGraphicsDeviceSet& setupGraphicsDesc,
                                const char* type,
                                const Render::RendererParams& rp,
                                ovrGraphicsLuid luid) override;

    void                SetMouseMode(MouseMode mm) override;
    void                GetWindowSize(int* w, int* h) const override;
    void                SetWindowSize(int w, int h) override;

    void                SetWindowTitle(const char*title) override;
    void                PlayMusicFile(const char *fileName) override;
    std::string         GetContentDirectory() const override;

    int                 GetDisplayCount();
    Render::DisplayId   GetDisplay(int screen);

    // Creates notification overlay text box over the top of OS window.
    virtual void        SetNotificationOverlay(int index, int fontHeightPixels,
                                               int yoffset, const char* text) override;

    int       Run();
};


// Win32 key conversion helper.
KeyCode MapVKToKeyCode(unsigned vk);

}}}


#endif // OVR_Win32_Platform_h
