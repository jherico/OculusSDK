/************************************************************************************

PublicHeader:   OVR
Filename    :   OVR_Log.h
Content     :   Logging support
Created     :   September 19, 2012
Notes       : 

Copyright   :   Copyright 2014 Oculus VR, LLC All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.2 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.2 

Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#ifndef OVR_Log_h
#define OVR_Log_h

#include "OVR_Types.h"
#include "OVR_Delegates.h"
#include "OVR_Callbacks.h"
#include <stdarg.h>

namespace OVR {

//-----------------------------------------------------------------------------------
// ***** Logging Constants

// LogMaskConstants defined bit mask constants that describe what log messages
// should be displayed.
enum LogMaskConstants
{
    LogMask_Regular = 0x100,
    LogMask_Debug   = 0x200,
    LogMask_None    = 0,
    LogMask_All     = LogMask_Regular|LogMask_Debug
};

// LogLevel should match the CAPI ovrLogLevel enum, values are passed back to ovrLogCallback
enum LogLevel
{
    LogLevel_Debug = 0,
    LogLevel_Info  = 1,
    LogLevel_Error = 2
};

// LogMessageType describes the type of the log message, controls when it is
// displayed and what prefix/suffix is given to it. Messages are subdivided into
// regular and debug logging types. Debug logging is only generated in debug builds.
//
// Log_Text         - General output text displayed without prefix or new-line.
//                    Used in OVR libraries for general log flow messages
//                    such as "Device Initialized".
//
// Log_Error        - Error message output with "Error: %s\n", intended for
//                    application/sample-level use only, in cases where an expected
//                    operation failed. OVR libraries should not use this internally,
//                    reporting status codes instead.
//
// Log_DebugText    - Message without prefix or new lines; output in Debug build only.
//
// Log_Debug        - Debug-build only message, formatted with "Debug: %s\n".
//                    Intended to comment on incorrect API usage that doesn't lead
//                    to crashes but can be avoided with proper use.
//                    There is no Debug Error on purpose, since real errors should
//                    be handled by API user.
//
// Log_Assert      -  Debug-build only message, formatted with "Assert: %s\n".
//                    Intended for severe unrecoverable conditions in library
//                    source code. Generated though OVR_ASSERT_MSG(c, "Text").

enum LogMessageType
{    
    // General Logging
    Log_Text        = LogMask_Regular | 0,    
    Log_Error       = LogMask_Regular | 1, // "Error: %s\n".

    // Debug-only messages (not generated in release build)
    Log_DebugText   = LogMask_Debug | 0,
    Log_Debug       = LogMask_Debug | 1,   // "Debug: %s\n".
    Log_Assert      = LogMask_Debug | 2,   // "Assert: %s\n".
};


// LOG_VAARG_ATTRIBUTE macro, enforces printf-style formatting for message types
#ifdef __GNUC__
#  define OVR_LOG_VAARG_ATTRIBUTE(a,b) __attribute__((format (printf, a, b)))
#else
#  define OVR_LOG_VAARG_ATTRIBUTE(a,b)
#endif

//-----------------------------------------------------------------------------------
// ***** Log

// Log defines a base class interface that can be implemented to catch both
// debug and runtime messages.
// Debug logging can be overridden by calling Log::SetGlobalLog.

class Log
{
	friend class System;

#ifdef OVR_OS_WIN32
    void* hEventSource;
#endif

public: 
    Log(unsigned logMask = LogMask_Debug);
    virtual ~Log();

	typedef Delegate2<void, const char*, LogMessageType> LogHandler;

    // The following is deprecated, as there is no longer a max log buffer message size.
    enum { MaxLogBufferMessageSize = 4096 };

    unsigned        GetLoggingMask() const            { return LoggingMask; }
    void            SetLoggingMask(unsigned logMask)  { LoggingMask = logMask; }

    static void     AddLogObserver(CallbackListener<LogHandler>* listener);

    // This is the same callback signature as in the CAPI.
    typedef void (*CAPICallback)(int level, const char* message);

    // This function should be called before OVR::Initialize()
    static void		SetCAPICallback(CAPICallback callback);

	// Internal
	// Invokes observers, then calls LogMessageVarg()
	static void     LogMessageVargInt(LogMessageType messageType, const char* fmt, va_list argList);

    // This virtual function receives all the messages,
    // developers should override this function in order to do custom logging
    virtual void    LogMessageVarg(LogMessageType messageType, const char* fmt, va_list argList);

    // Call the logging function with specific message type, with no type filtering.
    void            LogMessage(LogMessageType messageType,
                               const char* fmt, ...) OVR_LOG_VAARG_ATTRIBUTE(3,4);


    // Helper used by LogMessageVarg to format the log message, writing the resulting
    // string into buffer. It formats text based on fmt and appends prefix/new line
    // based on LogMessageType. Return behavior is the same as ISO C vsnprintf: returns the 
    // required strlen of buffer (which will be >= bufferSize if bufferSize is insufficient)
    // or returns a negative value because the input was bad.
    static int      FormatLog(char* buffer, size_t bufferSize, LogMessageType messageType,
                              const char* fmt, va_list argList);

    // Default log output implementation used by by LogMessageVarg.
    // Debug flag may be used to re-direct output on some platforms, but doesn't
    // necessarily disable it in release builds; that is the job of the called.    
    void            DefaultLogOutput(const char* textBuffer, LogMessageType messageType, int bufferSize = -1);

    // Determines if the specified message type is for debugging only.
    static bool     IsDebugMessage(LogMessageType messageType)
    {
        return (messageType & LogMask_Debug) != 0;
    }

    // *** Global APIs

    // Global Log registration APIs.
    //  - Global log is used for OVR_DEBUG messages. Set global log to null (0)
    //    to disable all logging.
    static void     SetGlobalLog(Log *log);
    static Log*     GetGlobalLog();

    // Returns default log singleton instance.
    static Log*     GetDefaultLog();

    // Applies logMask to the default log and returns a pointer to it.
    // By default, only Debug logging is enabled, so to avoid SDK generating console
    // messages in user app (those are always disabled in release build,
    // even if the flag is set). This function is useful in System constructor.
    static Log*     ConfigureDefaultLog(unsigned logMask = LogMask_Debug)
    {
        Log* log = GetDefaultLog();
        log->SetLoggingMask(logMask);
        return log;
    }

private:
    // Logging mask described by LogMaskConstants.
    unsigned     LoggingMask;
};


//-----------------------------------------------------------------------------------
// ***** Global Logging Functions and Debug Macros

// These functions will output text to global log with semantics described by
// their LogMessageType.
void LogText(const char* fmt, ...) OVR_LOG_VAARG_ATTRIBUTE(1,2);
void LogError(const char* fmt, ...) OVR_LOG_VAARG_ATTRIBUTE(1,2);

#ifdef OVR_BUILD_DEBUG

    // Debug build only logging.
    void LogDebugText(const char* fmt, ...) OVR_LOG_VAARG_ATTRIBUTE(1,2);
    void LogDebug(const char* fmt, ...) OVR_LOG_VAARG_ATTRIBUTE(1,2);
    void LogAssert(const char* fmt, ...) OVR_LOG_VAARG_ATTRIBUTE(1,2);

    // Macro to do debug logging, printf-style.
    // An extra set of set of parenthesis must be used around arguments,
    // as in: OVR_DEBUG_LOG(("Value %d", 2)).
    #define OVR_DEBUG_LOG(args)       do { OVR::LogDebug args; } while(0)
    #define OVR_DEBUG_LOG_TEXT(args)  do { OVR::LogDebugText args; } while(0)

	// Conditional logging. It logs when the condition 'c' is true.
	#define OVR_DEBUG_LOG_COND(c, args)			do { if ((c)) { OVR::LogDebug args; } } while(0)
	#define OVR_DEBUG_LOG_TEXT_COND(c, args)	do { if ((c)) { OVR::LogDebugText args; } } while(0)

	// Conditional logging & asserting. It asserts/logs when the condition 'c' is NOT true.
    #define OVR_ASSERT_LOG(c, args)	  do { if (!(c)) { OVR::LogAssert args; OVR_DEBUG_BREAK; } } while(0)

#else

    // If not in debug build, macros do nothing.
    #define OVR_DEBUG_LOG(args)				((void)0)
    #define OVR_DEBUG_LOG_TEXT(args)		((void)0)
	#define OVR_DEBUG_LOG_COND(c, args)		((void)0)
	#define OVR_DEBUG_LOG_TEXT_COND(args)	((void)0)
    #define OVR_ASSERT_LOG(c, args)			((void)0)

#endif

} // OVR 

#endif
