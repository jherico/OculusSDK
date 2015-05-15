/********************************************************************************//**

\file OVR_CAPI_D3D.h
\brief D3D specific structures used by the CAPI interface.
\date November 7, 2013
\author Michael Antonov

\copyright Copyright 2014 Oculus VR, LLC All Rights reserved.
\n
Licensed under the Oculus VR Rift SDK License Version 3.2 (the "License");
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.
\n
You may obtain a copy of the License at
\n
http://www.oculusvr.com/licenses/LICENSE-3.2 
\n
Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#ifndef OVR_CAPI_D3D_h
#define OVR_CAPI_D3D_h

#include "OVR_CAPI.h"

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4324) // structure was padded due to __declspec(align())
#endif

//-----------------------------------------------------------------------------------
// ***** D3D11 Specific
#if defined(_MSC_VER)
    #pragma warning(push, 0)
#endif
#include <d3d11.h>
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif


/// Used to pass D3D11 eye texture data to ovrHmd_EndFrame.
typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) ovrD3D11TextureData_
{
    ovrTextureHeader          Header;           ///< General device settings.
    OVR_ON64(OVR_UNUSED_STRUCT_PAD(pad0, 4))    ///< \internal struct padding.
    ID3D11Texture2D*          pTexture;         ///< The D3D11 texture containing the undistorted eye image.
    ID3D11ShaderResourceView* pSRView;          ///< The D3D11 shader resource view for this texture.
} ovrD3D11TextureData;

#if defined(__cplusplus)
    static_assert(sizeof(ovrTexture) >= sizeof(ovrD3D11TextureData), "Insufficient size.");
    static_assert(sizeof(ovrD3D11TextureData) == sizeof(ovrTextureHeader) OVR_ON64(+ 4) + 2 * OVR_PTR_SIZE, "size mismatch");
#endif


/// Contains D3D11-specific texture information.
union ovrD3D11Texture
{
    ovrTexture          Texture;    ///< General device settings.
    ovrD3D11TextureData D3D11;      ///< D3D11-specific settings.
};


#if defined(_MSC_VER)
    #pragma warning(pop)
#endif



/// Create Texture Set suitable for use with D3D11.
///
/// \param[in]  hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in]  device Specifies the associated ID3D11Device, which must be the one that the textures will be used with in the application's process.
/// \param[in]  desc Specifies requested texture properties.
/// \param[out] outTextureSet Specifies the created ovrSwapTextureSet, which will be valid only upon a successful return value.
///             This texture set must be eventually destroyed via ovrHmd_DestroySwapTextureSet before destroying the HMD with ovrHmd_Destroy.
///
/// \return Returns an ovrResult indicating success or failure. In the case of failure, use 
///         ovr_GetLastErrorInfo to get more information.
///
/// \see ovrHmd_DestroySwapTextureSet
///
OVR_PUBLIC_FUNCTION(ovrResult) ovrHmd_CreateSwapTextureSetD3D11(ovrHmd hmd,
                                                                ID3D11Device* device,
                                                                const D3D11_TEXTURE2D_DESC* desc,
                                                                ovrSwapTextureSet** outTextureSet);

/// Create Mirror Texture which is auto-refreshed to mirror Rift contents produced by this application.
///
/// \param[in]  hmd Specifies an ovrHmd previously returned by ovrHmd_Create.
/// \param[in]  device Specifies the associated ID3D11Device, which must be the one that the textures will be used with in the application's process.
/// \param[in]  desc Specifies requested texture properties.
/// \param[out] outMirrorTexture Specifies the created ovrTexture, which will be valid only upon a successful return value.
///             This texture must be eventually destroyed via ovrHmd_DestroyMirrorTexture before destroying the HMD with ovrHmd_Destroy.
///
/// \return Returns an ovrResult indicating success or failure. In the case of failure, use 
///         ovr_GetLastErrorInfo to get more information.
///
/// \see ovrHmd_DestroyMirrorTexture
///
OVR_PUBLIC_FUNCTION(ovrResult) ovrHmd_CreateMirrorTextureD3D11(ovrHmd hmd,
                                                               ID3D11Device* device,
                                                               const D3D11_TEXTURE2D_DESC* desc,
                                                               ovrTexture** outMirrorTexture);

#endif    // OVR_CAPI_h
