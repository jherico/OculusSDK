/************************************************************************************

Filename    :   OVR_Error.h
Content     :   Structs and functions for handling OVRErrors
Created     :   February 15, 2015
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

#ifndef OVR_Error_h
#define OVR_Error_h

#include "OVR_ErrorCode.h"
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_System.h"
#include "Kernel/OVR_Threads.h"
#include <chrono>
#include <utility>


/// -----------------------------------------------------------------------------
/// ***** OVR Error System
/*
The basic design of the LibOVR user-facing error reporting system is the following:
    - public API functions that can fail return an integer error code (ovr_Result).
    - The ovr_GetLastErrorInfo public function returns additional information about the last function that returned an error.
    - The full error information is written to the log if it's enabled.

Most of the rest of the error system is internal to LibOVR in OVR_Error.h/cpp and is not currently exposed to users.
The goal of the functionality in OVR_Error.h/cpp is to assist in easily generating errors and having them be logged and 
propagated back to the application-facing API without getting lost or ignored. Each subsystem may have its own way of using the 
error system to ensure errors are caught and reflected back to the application.

The basic pattern of usage is:
    - Some application-facing functions return an ovr_Result (e.g. ovr_Initialize).
    - Somewhere down in the execution of those functions, an error can occur which will necessitate
      returning an error code to the application.
    - The code that first detects that an error has occurred calls OVR_MAKE_ERROR, which has parameters
      for specifying the error code, a description string, etc.
    - OVR_MAKE_ERROR creates an OVRError object, which contains the OVR error code, description, and 
      additional info such as timestamp, callstack, FILE/LINE, and possibly an OS-specific low-level errno code.
    - OVR_MAKE_ERROR logs the relevant OVRError information, sets the OVRError as thread's last error, 
      and returns a copy of the OVRError.
    - The code that calls OVR_MAKE_ERROR can use the returned OVRError if it's useful to do so, 
      otherwise it can just use the individual ovr_Result error code. The OVRError won't get lost because
      it was set as the thread's last error. Either way, what's important is that the given error code 
      make its way back to the application without getting lost or ignored.

The primary entities in to assist in generating errors are:
    - ovr_Result, which is the integer error type itself.
         - See OVR_ErrorCode.h for instructions on usage of the codes themselves.
    - OVR_MAKE_ERROR, etc. which is a macro for generating an error (an OVRError object).
         - This is the primary means for generating an error.
    - The OVRError class, which contains the information about an error.
         - A given subsystem doesn't necessarily need to interact with this class unless it's useful.
    - GetLastErrorTLS, which returns the most recently generated OVRError for a thread.
         - Use of this by LibOVR code is optional and not typically needed.
    - GetErrorCodeString, which converts an ovr_Result to a string version (e.g. ovrError_IncompatibleOS -> "ovrError_IncompatibleOS").
    - GetSysErrorCodeString, which converts an HRESULT or errno to a readable string.

Keep in mind that not everything that's an error necessarily needs to be reported back 
to the application, depending on the situation. If the application truly doesn't need to know 
about some OS API error that occurred, that burden shouldn't be put onto the application. 
*/




/// -----------------------------------------------------------------------------
/// ***** OVR_FILE / OVR_LINE
///
#if !defined(OVR_FILE)
    #if defined(OVR_BUILD_DEBUG)
        #define OVR_FILE __FILE__
        #define OVR_LINE __LINE__
    #else
        #define OVR_FILE nullptr
        #define OVR_LINE 0
    #endif
#endif


