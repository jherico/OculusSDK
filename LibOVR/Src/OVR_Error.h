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
#include <chrono>
#include <utility>


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


namespace OVR
{
    /// -----------------------------------------------------------------------------
    /// ***** OVR_MAKE_ERROR, OVR_MAKE_SYS_ERROR
    ///
    /// Declaration:
    ///      OVRError OVR_MAKE_ERROR(ovrResult code, const char* pDescription);
    ///      OVRError OVR_MAKE_SYS_ERROR(ovrResult code, ovrSysErrorCode sysCode, const char* pDescription);
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
    ///
    ///          return ovrSuccess; // Converts to an OVRError instance that has no error.
    ///      }
    ///
    #define OVR_MAKE_ERROR(errorCode, pDescription) \
        OVR::MakeError((errorCode), OVR::ovrSysErrorCodeSuccess, OVR_FILE, OVR_LINE, nullptr, "%s", (pDescription))

    #define OVR_MAKE_ERROR_F(errorCode, pDescriptionFormat, ...) \
        OVR::MakeError((errorCode), OVR::ovrSysErrorCodeSuccess, OVR_FILE, OVR_LINE, nullptr, (pDescriptionFormat), __VA_ARGS__)


    #define OVR_MAKE_SYS_ERROR(errorCode, sysErrorCode, pDescription) \
        OVR::MakeError((errorCode), (sysErrorCode), OVR_FILE, OVR_LINE, nullptr, "%s", (pDescriptionFormat))

    #define OVR_MAKE_SYS_ERROR_F(errorCode, sysErrorCode, pDescriptionFormat, ...) \
        OVR::MakeError((errorCode), (sysErrorCode), OVR_FILE, OVR_LINE, nullptr, (pDescriptionFormat), __VA_ARGS__)
    



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
    public:
        OVRError();
        OVRError(ovrResult code); // Intentionally not explicit.

        OVRError(const OVRError& OVRError);
        OVRError(OVRError&& OVRError);

        virtual ~OVRError();

        OVRError& operator=(const OVRError& OVRError);
        OVRError& operator=(OVRError&& OVRError);

        operator bool() const;
        
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
    };



    /// -----------------------------------------------------------------------------
    /// ***** MakeError
    ///
    /// Utility function for making an error and logging it.
    /// It's preferred to instead use the OVR_MAKE_ERROR macro functions, as they
    /// handle file/line functionality cleanly between debug and release.
    ///
    OVRError MakeError(ovrResult errorCode, ovrSysErrorCode sysCode, const char* pSourceFile,
                        int sourceLine, const char* pContext, const char* pDescriptionFormat, ...);
    

    /// -----------------------------------------------------------------------------
    /// ***** GetErrorCodeString
    ///
    /// Converts an ovrResult error code to a readable string version.
    ///
    bool GetErrorCodeString(ovrResult errorCode, bool prefixErrorCode, OVR::String& sResult);


    /// -----------------------------------------------------------------------------
    /// ***** GetSysErrorCodeString
    ///
    /// Converts a system error to a string. Similar to the Windows FormatMessage function.
    /// If prefixErrorCode is true then the string is prefixed with "<code>: " 
    /// Returns true if the sysErrorCode was a known valid code and a string was produced from it.
    /// Else the returned string will be empty. The returned string may have tabs or newlines.
    /// Users of OVR_MAKE_SYS_ERROR and MakeError don't need to call this function, as it's done
    /// automatically internally.
    ///
    bool GetSysErrorCodeString(ovrSysErrorCode sysErrorCode, bool prefixErrorCode, OVR::String& sResult);


} // namespace OVR



#endif // Header include guard



