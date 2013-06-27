
#import <Cocoa/Cocoa.h>
#import "OSX_Platform.h"
#import "OSX_Gamepad.h"

#import <CoreGraphics/CoreGraphics.h>
#import <CoreGraphics/CGDirectDisplay.h>

@interface OVRApp : NSApplication

@property (assign) IBOutlet NSWindow* win;
@property (assign) OVR::Platform::OSX::PlatformCore* Platform;
@property (assign) OVR::Platform::Application* App;

-(void) run;

@end

@interface OVRView : NSOpenGLView <NSWindowDelegate>

@property (assign) OVR::Platform::OSX::PlatformCore* Platform;
@property (assign) OVR::Platform::Application* App;
@property unsigned long Modifiers;

-(void)ProcessMouse:(NSEvent*)event;
-(void)warpMouseToCenter;

+(CGDirectDisplayID) displayFromScreen:(NSScreen*)s;

@end

