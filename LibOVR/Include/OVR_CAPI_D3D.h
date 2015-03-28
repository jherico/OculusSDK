/************************************************************************************

Filename    :   OVR_CAPI_D3D.h
Content     :   D3D specific structures used by the CAPI interface.
Created     :   November 7, 2013
Authors     :   Michael Antonov

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

#ifndef OVR_CAPI_D3D_h
#define OVR_CAPI_D3D_h

/// @file OVR_CAPI_D3D.h
/// D3D rendering support.

#include "OVR_CAPI.h"

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4324) // structure was padded due to __declspec(align())
#endif

#if !defined(OVR_DEPRECATED)
    #if defined(_MSC_VER)
        #define OVR_DEPRECATED          __declspec(deprecated)
        #define OVR_DEPRECATED_MSG(msg) __declspec(deprecated(msg))
    #endif
#endif


//-----------------------------------------------------------------------------------
// ***** D3D11 Specific

#include <d3d11.h>

/// Used to configure slave D3D rendering (i.e. for devices created externally).
struct ovrD3D11ConfigData
{
    ovrRenderAPIConfigHeader    Header;             ///< General device settings.
    ID3D11Device*               pDevice;            ///< The D3D device to use for rendering.
    ID3D11DeviceContext*        pDeviceContext;     ///< The D3D device context to use for rendering.
    ID3D11RenderTargetView*     pBackBufferRT;      ///< A render target view for the backbuffer.
    ID3D11UnorderedAccessView*  pBackBufferUAV;     ///< A UAV for the backbuffer (if using compute shaders)
    IDXGISwapChain*             pSwapChain;         ///< The swapchain that will present rendered frames.
};

#if defined(__cplusplus)
    static_assert(sizeof(ovrRenderAPIConfig) >= sizeof(ovrD3D11ConfigData), "Insufficient size.");
#endif

/// Contains D3D11-specific rendering information.
union ovrD3D11Config
{
    ovrRenderAPIConfig Config;      ///< General device settings.
    ovrD3D11ConfigData D3D11;       ///< D3D11-specific settings.
};

/// Used to pass D3D11 eye texture data to ovrHmd_EndFrame.
struct ovrD3D11TextureData
{
    ovrTextureHeader          Header;   ///< General device settings.
    ID3D11Texture2D*          pTexture; ///< The D3D11 texture containing the undistorted eye image.
    ID3D11ShaderResourceView* pSRView;  ///< The D3D11 shader resource view for this texture.
};

#if defined(__cplusplus)
    static_assert(sizeof(ovrTexture) >= sizeof(ovrD3D11TextureData), "Insufficient size.");
#endif

/// Contains OpenGL-specific texture information.
union ovrD3D11Texture
{
    ovrTexture          Texture;    ///< General device settings.
    ovrD3D11TextureData D3D11;      ///< D3D11-specific settings.
};

//-----------------------------------------------------------------------------------
// ***** D3D9 Specific

#include <d3d9.h>

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4996) // Disable deprecation warning
#endif

///@cond DoxygenIgnoreCode
// Used to configure D3D9 rendering 
struct OVR_DEPRECATED ovrD3D9ConfigData
{
    ovrRenderAPIConfigHeader Header;
    IDirect3DDevice9*        pDevice;
    IDirect3DSwapChain9*     pSwapChain;
};

union OVR_DEPRECATED ovrD3D9Config
{
    ovrRenderAPIConfig Config;
    ovrD3D9ConfigData  D3D9;
};

// Used to pass D3D9 eye texture data to ovrHmd_EndFrame.
struct OVR_DEPRECATED ovrD3D9TextureData
{
    ovrTextureHeader    Header;
    IDirect3DTexture9*  pTexture;
};

union OVR_DEPRECATED ovrD3D9Texture
{
    ovrTexture         Texture;
    ovrD3D9TextureData D3D9;
};
///@endcond

#if defined(_MSC_VER)
    #pragma warning(pop)
    #pragma warning(pop)
#endif

#endif    // OVR_CAPI_h