namespace OVR {


/// -----------------------------------------------------------------------------
/// ***** OVR_MAKE_ERROR, OVR_MAKE_ERROR_F, OVR_MAKE_SYS_ERROR, OVR_MAKE_SYS_ERROR_F
///
/// Declaration:
///      OVRError OVR_MAKE_ERROR(ovrResult code, const char* pDescription);
///      OVRError OVR_MAKE_ERROR_F(ovrResult code, const char* pFormat, ...);
///
///      OVRError OVR_MAKE_SYS_ERROR(ovrResult code, ovrSysErrorCode sysCode, const char* pDescription);
///      OVRError OVR_MAKE_SYS_ERROR_F(ovrResult code, ovrSysErrorCode sysCode, const char* pFormat, ...);
///
/// Example usage:
///      OVRError InitGraphics()
///      {
///          if(!GraphicsCardPresent())
///          {
///              return OVR_MAKE_ERROR(ovrError_GraphicsInit, "Failed to init graphics; graphics support absent.");
///          }
///
///          HRESULT hr = pDevice->CreateTexture2D(&dsDesc, nullptr, &Texture);
///          if(FAILED(hr))
///          {
///              return OVR_MAKE_SYS_ERROR_F(ovrError_GraphicsInit, hr, "Failed to create texture of size %u x %u", dsDesc.Width, dsDesc.Height);
///          }
///          or:
///              OVR_HR_CHECK_RET_ERROR(ovrError_GraphicsInit, hr, "Failed to create texture of size %u x %u", dsDesc.Width, dsDesc.Height);
///
///          return ovrSuccess; // Converts to an OVRError instance that has no error.
///      }
///
#define OVR_MAKE_ERROR(errorCode, pDescription) \
    OVR::MakeError((errorCode), OVR::ovrSysErrorCodeSuccess, OVR_FILE, OVR_LINE, nullptr, "%s", (pDescription))

#define OVR_MAKE_ERROR_F(errorCode, pDescriptionFormat, ...) \
    OVR::MakeError((errorCode), OVR::ovrSysErrorCodeSuccess, OVR_FILE, OVR_LINE, nullptr, (pDescriptionFormat), __VA_ARGS__)


#define OVR_MAKE_SYS_ERROR(errorCode, sysErrorCode, pDescription) \
    OVR::MakeError((errorCode), (sysErrorCode), OVR_FILE, OVR_LINE, nullptr, "%s", (pDescription))

#define OVR_MAKE_SYS_ERROR_F(errorCode, sysErrorCode, pDescriptionFormat, ...) \
    OVR::MakeError((errorCode), (sysErrorCode), OVR_FILE, OVR_LINE, nullptr, (pDescriptionFormat), __VA_ARGS__)

#define OVR_SET_ERROR(ovrError) \
    OVR::SetError(ovrError)

#define OVR_HR_CHECK_RET_ERROR(errorCode, sysErrorCode, pDescription) \
    if (FAILED(sysErrorCode)) { \
        return OVR::MakeError((errorCode), (sysErrorCode), OVR_FILE, OVR_LINE, nullptr, "%s", (pDescription)); \
    }

#define OVR_HR_CHECK_RET_ERROR_F(errorCode, sysErrorCode, pDescriptionFormat, ...) \
    if (FAILED(sysErrorCode)) { \
        return OVR::MakeError((errorCode), (sysErrorCode), OVR_FILE, OVR_LINE, nullptr, (pDescriptionFormat), __VA_ARGS__); \
    }



/// -----------------------------------------------------------------------------
/// ***** ovrSysErrorCode
///
/// Identifies a platform-specific error identifier.
/// For Windows this means an HRESULT or DWORD system error code from GetLastError.
/// For Unix this means errno.
///
typedef uint32_t ovrSysErrorCode;


/// -----------------------------------------------------------------------------
/// ***** ovrSysErrorCodeSuccess
///
/// Identifies a ovrSysErrorCode that's success.
///
const ovrSysErrorCode ovrSysErrorCodeSuccess = 0;
    

/// -----------------------------------------------------------------------------
/// ***** ovrSysErrorCodeNone
///
/// Identifies a ovrSysErrorCode that's un-set.
///
const ovrSysErrorCode ovrSysErrorCodeNone = 0;

    

// SysClockTime is a C++11 equivalent to C time_t.
typedef std::chrono::time_point<std::chrono::system_clock> SysClockTime;



/// -----------------------------------------------------------------------------
/// ***** OVRError
///
/// Represents an error and relevant information about it.
/// While you can create error instances directly via this class, it's better if
/// you create them via the OVR_MAKE_ERROR family of macros, or at least via the
/// MakeError function.
///
/// Relevant design analogues:
///     https://developer.apple.com/library/mac/documentation/Cocoa/Reference/Foundation/Classes/NSError_Class/
///     https://msdn.microsoft.com/en-us/library/windows/desktop/ms723041%28v=vs.85%29.aspx
///
class OVRError
{
private:
    // Cannot convert boolean to OVRError - It must be done explicitly.
    OVRError(bool) { OVR_ASSERT(false); }
    OVRError(bool, const char*, ...) { OVR_ASSERT(false); }

public:
    OVRError();
    OVRError(ovrResult code); // Intentionally not explicit.
    OVRError(ovrResult code, const char* pFormat, ...);

