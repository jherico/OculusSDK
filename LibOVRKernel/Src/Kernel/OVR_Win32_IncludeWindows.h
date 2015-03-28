/************************************************************************************

Filename    :   OVR_Win32_IncludeWindows.h
Content     :   Small helper header to include Windows.h properly
Created     :   Oct 16, 2014
Authors     :   Chris Taylor, Scott Bassett

Copyright   :   Copyright 2014 Oculus, Inc. All Rights reserved.

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

*************************************************************************************/

#ifndef OVR_Win32_IncludeWindows_h
#define OVR_Win32_IncludeWindows_h

#include "OVR_Types.h"

// Automatically avoid including the Windows header on non-Windows platforms.
#ifdef OVR_OS_MS

// It is common practice to define WIN32_LEAN_AND_MEAN to reduce compile times.
// However this then requires us to define our own NTSTATUS data type and other
// irritations throughout our code-base.
//#define WIN32_LEAN_AND_MEAN

// Prevents <Windows.h> from #including <Winsock.h>, as we use <Winsock2.h> instead.
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

// Prevents <Windows.h> from defining min() and max() macro symbols.
#ifndef NOMINMAX
#define NOMINMAX
#endif

// We support Windows Windows 7 or newer.
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 /* Windows 7+ */
#endif

#include <Windows.h>

namespace OVR {


//-----------------------------------------------------------------------------
// ScopedHANDLE
//
// MSVC 2010, and MSVC 2012 do not support the <wrl.h> utility header.
// So we provide our own version of HandleRAII, which is an incredibly
// useful pattern for working with Windows HANDLE values.
//
// HANDLEs have two invalid values in Windows, either nullptr or
// INVALID_HANDLE_VALUE.  The invalid value that is correct for the usage must
// be provided as the template argument.

struct ScopedHANDLE_NullTraits
{
    // We cannot make this a static member variable as it is not an integral type.
    inline static HANDLE InvalidValue()
    {
        return nullptr;
    }
};
struct ScopedHANDLE_InvalidTraits
{
    inline static HANDLE InvalidValue()
    {
        return INVALID_HANDLE_VALUE;
    }
};

template<typename Traits>
class ScopedHANDLE
{
    HANDLE hAttachedHandle;

public:
    ScopedHANDLE(HANDLE handle) :
        hAttachedHandle(handle)
    {
    }
    ScopedHANDLE()
    {
        hAttachedHandle = Traits::InvalidValue();
    }
    ScopedHANDLE& operator=(HANDLE handle)
    {
        Close();
        hAttachedHandle = handle;
        return *this;
    }
    ~ScopedHANDLE()
    {
        Close();
    }

    bool IsValid()
    {
        return hAttachedHandle != Traits::InvalidValue();
    }
    HANDLE Get()
    {
        return hAttachedHandle;
    }
    HANDLE& GetRawRef()
    {
        return hAttachedHandle;
    }
    void Attach(HANDLE handle)
    {
        Close();
        hAttachedHandle = handle;
    }
    void Detach()
    {
        // Do not close handle
        hAttachedHandle = Traits::InvalidValue();
    }
    bool Close()
    {
        bool success = true;
        if (hAttachedHandle != Traits::InvalidValue())
        {
            if (::CloseHandle(hAttachedHandle) != TRUE)
            {
                success = false;
            }
            hAttachedHandle = Traits::InvalidValue();
        }
        return success;
    }
};

// Different Windows API functions have different invalid values.
// These types are provided to improve readability.
typedef ScopedHANDLE < ScopedHANDLE_NullTraits > ScopedEventHANDLE;
typedef ScopedHANDLE < ScopedHANDLE_InvalidTraits > ScopedFileHANDLE;
typedef ScopedHANDLE < ScopedHANDLE_NullTraits > ScopedProcessHANDLE;

// Scoped registry keys
class ScopedHKEY
{
    HKEY hAttachedHandle;

public:
    ScopedHKEY(HKEY handle) :
        hAttachedHandle(handle)
    {
    }
    ScopedHKEY()
    {
        hAttachedHandle = nullptr;
    }
    ScopedHKEY& operator=(HKEY handle)
    {
        Close();
        hAttachedHandle = handle;
        return *this;
    }
    ~ScopedHKEY()
    {
        Close();
    }

    bool IsValid()
    {
        return hAttachedHandle != nullptr;
    }
    HKEY Get()
    {
        return hAttachedHandle;
    }
    HKEY& GetRawRef()
    {
        return hAttachedHandle;
    }
    void Attach(HKEY handle)
    {
        Close();
        hAttachedHandle = handle;
    }
    void Detach()
    {
        // Do not close handle
        hAttachedHandle = nullptr;
    }
    bool Close()
    {
        bool success = true;
        if (hAttachedHandle != nullptr)
        {
            if (::RegCloseKey(hAttachedHandle) == ERROR_SUCCESS)
            {
                success = false;
            }
            hAttachedHandle = nullptr;
        }
        return success;
    }
};


} // namespace OVR


#endif // OVR_OS_WIN32

#endif // OVR_Win32_IncludeWindows_h
