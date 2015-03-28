/************************************************************************************

Filename    :   Util_Direct3D.cpp
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

#include "Util_Direct3D.h"

#include "Kernel/OVR_Log.h"

namespace OVR { namespace D3DUtil {


bool VerifyHRESULT(const char* file, int line, HRESULT hr)
{
    if (FAILED(hr))
    {
        LogError("D3D function returned fail HRESULT at %s on line %d : %s",
                 file, line, D3DUtil::GetWindowsErrorString(hr).ToCStr());
        OVR_ASSERT(false);
        return false;
    }

    return true;
}

bool CheckD3D9Ex()
{
    bool available = false;
    HMODULE libHandle = LoadLibraryW(L"d3d9.dll");

    if (libHandle != nullptr)
    {
        available = (GetProcAddress(libHandle, "Direct3DCreate9Ex") != nullptr);
        FreeLibrary(libHandle);
    }

    return available;
}

String GetWindowsErrorString(HRESULT hr)
{
    // DirectX 9 error strings
    switch (hr)
    {
    case D3DERR_WRONGTEXTUREFORMAT: return "D3DERR_WRONGTEXTUREFORMAT";
    case D3DERR_UNSUPPORTEDCOLOROPERATION: return "D3DERR_UNSUPPORTEDCOLOROPERATION";
    case D3DERR_UNSUPPORTEDCOLORARG: return "D3DERR_UNSUPPORTEDCOLORARG";
    case D3DERR_UNSUPPORTEDALPHAOPERATION: return "D3DERR_UNSUPPORTEDALPHAOPERATION";
    case D3DERR_UNSUPPORTEDALPHAARG: return "D3DERR_UNSUPPORTEDALPHAARG";
    case D3DERR_TOOMANYOPERATIONS: return "D3DERR_TOOMANYOPERATIONS";
    case D3DERR_CONFLICTINGTEXTUREFILTER: return "D3DERR_CONFLICTINGTEXTUREFILTER";
    case D3DERR_UNSUPPORTEDFACTORVALUE: return "D3DERR_UNSUPPORTEDFACTORVALUE";
    case D3DERR_CONFLICTINGRENDERSTATE: return "D3DERR_CONFLICTINGRENDERSTATE";
    case D3DERR_UNSUPPORTEDTEXTUREFILTER: return "D3DERR_UNSUPPORTEDTEXTUREFILTER";
    case D3DERR_CONFLICTINGTEXTUREPALETTE: return "D3DERR_CONFLICTINGTEXTUREPALETTE";
    case D3DERR_DRIVERINTERNALERROR: return "D3DERR_DRIVERINTERNALERROR";
    case D3DERR_NOTFOUND: return "D3DERR_NOTFOUND";
    case D3DERR_MOREDATA: return "D3DERR_MOREDATA";
    case D3DERR_DEVICELOST: return "D3DERR_DEVICELOST";
    case D3DERR_DEVICENOTRESET: return "D3DERR_DEVICENOTRESET";
    case D3DERR_NOTAVAILABLE: return "D3DERR_NOTAVAILABLE";
    case D3DERR_OUTOFVIDEOMEMORY: return "D3DERR_OUTOFVIDEOMEMORY";
    case D3DERR_INVALIDDEVICE: return "D3DERR_INVALIDDEVICE";
    case D3DERR_INVALIDCALL: return "D3DERR_INVALIDCALL";
    case D3DERR_DRIVERINVALIDCALL: return "D3DERR_DRIVERINVALIDCALL";
    case D3DERR_WASSTILLDRAWING: return "D3DERR_WASSTILLDRAWING";
    case D3DOK_NOAUTOGEN: return "D3DOK_NOAUTOGEN";
    case D3DERR_DEVICEREMOVED: return "D3DERR_DEVICEREMOVED";
    case S_NOT_RESIDENT: return "S_NOT_RESIDENT";
    case S_RESIDENT_IN_SHARED_MEMORY: return "S_RESIDENT_IN_SHARED_MEMORY";
    case S_PRESENT_MODE_CHANGED: return "S_PRESENT_MODE_CHANGED";
    case S_PRESENT_OCCLUDED: return "S_PRESENT_OCCLUDED";
    case D3DERR_DEVICEHUNG: return "D3DERR_DEVICEHUNG";
    case D3DERR_UNSUPPORTEDOVERLAY: return "D3DERR_UNSUPPORTEDOVERLAY";
    case D3DERR_UNSUPPORTEDOVERLAYFORMAT: return "D3DERR_UNSUPPORTEDOVERLAYFORMAT";
    case D3DERR_CANNOTPROTECTCONTENT: return "D3DERR_CANNOTPROTECTCONTENT";
    case D3DERR_UNSUPPORTEDCRYPTO: return "D3DERR_UNSUPPORTEDCRYPTO";
    case D3DERR_PRESENT_STATISTICS_DISJOINT: return "D3DERR_PRESENT_STATISTICS_DISJOINT";
    default: break; // Not a DirectX 9 error, use FormatMessageA...
    }

    char errorTextAddr[256] = {};

    DWORD slen = FormatMessageA(
        // use system message tables to retrieve error text
        FORMAT_MESSAGE_FROM_SYSTEM
        // allocate buffer on local heap for error text
        | FORMAT_MESSAGE_ALLOCATE_BUFFER
        // Important! will fail otherwise, since we're not 
        // (and CANNOT) pass insertion parameters
        | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,    // unused with FORMAT_MESSAGE_FROM_SYSTEM
        hr,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        errorTextAddr,  // output 
        256, // minimum size for output buffer
        nullptr);   // arguments - see note 

    char* errorText = *(char**)errorTextAddr;

    char formatStr[512];
    OVR_snprintf(formatStr, sizeof(formatStr), "[Code=%x = %d]", hr, hr);

    String retStr = formatStr;

    if (slen > 0 && errorText)
    {
        retStr += " ";
        retStr += errorText;

        // release memory allocated by FormatMessage()
        LocalFree(errorText);
    }

    return retStr;
}

void LogD3DCompileError(HRESULT hr, ID3DBlob* blob)
{
    if (FAILED(hr))
    {
        char* errStr = (char*)blob->GetBufferPointer();
        SIZE_T len = blob->GetBufferSize();

        if (errStr && len > 0)
        {
            LogError("Error compiling shader: %s", errStr);
        }
    }
}


}} // namespace OVR::D3DUtil
