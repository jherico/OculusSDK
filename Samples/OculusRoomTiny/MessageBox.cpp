#include "MessageBox.h"
#ifdef WIN32
#include <Windows.h>
#endif

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif


MessageBoxResult MessageBox(const char * text) {
#ifdef WIN32
        int result = ::MessageBoxA(0, text, "Oculus Rift Detection",
                        MB_CANCELTRYCONTINUE|MB_ICONWARNING);
        switch (result) {
        case IDCANCEL:
            return Cancel;
        case IDCONTINUE:
            return Continue;
        case IDRETRY:
            return Retry;
        }
#endif
    
#ifdef __APPLE__
    CFStringRef headerStrRef  = CFStringCreateWithCString(NULL, "Oculus Rift Detection", kCFStringEncodingMacRoman);
    CFStringRef messageStrRef = CFStringCreateWithCString(NULL, text, kCFStringEncodingMacRoman);
    CFOptionFlags result;
    
//    kCFUserNotificationDefaultResponse = 0,
//    kCFUserNotificationAlternateResponse = 1,
//    kCFUserNotificationOtherResponse = 2,
//    kCFUserNotificationCancelResponse = 3
    
    //launch the message box
    CFUserNotificationDisplayAlert(0,
                                   kCFUserNotificationNoteAlertLevel,
                                   NULL, NULL, NULL,
                                   headerStrRef, // header text
                                   messageStrRef, // message text
                                   CFSTR("Try again"),
                                   CFSTR("Continue"),
                                   CFSTR("Cancel"),
                                   &result);

    //Clean up the strings
    CFRelease(headerStrRef);
    CFRelease(messageStrRef);

    switch (result) {
        case kCFUserNotificationCancelResponse:
        case kCFUserNotificationOtherResponse:
            return Cancel;
        case kCFUserNotificationDefaultResponse:
            return Retry;
        case kCFUserNotificationAlternateResponse:
            return Continue;
    }
#endif
    return Continue;
}