    OVRError(const OVRError& OVRError);
    OVRError(OVRError&& OVRError);

    virtual ~OVRError();

    // Construct a success code.  Use Succeeded() to check for success.
    static OVRError Success() { return OVRError(); }

    OVRError& operator=(const OVRError& OVRError);
    OVRError& operator=(OVRError&& OVRError);

    // Use this to check if result is a success code
    bool Succeeded()
    {
        return Code >= ovrSuccess;
    }

    // Sets the OVRTime, ClockTime, Backtrace to current values.
    void SetCurrentValues(); // To do: Come up with a more appropiate name.

    // Clears all members to a newly default-constructed state.
    void Reset();
        
    // Get the full error string for this error. May include newlines.
    String GetErrorString() const;

    // Property accessors
    void      SetCode(ovrResult code);
    ovrResult GetCode() const;

    void            SetSysCode(ovrSysErrorCode sysCode);
    ovrSysErrorCode GetSysCode() const;

    void   SetDescription(const char* pDescription);
    String GetDescription() const;

    void   SetContext(const char* pContext);
    String GetContext() const;

    void   SetOVRTime(double ovrTime);
    double GetOVRTime() const;

    void         SetSysClockTime(const SysClockTime& clockTime);
    SysClockTime GetSysClockTime() const;

    static const int64_t kLogLineUnset = -1;
    void    SetLogLine(int64_t logLine);
    int64_t GetLogLine() const;

    bool    IsAlreadyLogged() const
    {
        return AlreadyLogged;
    }
    void    SetAlreadyLogged()
    {
        AlreadyLogged = true;
    }
    void    ResetAlreadyLogged()
    {
        AlreadyLogged = false;
    }

    void                   SetSource(const char* pSourceFilePath, int sourceFileLine);
    std::pair<String, int> GetSource() const;

    typedef OVR::Array<void*> AddressArray;
    AddressArray GetBacktrace() const;
        
protected:
    ovrResult         Code;                /// The main ovrResult, which is a high level error id.
    ovrSysErrorCode   SysCode;             /// May be ovrSysErrorCodeSuccess to indicate there isn't a relevant system error code.
    String            Description;         /// Unlocalized error description string.
    String            Context;             /// Context string. For example, for a file open failure this is the file path.
    double            OVRTime;             /// Time when the error was generated. Same format as OVR time.
    SysClockTime      ClockTime;           /// Wall clock time.
    int64_t           LogLine;             /// Log line of the error. -1 if not set (not logged).
    String            SourceFilePath;      /// The __FILE__ where the error was first encountered.
    int               SourceFileLine;      /// The __LINE__ where the error was first encountered.
    AddressArray      Backtrace;           /// Backtrace at point of error. May be empty in publicly released builds.
    bool              AlreadyLogged;       /// Error has already been logged to avoid double-printing it.
};




/// -----------------------------------------------------------------------------
/// ***** SetError
///
/// Utility function for taking an error, logging it, and setting it as the last error for the current thread.
/// This is an alternative to MakeError for when you already have an error made.
/// It may be preferred to instead use the OVR_SET_ERROR macro functions, as it
/// handles file/line functionality cleanly between debug and release.
///
void SetError(OVRError& ovrError);

/// -----------------------------------------------------------------------------
/// ***** MakeError
///
/// Utility function for making an error, logging it, and setting it as the last error for the current thread.
/// It's preferred to instead use the OVR_MAKE_ERROR macro functions, as they
/// handle file/line functionality cleanly between debug and release.
///
OVRError MakeError(ovrResult errorCode, ovrSysErrorCode sysCode, const char* pSourceFile,
                    int sourceLine, const char* pContext, const char* pDescriptionFormat, ...);


// -----------------------------------------------------------------------------
// ***** LastErrorTLS
//
// We don't use C++11 thread-local storage nor C-style __thread/__declsped(thread)
// to manager thread-local storage, as neither of these provide a means for us
// to control the lifetime of the data. Rather it can be controlled only 
// passively by the thread's lifetime. Similarly we don't use pthread_getspecific
// pthread_setspecific (and Windows equivalents) because they too don't let us
// control the lifetime of the data. Our solution is to have a map of threads
// to thread-specific data, and we can clear the entire map on ovrShutdown as-needed.
// this scheme is not as fast as the aforementioned schemes but it doesn't need to
// be fast for our use.
//
// We use pointers below instead of concrete objects because we want to have their 
// lifetimes entirely controlled by ovr_Initialize/ovr_Shutdown.

class LastErrorTLS : public NewOverrideBase, public SystemSingletonBase <LastErrorTLS>
{
    OVR_DECLARE_SINGLETON(LastErrorTLS);

public:
    OVRError& LastError();

protected:
    // Protect hash from multiple thread access.
    Lock TheLock;

