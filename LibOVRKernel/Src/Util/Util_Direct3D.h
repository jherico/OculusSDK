/************************************************************************************

Filename    :   Util_Direct3D.h
Content     :   Shared code for Direct3D
Created     :   Oct 14, 2014
Authors     :   Chris Taylor

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

#ifndef OVR_Util_Direct3D_h
#define OVR_Util_Direct3D_h

// Include Windows correctly first before implicitly including it below
#include "Kernel/OVR_Win32_IncludeWindows.h"
#include "Kernel/OVR_String.h"

// DirectX 9 Ex
#include <d3d9.h>

// Direct3D 11
#include <D3D11Shader.h>
#include <d3dcompiler.h>

#if _MSC_VER >= 1800
    // Visual Studio 2013+ support newer D3D/DXGI headers.
    #define OVR_D3D11_VER 2
    #include <d3d11_2.h>
    #define OVR_DXGI_VER 3
    #include <dxgi1_3.h> // Used in place of 1.2 for IDXGIFactory2 debug version (when available)
#elif _MSC_VER >= 1700
    // Visual Studio 2012+ only supports older D3D/DXGI headers.
    #define OVR_D3D11_VER 1
    #include <d3d11_1.h>
#else
    // Visual Studio 2010+ only supports original D3D/DXGI headers.
    #define OVR_D3D11_VER 1
    #include <d3d11.h>
#endif

namespace OVR { namespace D3DUtil {


// Check if D3D9Ex support exists in the environment
bool CheckD3D9Ex();

String GetWindowsErrorString(HRESULT hr);


//-----------------------------------------------------------------------------
// Helper macros for verifying HRESULT values from Direct3D API calls
//
// These will assert on failure in debug mode, and in release or debug mode
// these functions will report the file and line where the error occurred,
// and what the error code was to the log at Error-level.

// Assert on HRESULT failure
bool VerifyHRESULT(const char* file, int line, HRESULT hr);

#define OVR_D3D_CHECK(hr) OVR::D3DUtil::VerifyHRESULT(__FILE__, __LINE__, hr)

// Returns provided value on failure
#define OVR_D3D_CHECK_RET_VAL(hr, failureValue) \
    { \
        if (!OVR_D3D_CHECK(hr)) \
        { \
            return failureValue; \
        } \
    }

// Returns void on failure
#define OVR_D3D_CHECK_RET(hr)       OVR_D3D_CHECK_RET_VAL(hr, ;)

// Returns false on failure
#define OVR_D3D_CHECK_RET_FALSE(hr) OVR_D3D_CHECK_RET_VAL(hr, false)

// Returns nullptr on failure
#define OVR_D3D_CHECK_RET_NULL(hr)  OVR_D3D_CHECK_RET_VAL(hr, nullptr)

// If the result is a failure, it will write the exact compile error to the error log
void LogD3DCompileError(HRESULT hr, ID3DBlob* errorBlob);


}} // namespace OVR::D3DUtil

#endif // OVR_Util_Direct3D_h
