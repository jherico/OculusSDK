/************************************************************************************

PublicHeader:   None
Filename    :   OVR_Error.cpp
Content     :   Structs and functions for handling OVRErrorInfos
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


#include "OVR_Error.h"
#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_Timer.h"
#include "Kernel/OVR_DebugHelp.h"
#include "Kernel/OVR_Hash.h"
#include "Kernel/OVR_Atomic.h"
#include "Kernel/OVR_UTF8Util.h"
#include "Kernel/OVR_Threads.h"
#include "Kernel/OVR_Win32_IncludeWindows.h"

OVR_DISABLE_ALL_MSVC_WARNINGS()
OVR_DISABLE_MSVC_WARNING(4265)
#include <utility>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <stdarg.h>
#include <cstdio>
#include <mutex>
OVR_RESTORE_MSVC_WARNING()
OVR_RESTORE_ALL_MSVC_WARNINGS()

OVR_DEFINE_SINGLETON(OVR::LastErrorTLS);

OVR_DISABLE_MSVC_WARNING(4996) // 'localtime': This function or variable may be unsafe. 



// -----------------------------------------------------------------------------
// ***** OVR_ERROR_ENABLE_BACKTRACES
//
// If defined then we record backtraces in Errors.
//
#if !defined(OVR_ERROR_ENABLE_BACKTRACES)
    #if defined(OVR_BUILD_DEBUG)
        #define OVR_ERROR_ENABLE_BACKTRACES 1
    #endif
#endif




// -----------------------------------------------------------------------------
// ***** GetErrorDescription
//
// Returns a string representation of an ovrResult.
//
struct ErrorDescriptionPair
{
    ovrResult   Result;
    const char* Description;
};

#define OVR_ERROR_ENTRY(ovrResult) { ovrResult, #ovrResult }

// Problem: This system is fragile and makes it easy to forget to add error entries. We should consider 
// coming up with a way to declare error code such that they don't get missed. As it currently stands,
// a missing entry here means only that logged error codes will be only numbers and not readable names.
static const ErrorDescriptionPair errorDescriptionArray[]
{
    OVR_ERROR_ENTRY(ovrSuccess),

    OVR_ERROR_ENTRY(ovrError_MemoryAllocationFailure),
    OVR_ERROR_ENTRY(ovrError_SocketCreationFailure),
    OVR_ERROR_ENTRY(ovrError_InvalidHmd),
    OVR_ERROR_ENTRY(ovrError_Timeout),
    OVR_ERROR_ENTRY(ovrError_NotInitialized),
    OVR_ERROR_ENTRY(ovrError_InvalidParameter),
    OVR_ERROR_ENTRY(ovrError_ServiceError),
    OVR_ERROR_ENTRY(ovrError_NoHmd),

    OVR_ERROR_ENTRY(ovrError_Initialize),
    OVR_ERROR_ENTRY(ovrError_LibLoad),
    OVR_ERROR_ENTRY(ovrError_LibVersion),
    OVR_ERROR_ENTRY(ovrError_ServiceConnection),
    OVR_ERROR_ENTRY(ovrError_ServiceVersion),
    OVR_ERROR_ENTRY(ovrError_IncompatibleOS),
    OVR_ERROR_ENTRY(ovrError_DisplayInit),
    OVR_ERROR_ENTRY(ovrError_ServerStart),
    OVR_ERROR_ENTRY(ovrError_Reinitialization),

    OVR_ERROR_ENTRY(ovrError_InvalidBundleAdjustment),
    OVR_ERROR_ENTRY(ovrError_USBBandwidth),
};

static const char* GetErrorDescription(ovrResult errorCode)
{
    // We choose not to use std::lower_bound because it would require us to be diligent
    // about maintaining ordering in the array, but wouldn't buy much in practice, given
    // that there aren't very many errors and this function wouldn't be called often.
    
    for (size_t i = 0; i < OVR_ARRAY_COUNT(errorDescriptionArray); ++i)
    {
        if (errorDescriptionArray[i].Result == errorCode)
            return errorDescriptionArray[i].Description;
    }
    
    OVR_FAIL_M("Undocumented error. The errorCode needs to be added to the array above.");
    return "Undocumented error";
};


namespace OVR {


//-----------------------------------------------------------------------------
// LastErrorTLS

static SymbolLookup Symbols;

LastErrorTLS::LastErrorTLS()
{
    Symbols.Initialize();

    // Must be at end of function
    PushDestroyCallbacks();
}

LastErrorTLS::~LastErrorTLS()
{
    Symbols.Shutdown();
}

void LastErrorTLS::OnSystemDestroy()
{
    delete this;
}

// This is an accessor which auto-allocates and initializes the return value if needed.
OVRError& LastErrorTLS::LastError()
{
    Lock::Locker autoLock(&TheLock);

    ThreadId threadId = GetCurrentThreadId();
    auto i = TLSDictionary.Find(threadId);

    if (i == TLSDictionary.End())
    {
        TLSDictionary.Add(threadId, OVRError::Success());
        i = TLSDictionary.Find(threadId);
    }

    return (*i).Second;
}




// ****** OVRFormatDateTime
//
// Prints a date/time like so:
//     Y-M-d H:M:S [ms:us:ns]
// Example output:
//     2016-12-25 8:15:01 [392:445:23]
//
// SysClockTime is of type std::chrono::time_point<std::chrono::system_clock>.
//
// To consider: Move SysClockTime and OVRFormatDateTime to OVRKernel.
//
static void OVRFormatDateTime(SysClockTime sysClockTime, OVR::String& dateTimeString)
{
    // Get the basic Date and HMS time.
    char buffer[128];
    struct tm tmResult;
    const time_t cTime = std::chrono::system_clock::to_time_t(sysClockTime);

    #if defined(_MSC_VER)
        localtime_s(&tmResult, &cTime);
    #else
        localtime_r(&cTime, &tmResult);
    #endif

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tmResult);

    // Append milli:micro:nano time.
    std::chrono::system_clock::duration timeSinceEpoch = sysClockTime.time_since_epoch();

    std::chrono::seconds       s = std::chrono::duration_cast<std::chrono::seconds>(timeSinceEpoch);
    timeSinceEpoch -= s;

    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeSinceEpoch);
    timeSinceEpoch -= ms;

    std::chrono::microseconds us = std::chrono::duration_cast<std::chrono::microseconds>(timeSinceEpoch);
    timeSinceEpoch -= us;

    std::chrono::nanoseconds  ns = std::chrono::duration_cast<std::chrono::nanoseconds>(timeSinceEpoch);
    
    char buffer2[384];
    sprintf(buffer2, "%s [%d:%d:%d]", buffer, (int)ms.count(), (int)us.count(), (int)ns.count());

    dateTimeString = buffer2;
}
    
    



OVR_SELECTANY const int64_t OVRError::kLogLineUnset;


OVRError::OVRError()
{
    Reset();
}
    
    
OVRError::OVRError(ovrResult code)
    : OVRError()
{
    Code = code;
}
    

OVRError::OVRError(ovrResult code, const char* pFormat, ...)
    : OVRError(code)
{
    OVR_ASSERT(code <= 0); // Valid error codes are not positive.
    va_list argList;
    va_start(argList, pFormat);
    StringBuffer strbuff;
    strbuff.AppendFormatV(pFormat, argList);
    SetDescription(strbuff.ToCStr());
    va_end(argList);
}

OVRError::OVRError(const OVRError& ovrError)
{
    operator=(ovrError);
}
    

OVRError::OVRError(OVRError&& ovrError)
{
    operator=(std::move(ovrError));
}


OVRError::~OVRError()
{
    // Empty
}


OVRError& OVRError::operator=(const OVRError& ovrError)
{
    Code             = ovrError.Code;
    SysCode          = ovrError.SysCode;
    Description      = ovrError.Description;
    Context          = ovrError.Context;
    OVRTime          = ovrError.OVRTime;
    ClockTime        = ovrError.ClockTime;
    LogLine          = ovrError.LogLine;
    SourceFilePath   = ovrError.SourceFilePath;
    SourceFileLine   = ovrError.SourceFileLine;
    Backtrace        = ovrError.Backtrace;
    AlreadyLogged    = ovrError.AlreadyLogged;
        
    return *this;
}
    

OVRError& OVRError::operator=(OVRError&& ovrError)
{
    Code             = ovrError.Code;
    SysCode          = ovrError.SysCode;
    Description      = std::move(ovrError.Description);
    Context          = std::move(ovrError.Context);
    OVRTime          = ovrError.OVRTime;
    ClockTime        = ovrError.ClockTime;
    LogLine          = ovrError.LogLine;
    SourceFilePath   = std::move(ovrError.SourceFilePath);
    SourceFileLine   = ovrError.SourceFileLine;
    Backtrace        = std::move(ovrError.Backtrace);
    AlreadyLogged    = ovrError.AlreadyLogged;
        
    return *this;
}

    
void OVRError::SetCurrentValues()
{
    OVRTime = Timer::GetSeconds(); // It would be better if we called ovr_GetTimeInSeconds, but that doesn't have a constant header to use.
        
    ClockTime = std::chrono::system_clock::now();
        
    #if defined(OVR_ERROR_ENABLE_BACKTRACES)
        if (Symbols.IsInitialized())
        {
            void* addressArray[32];
            size_t n = Symbols.GetBacktrace(addressArray, OVR_ARRAY_COUNT(addressArray), 2,
                                            nullptr, OVR_THREADSYSID_INVALID);
            Backtrace.Clear();
            Backtrace.Append(addressArray, n);
        }
    #endif
}
    

void OVRError::Reset()
{
    Code = ovrSuccess;
    SysCode = ovrSysErrorCodeSuccess;
    Description.Clear();
    Context.Clear();
    OVRTime = 0;
    ClockTime = SysClockTime();
    LogLine = kLogLineUnset;
    SourceFilePath.Clear();
    SourceFileLine = 0;
    Backtrace.ClearAndRelease();
    AlreadyLogged = false;
}


String OVRError::GetErrorString() const
{
    StringBuffer stringBuffer("OVR Error:\n");
        
    // OVRTime
    stringBuffer.AppendFormat("  OVRTime: %f\n", OVRTime);
        
    // SysClockTime
    OVR::String sysClockTimeString;
    OVRFormatDateTime(ClockTime, sysClockTimeString);
    stringBuffer.AppendFormat("  Time: %s\n", sysClockTimeString.ToCStr());

    // Code
    OVR::String errorCodeString;
    GetErrorCodeString(Code, false, errorCodeString);
    stringBuffer.AppendFormat("  Code: %d -- %s\n", Code, errorCodeString.ToCStr());
        
    // SysCode
    if (SysCode != ovrSysErrorCodeSuccess)
    {
        OVR::String sysErrorString;
        GetSysErrorCodeString(SysCode, false, sysErrorString);
        stringBuffer.AppendFormat("  System error: %d (%x) -- %s\n", (int)SysCode, (int)SysCode, sysErrorString.ToCStr());
    }

    // Description
    if (Description.GetLength())
    {
        stringBuffer.AppendFormat("  Description: %s\n", Description.ToCStr());
    }
        
    // Context
    if (Context.GetLength())
    {
        stringBuffer.AppendFormat("  Context: %s\n", Context.ToCStr());
    }
        
    // If LogLine is set,
    if (LogLine != kLogLineUnset)
    {
        stringBuffer.AppendFormat("  LogLine: %lld\n", LogLine);
    }

    // FILE/LINE
    if (SourceFilePath.GetLength())
    {
        stringBuffer.AppendFormat("  File/Line: %s:%d\n", SourceFilePath.ToCStr(), SourceFileLine);
    }

    // Backtrace
    if (Backtrace.GetSize())
    {
        // We can trace symbols in a debug build here or we can trace just addresses. See other code for
        // examples of how to trace symbols.
        stringBuffer.AppendFormat("  Backtrace: ");
        for (size_t i = 0, iEnd = Backtrace.GetSize(); i != iEnd; ++i)
            stringBuffer.AppendFormat(" %p", Backtrace[i]);
        stringBuffer.AppendChar('\n');
    }

    return OVR::String(stringBuffer.ToCStr(), stringBuffer.GetSize());
}


void OVRError::SetCode(ovrResult code)
{
    Code = code;
}


ovrResult OVRError::GetCode() const
{
    return Code;
}


void OVRError::SetSysCode(ovrSysErrorCode sysCode)
{
    SysCode = sysCode;
}


ovrSysErrorCode OVRError::GetSysCode() const
{
    return SysCode;
}


void OVRError::SetDescription(const char* pDescription)
{
    if (pDescription)
        Description = pDescription;
    else
        Description.Clear();
}
    

String OVRError::GetDescription() const
{
    return Description;
}


void OVRError::SetContext(const char* pContext)
{
    if (pContext)
        Context = pContext;
    else
        Context.Clear();
}


String OVRError::GetContext() const
{
    return Context;
}


void OVRError::SetOVRTime(double ovrTime)
{
    OVRTime = ovrTime;
}


double OVRError::GetOVRTime() const
{
    return OVRTime;
}


void OVRError::SetSysClockTime(const SysClockTime& clockTime)
{
    ClockTime = clockTime;
}
    
    
SysClockTime OVRError::GetSysClockTime() const
{
    return ClockTime;
}


void OVRError::SetLogLine(int64_t logLine)
{
    LogLine = logLine;
}
    

int64_t OVRError::GetLogLine() const
{
    return LogLine;
}


void OVRError::SetSource(const char* pSourceFilePath, int sourceFileLine)
{
    if (pSourceFilePath)
        SourceFilePath = pSourceFilePath;
    else
        SourceFilePath.Clear();
    SourceFileLine = sourceFileLine;
}


std::pair<OVR::String, int> OVRError::GetSource() const
{
    return std::make_pair(SourceFilePath, SourceFileLine);
}
    

OVRError::AddressArray OVRError::GetBacktrace() const
{
    return Backtrace;
}


/*
size_t OVRError::GetBacktrace(void* addressArray[], size_t addressArrayCapacity)
{
    addressArrayCapacity = std::min(addressArrayCapacity, Backtrace.GetSize());
        
    for (size_t i = 0; i != addressArrayCapacity; ++i)
    {
        addressArray[i] = Backtrace[i];
    }
        
    return addressArrayCapacity;
}
*/



