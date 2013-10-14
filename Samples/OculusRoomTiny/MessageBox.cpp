#include "MessageBox.h"
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif


MessageBoxResult MessageBox(const char * text) {
#ifdef OVR_OS_WIN32
    
    // FIXME extract the win32-ness
    //        if (detectionMessage)
    //        {
    //            int         detectionResult = IDCONTINUE;
    //            String messageText(detectionMessage);
    //            messageText += "\n\n"
    //                           "Press 'Try Again' to run retry detection.\n"
    //                           "Press 'Continue' to run full-screen anyway.";
    //
    //            detectionResult = ::MessageBoxA(0, messageText.ToCStr(), "Oculus Rift Detection",
    //                                            MB_CANCELTRYCONTINUE|MB_ICONWARNING);
    //
    //            if (detectionResult == IDCANCEL)
    //                return 1;
    //        }
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
