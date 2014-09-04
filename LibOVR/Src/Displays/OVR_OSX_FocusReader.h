#ifndef OVR_OSX_FocusReader_h
#define OVR_OSX_FocusReader_h

#import <Cocoa/Cocoa.h>

@interface FocusReader : NSObject <NSApplicationDelegate>{
    NSWindow *window;
}

- (void)start;

@property (assign) IBOutlet NSWindow *window;

@end

#endif