    // Map thread-id to OVRError objects.
    typedef Hash<ThreadId, OVRError> TLSHash;
    TLSHash TLSDictionary;
};


/// -----------------------------------------------------------------------------
/// ***** GetErrorCodeString
///
/// Utility function which converts an ovrResult error code to a readable string version.
///
bool GetErrorCodeString(ovrResult errorCode, bool prefixErrorCode, OVR::String& sResult);


/// -----------------------------------------------------------------------------
/// ***** GetSysErrorCodeString
///
/// Utility function which converts a system error to a string. Similar to the Windows FormatMessage
/// function and the Unix strerror_r function.
/// If prefixErrorCode is true then the string is prefixed with "<code>: " 
/// Returns true if the sysErrorCode was a known valid code and a string was produced from it.
/// Else the returned string will be empty. The returned string may have tabs or newlines.
/// Users of OVR_MAKE_SYS_ERROR and MakeError don't need to call this function, as it's done
/// automatically internally.
///
bool GetSysErrorCodeString(ovrSysErrorCode sysErrorCode, bool prefixErrorCode, OVR::String& sResult);


// Temporarily placed this here until it can be moved to a better location -cat

/*
    OVR_D3D_CREATE(ObjectPtr, <Function-Call>);

    For the very common case of a Ptr<> wrapped D3D object pointer, this
    combines several very common code patterns into one line of code:

    (1) Assert in debug mode that the Ptr<> is null before the create call.
    (2) In release mode set to nullptr to avoid leaks.
    (3) Check its HRESULT and return an OVRError from the current function on failure.
    (4) Tag the object with a name containing its creation location.

    Exceptions to this pattern:
        Create*State() functions.  These seem to be cached and not recreated so the
        second time it is tagged with a name the tagging will emit a D3D warning.

    Usage example:

        Replace this:

            OVR_ASSERT(!BltState); // Expected to be null on the way in.
            BltState = nullptr; // Prevents a potential leak on the next line.
            HRESULT hr = Device->CreateDeviceContextState(
                stateFlags, &featureLevel, 1, D3D11_SDK_VERSION,
                __uuidof(ID3D11Device1), nullptr, &BltState.GetRawRef());
            OVR_D3D_CHECK_RET_FALSE(hr);
            OVR_D3D_TAG_OBJECT(BltState);

        With:

            OVR_D3D_CREATE(BltState, Device->CreateDeviceContextState(
                stateFlags, &featureLevel, 1, D3D11_SDK_VERSION,
                __uuidof(ID3D11Device1), nullptr, &BltState.GetRawRef()));

*/

#define OVR_D3D_CREATE_NOTAG(objPtr, functionCall) \
    { \
        OVR_ASSERT(!objPtr);   /* Expected to be null on the way in. */ \
        objPtr = nullptr;      /* Prevents a potential leak on the next line. */ \
        HRESULT d3dCreateResult = functionCall; /* Make the call */ \
        OVR_HR_CHECK_RET_ERROR(ovrError_Initialize, d3dCreateResult, OVR_STRINGIZE(functionCall)); \
        OVR_ASSERT(objPtr);    /* Expected to be non-null on the way out. */ \
    }

#define OVR_D3D_CREATE(objPtr, functionCall) \
    OVR_D3D_CREATE_NOTAG(objPtr, functionCall); \
    OVR_D3D_TAG_OBJECT(objPtr);


} // namespace OVR


#endif // OVR_Error_h