// To consider: promote this function to the header file, so a user can Log an error
static void LogError(OVRError& ovrError)
{
    // If not already logged,
    if (!ovrError.IsAlreadyLogged())
    {
        const String errorString(ovrError.GetErrorString());

        OVR::LogText("%s", errorString.ToCStr());

        ovrError.SetAlreadyLogged();
    }
}

void SetError(OVRError& ovrError)
{
    LogError(ovrError);

    // Record that the current thread's last error is this error. If we wanted to support
    // chaining of errors such that multiple OVRErrors could be concurrent in a thread
    // (e.g. one that occurred deep in the call chain and a higher level version of it higher
    // in the call chain), we could handle that here.
    LastErrorTLS::GetInstance()->LastError() = ovrError;

    // Assert in debug mode to alert unit tester/developer of the error as it occurs.
    OVR_FAIL_M(ovrError.GetDescription().ToCStr());
}

OVRError MakeError(ovrResult errorCode, ovrSysErrorCode sysCode, const char* pSourceFile,
                    int sourceLine, const char* pContext, const char* pDescriptionFormat, ...)
{
    OVRError ovrError(errorCode);
        
    ovrError.SetCurrentValues(); // Sets the current time, etc.

    ovrError.SetSysCode(sysCode);

    va_list argList;
    va_start(argList, pDescriptionFormat);
    StringBuffer strbuff;
    strbuff.AppendFormatV(pDescriptionFormat, argList);
    va_end(argList);
    ovrError.SetDescription(strbuff.ToCStr());
    if (pContext)
        ovrError.SetContext(pContext);
        
    if (pSourceFile)
        ovrError.SetSource(pSourceFile, sourceLine);

    SetError(ovrError);

    return ovrError;
}

    

