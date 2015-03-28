/************************************************************************************

PublicHeader:   OVR_Kernel.h
Filename    :   OVR_Color.h
Content     :   Contains color struct.
Created     :   February 7, 2013
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
#ifndef OVR_Color_h
#define OVR_Color_h

#include "OVR_Types.h"

namespace OVR {


struct Color
{
    uint8_t R,G,B,A;

    Color()
    {
        #if defined(OVR_BUILD_DEBUG)
            R = G = B = A = 0;
        #endif
    }

    // Constructs color by channel. Alpha is set to 0xFF (fully visible)
    // if not specified.
    Color(unsigned char r,unsigned char g,unsigned char b, unsigned char a = 0xFF)
        : R(r), G(g), B(b), A(a) { }

    // 0xAARRGGBB - Common HTML color Hex layout
    Color(unsigned c)
        : R((unsigned char)(c>>16)), G((unsigned char)(c>>8)),
        B((unsigned char)c), A((unsigned char)(c>>24)) { }

    bool operator==(const Color& b) const
    {
        return R == b.R && G == b.G && B == b.B && A == b.A;
    }

    void  GetRGBA(float *r, float *g, float *b, float* a) const
    {
        *r = R / 255.0f;
        *g = G / 255.0f;
        *b = B / 255.0f;
        *a = A / 255.0f;
    }
};


} // namespace OVR

#endif
