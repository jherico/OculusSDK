/************************************************************************************
 
 Filename    :   OSX_OculusRoomTiny.mm
 Content     :   Simplest possible first-person view test application for Oculus Rift
 Created     :   May 7, 2013
 Authors     :   Michael Antonov, Andrew Reisse, Artem Bolgar
 
 Copyright   :   Copyright 2013 Oculus, Inc. All Rights reserved.
 
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

#import "OSX_OculusRoomTiny.h"
#include "RenderTiny_GL_Device.h"

#include "Kernel/OVR_KeyCodes.h"

using namespace OVR;

//-------------------------------------------------------------------------------------
// ***** OculusRoomTiny Class

// Static pApp simplifies routing the window function.
OculusRoomTinyApp* OculusRoomTinyApp::pApp = 0;


OculusRoomTinyApp::OculusRoomTinyApp(OVRApp* nsapp)
    : pRender(0),
    LastUpdate(0),
    NsApp(nsapp),
    Quit(0),

    // Initial location
    EyePos(0.0f, 1.6f, -5.0f),
    EyeYaw(YawInitial), EyePitch(0), EyeRoll(0),
    LastSensorYaw(0),
    SConfig(),
    PostProcess(PostProcess_Distortion),
    ShiftDown(false),
    ControlDown(false)
{
    pApp = this;
    
    Width  = 1280;
    Height = 800;
    
    StartupTicks = OVR::Timer::GetTicks();
    
    MoveForward   = MoveBack = MoveLeft = MoveRight = 0;
}

OculusRoomTinyApp::~OculusRoomTinyApp()
{
	RemoveHandlerFromDevices();
    pSensor.Clear();
    pHMD.Clear();
    destroyWindow();
    pApp = 0;
}

bool  OculusRoomTinyApp::setupWindow()
{
    NSRect winrect;
    winrect.origin.x = 0;
    winrect.origin.y = 1000;
    winrect.size.width = Width;
    winrect.size.height = Height;
    NSWindow* win = [[NSWindow alloc] initWithContentRect:winrect styleMask:NSTitledWindowMask|NSClosableWindowMask backing:NSBackingStoreBuffered defer:NO];
    
    OVRView* view = [[OVRView alloc] initWithFrame:winrect];
    [win setContentView:view];
    [win setAcceptsMouseMovedEvents:YES];
    [win setDelegate:view];
    [view setApp:pApp];
    Win = win;
    View = view;

    const char* title = "OculusRoomTiny";
    [((NSWindow*)Win) setTitle:[[NSString alloc] initWithBytes:title length:strlen(title) encoding:NSUTF8StringEncoding]];
    
    [NSCursor hide];
    [view warpMouseToCenter];
    CGAssociateMouseAndMouseCursorPosition(false);

    SetFullscreen(RenderParams, true);
    return true;
}

void  OculusRoomTinyApp::destroyWindow()
{
    SetFullscreen(RenderParams, false);
    [((NSWindow*)Win) close];
}

void OculusRoomTinyApp::Exit()
{
    [NsApp stop:nil];
    Quit = true;
}

bool OculusRoomTinyApp::SetFullscreen(const RenderTiny::RendererParams& rp, int fullscreen)
{
    if (fullscreen == RenderTiny::Display_Window)
        [(OVRView*)View exitFullScreenModeWithOptions:nil];
    else
    {
        NSScreen* usescreen = [NSScreen mainScreen];
        NSArray* screens = [NSScreen screens];
        for (int i = 0; i < [screens count]; i++)
        {
            NSScreen* s = (NSScreen*)[screens objectAtIndex:i];
            CGDirectDisplayID disp = [OVRView displayFromScreen:s];
            
            if (disp == rp.DisplayId)
                usescreen = s;
        }
        
        [(OVRView*)View enterFullScreenMode:usescreen withOptions:nil];
    }
    
    if (pRender)
        pRender->SetFullscreen((RenderTiny::DisplayMode)fullscreen);
    return 1;
}

//-------------------------------------------------------------------------------------
// ***** OS X-Specific Logic

@implementation OVRApp

- (void)dealloc
{
    [super dealloc];
}

- (void)run
{
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    _running = YES;
    OculusRoomTinyApp* app;
   
    // Initializes LibOVR. This LogMask_All enables maximum logging.
    // Custom allocator can also be specified here.
    OVR::System::Init(OVR::Log::ConfigureDefaultLog(OVR::LogMask_All));
   
    int exitCode = 0;
    do
    {
        {
            using namespace OVR;
            
            // CreateApplication must be the first call since it does OVR::System::Initialize.
            app = new OculusRoomTinyApp(self);
            // The platform attached to an app will be deleted by DestroyApplication.
            
            [self setApp:app];
            
            const char* argv[] = {"OVRApp"};
            exitCode = app->OnStartup(argv[0]);
            if (exitCode)
                break;
        }
        [self finishLaunching];
        [pool drain];

        while ([self isRunning])
        {
            pool = [[NSAutoreleasePool alloc] init];
            NSEvent* event = [self nextEventMatchingMask:NSAnyEventMask untilDate:nil inMode:NSDefaultRunLoopMode dequeue:YES];
            if (event)
            {
                [self sendEvent:event];
            }
            _App->OnIdle();
            [pool drain];
        }
    } while(0);
    
    delete app;

    // No OVR functions involving memory are allowed after this.
    OVR::System::Destroy();
}

@end

static int KeyMap[][2] =
{
    { NSDeleteFunctionKey,      OVR::Key_Delete },
    { '\t',       OVR::Key_Tab },
    { '\n',    OVR::Key_Return },
    { NSPauseFunctionKey,     OVR::Key_Pause },
    { 27,      OVR::Key_Escape },
    { 127,     OVR::Key_Backspace },
    { ' ',     OVR::Key_Space },
    { NSPageUpFunctionKey,     OVR::Key_PageUp },
    { NSPageDownFunctionKey,      OVR::Key_PageDown },
    { NSNextFunctionKey,      OVR::Key_PageDown },
    { NSEndFunctionKey,       OVR::Key_End },
    { NSHomeFunctionKey,      OVR::Key_Home },
    { NSLeftArrowFunctionKey,      OVR::Key_Left },
    { NSUpArrowFunctionKey,        OVR::Key_Up },
    { NSRightArrowFunctionKey,     OVR::Key_Right },
    { NSDownArrowFunctionKey,      OVR::Key_Down },
    { NSInsertFunctionKey,    OVR::Key_Insert },
    { NSDeleteFunctionKey,    OVR::Key_Delete },
    { NSHelpFunctionKey,      OVR::Key_Insert },
};


static KeyCode MapToKeyCode(wchar_t vk)
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
    else if ((vk >= '0') && (vk <= '9'))
    {
        key = vk - '0' + Key_Num0;
    }
    else if ((vk >= NSF1FunctionKey) && (vk <= NSF15FunctionKey))
    {
        key = vk - NSF1FunctionKey + Key_F1;
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

@implementation OVRView

-(BOOL) acceptsFirstResponder
{
    return YES;
}
-(BOOL) acceptsFirstMouse:(NSEvent *)ev
{
    return YES;
}

+(CGDirectDisplayID) displayFromScreen:(NSScreen *)s
{
    NSNumber* didref = (NSNumber*)[[s deviceDescription] objectForKey:@"NSScreenNumber"];
    CGDirectDisplayID disp = (CGDirectDisplayID)[didref longValue];
    return disp;
}

-(void) warpMouseToCenter
{
    NSPoint w;
    w.x = _App->GetWidth()/2.0f;
    w.y = _App->GetHeight()/2.0f;
    w = [[self window] convertBaseToScreen:w];
    CGDirectDisplayID disp = [OVRView displayFromScreen:[[self window] screen]];
    CGPoint p = {w.x, CGDisplayPixelsHigh(disp)-w.y};
    CGDisplayMoveCursorToPoint(disp, p);
}

-(void) keyDown:(NSEvent*)ev
{
    NSString* chars = [ev charactersIgnoringModifiers];
    if ([chars length])
    {
        wchar_t ch = [chars characterAtIndex:0];
        OVR::KeyCode key = MapToKeyCode(ch);
        _App->OnKey(key, true);
    }
}
-(void) keyUp:(NSEvent*)ev
{
    NSString* chars = [ev charactersIgnoringModifiers];
    if ([chars length])
    {
        wchar_t ch = [chars characterAtIndex:0];
        OVR::KeyCode key = MapToKeyCode(ch);
        _App->OnKey(key, false);
        
    }
}


-(void)flagsChanged:(NSEvent *)ev
{
    static const OVR::KeyCode ModifierKeys[] = {OVR::Key_None, OVR::Key_Shift, OVR::Key_Control, OVR::Key_Alt, OVR::Key_Meta};

    unsigned long cmods = [ev modifierFlags];
    if ((cmods & 0xffff0000) != _Modifiers)
    {
        uint32_t mods = 0;
        if (cmods & NSShiftKeyMask)
            mods |= 0x01;
        if (cmods & NSControlKeyMask)
            mods |= 0x02;
        if (cmods & NSAlternateKeyMask)
            mods |= 0x04;
        if (cmods & NSCommandKeyMask)
            mods |= 0x08;

        for (int i = 1; i <= 4; i++)
        {
            unsigned long m = (1 << (16+i));
            if ((cmods & m) != (_Modifiers & m))
            {
                if (cmods & m)
                    _App->OnKey(ModifierKeys[i], true);
                else
                    _App->OnKey(ModifierKeys[i], false);
            }
        }
        _Modifiers = cmods & 0xffff0000;
    }
}

-(void)ProcessMouse:(NSEvent*)ev
{
    switch ([ev type])
    {
        case NSLeftMouseDragged:
        case NSRightMouseDragged:
        case NSOtherMouseDragged:
        case NSMouseMoved:
        {
            int dx = [ev deltaX];
            int dy = [ev deltaY];
            
            if (dx != 0 || dy != 0)
            {
                _App->OnMouseMove(dx, dy, 0);
                [self warpMouseToCenter];
            }
        }
        break;
        case NSLeftMouseDown:
        case NSRightMouseDown:
        case NSOtherMouseDown:
            break;
    }
}

-(void) mouseMoved:(NSEvent*)ev
{
    [self ProcessMouse:ev];
}
-(void) mouseDragged:(NSEvent*)ev
{
    [self ProcessMouse:ev];
}

-(void) mouseDown:(NSEvent*)ev
{
    [self warpMouseToCenter];
    CGAssociateMouseAndMouseCursorPosition(false);
    [NSCursor hide];
}

//-(void)

-(id) initWithFrame:(NSRect)frameRect
{
    NSOpenGLPixelFormatAttribute attr[] =
    {NSOpenGLPFAWindow, NSOpenGLPFADoubleBuffer, NSOpenGLPFADepthSize, 24, nil};
        
    NSOpenGLPixelFormat *pf = [[[NSOpenGLPixelFormat alloc] initWithAttributes:attr] autorelease];
    
    self = [super initWithFrame:frameRect pixelFormat:pf];
    GLint swap = 0;
    [[self openGLContext] setValues:&swap forParameter:NSOpenGLCPSwapInterval];
    //[self setWantsBestResolutionOpenGLSurface:YES];
    return self;
}

-(BOOL)windowShouldClose:(id)sender
{
    _App->Exit();
    return 1;
}

@end

// GL OSX-specific logic
namespace OSX {

    RenderDevice::RenderDevice(const RendererParams& p, void* osview, void* context)
        : GL::RenderDevice(p), Context(context)
    {
        OVRView* view = (OVRView*)osview;
        NSRect bounds = [view bounds];
        WindowWidth = bounds.size.width;
        WindowHeight= bounds.size.height;
    }

    RenderDevice* RenderDevice::CreateDevice(const RendererParams& rp, void* osview)
    {
        OVRView* view = (OVRView*)osview;
        NSOpenGLContext *context = [view openGLContext];
        if (!context)
            return NULL;

        [context makeCurrentContext];
        [[view window] makeKeyAndOrderFront:nil];

        return new OSX::RenderDevice(rp, osview, context);
    }

    void RenderDevice::Present()
    {
        NSOpenGLContext *context = (NSOpenGLContext*)Context;
        [context flushBuffer];
    }

    void RenderDevice::Shutdown()
    {
        Context = NULL;
    }

    bool RenderDevice::SetFullscreen(DisplayMode fullscreen)
    {
        Params.Fullscreen = fullscreen;
        return 1;
    }
    
}


int main(int argc, char *argv[])
{
    NSApplication* nsapp = [OVRApp sharedApplication];
    [nsapp run];
    return 0;
}