bool GetErrorCodeString(ovrResult errorCode, bool prefixErrorCode, OVR::String& sResult)
{
    char codeBuffer[256];
        
    if (prefixErrorCode)
    {
        OVR_snprintf(codeBuffer, OVR_ARRAY_COUNT(codeBuffer), "0x%llx (%lld): %s", (uint64_t)errorCode, (int64_t)errorCode, GetErrorDescription(errorCode));
    }
    else
    {
        OVR_snprintf(codeBuffer, OVR_ARRAY_COUNT(codeBuffer), "%s", GetErrorDescription(errorCode));
    }

    sResult = codeBuffer;
        
    return true;
}



bool GetSysErrorCodeString(ovrSysErrorCode sysErrorCode, bool prefixErrorCode, OVR::String& sResult)
{
    char errorBuffer[1024];
    errorBuffer[0] = '\0';

    if (prefixErrorCode)
    {
        char prefixBuffer[64];
        OVR_snprintf(prefixBuffer, OVR_ARRAY_COUNT(prefixBuffer), "0x%llx (%lld): ", (uint64_t)sysErrorCode, (int64_t)sysErrorCode);
        sResult = prefixBuffer;
    }
    else
    {
        sResult.Clear();
    }

    #if defined(OVR_OS_WIN32)
        // Note: It may be useful to use FORMAT_MESSAGE_FROM_HMODULE here to get a module-specific error string if our source of errors 
        // ends up including more than just system-native errors. For example, a third party module with custom errors defined in it.

        WCHAR errorBufferW[1024];
        DWORD errorBufferWCapacity = OVR_ARRAY_COUNT(errorBufferW);
        DWORD length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, (DWORD)sysErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorBufferW, errorBufferWCapacity, nullptr);
        
        if (length) // If FormatMessageW succeeded...
        {
            // Need to convert WCHAR errorBuffer to UTF8 char sResult;
            const size_t requiredUTF8Length = (size_t)OVR::UTF8Util::GetEncodeStringSize(errorBufferW); // Returns required strlen.

            if (requiredUTF8Length < OVR_ARRAY_COUNT(errorBuffer)) // If enough capacity...
                OVR::UTF8Util::EncodeString(errorBuffer, errorBufferW, -1);
            // Else fall through
        } // Else fall through
    #else
        #if (((_POSIX_C_SOURCE >= 200112L) || (_XOPEN_SOURCE >= 600)) && !_GNU_SOURCE) || defined(__APPLE__) || defined(__BSD__)
            const int result = strerror_r((int)sysErrorCode, errorBuffer, OVR_ARRAY_COUNT(errorBuffer));

            if (result != 0)        // If failed... [result is 0 upon success; result will be EINVAL if the code is not recognized; ERANGE if buffer didn't have enough capacity.]
                errorBuffer[0] = '\0';  // re-null-terminate, in case strerror_r left it in an invalid state.
        #else
            const char* result = strerror_r((int)sysErrorCode, errorBuffer, OVR_ARRAY_COUNT(errorBuffer));

            if (result == nullptr)  // Implementations in practice seem to always return a pointer, though the behavior isn't formally standardized.
                errorBuffer[0] = '\0';  // re-null-terminate, in case strerror_r left it in an invalid state.
        #endif
    #endif

    // Fall through functionality of simply printing the value as an integer.
    if (errorBuffer[0]) // If errorBuffer was successfully written above...
    {
        sResult += errorBuffer;
        return true;
    }
        
    sResult += "(unknown)"; // This is not localized. Question: Is there a way to get the error formatting functions above to print this themselves in a localized way?
    return false;
}


}  // namespace OVR
