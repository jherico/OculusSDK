/************************************************************************************

Filename    :   CAPI_DistortionRenderer.cpp
Content     :   Combines all of the rendering state associated with the HMD
Created     :   February 2, 2014
Authors     :   Michael Antonov

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.1 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.1 

Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#include "CAPI_DistortionRenderer.h"

#if defined (OVR_OS_WIN32)

// TBD: Move to separate config file that handles back-ends.
#define OVR_D3D_VERSION 11
#include "D3D1X/CAPI_D3D1X_DistortionRenderer.h"
#undef OVR_D3D_VERSION

#define OVR_D3D_VERSION 10
#include "D3D1X/CAPI_D3D1X_DistortionRenderer.h"
#undef OVR_D3D_VERSION

#define OVR_D3D_VERSION 9
#include "D3D1X/CAPI_D3D9_DistortionRenderer.h"
#undef OVR_D3D_VERSION

#endif

#include "GL/CAPI_GL_DistortionRenderer.h"

namespace OVR { namespace CAPI {

//-------------------------------------------------------------------------------------
// ***** DistortionRenderer

// TBD: Move to separate config file that handles back-ends.

DistortionRenderer::CreateFunc DistortionRenderer::APICreateRegistry[ovrRenderAPI_Count] =
{
    0, // None
    &GL::DistortionRenderer::Create,
    0, // Android_GLES
#if defined (OVR_OS_WIN32)
    &D3D9::DistortionRenderer::Create,
    &D3D10::DistortionRenderer::Create,
    &D3D11::DistortionRenderer::Create
#else
    0,
    0,
    0
#endif
};


}} // namespace OVR::CAPI

