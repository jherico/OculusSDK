/************************************************************************************

Filename    :   Render_LoadeTextureTGA.cpp
Content     :   Loading of TGA implementation
Created     :   October, 2012
Authors     :   Andrew Reisse

Copyright   :   Copyright 2012 Oculus VR, LLC. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/


#include "Render_Device.h"

namespace OVR { namespace Render {

Texture* LoadTextureTgaEitherWay(RenderDevice* ren, File* f, int textureLoadFlags, unsigned char alpha, bool bottomUp)
{
    OVR_ASSERT(textureLoadFlags != 255); // probably means an older style call is being made

    bool srgbAware = (textureLoadFlags & TextureLoad_SrgbAware) != 0;
    bool anisotropic = (textureLoadFlags & TextureLoad_Anisotropic) != 0;
    bool generatePremultAlpha = (textureLoadFlags & TextureLoad_MakePremultAlpha) != 0;
    bool createSwapTextureSet = (textureLoadFlags & TextureLoad_SwapTextureSet) != 0;
    bool isHdcp = (textureLoadFlags & TextureLoad_Hdcp) != 0;

    f->SeekToBegin();

    if ( f->GetLength() == 0 )
    {
        // File doesn't exist!
        return NULL;
    }
    
    int desclen = f->ReadUByte();
    int palette = f->ReadUByte();
    OVR_UNUSED(palette);
    int imgtype = f->ReadUByte();
    f->ReadUInt16();
    int palCount = f->ReadUInt16();
    int palSize = f->ReadUByte();
    f->ReadUInt16();
    f->ReadUInt16();
    int width = f->ReadUInt16();
    int height = f->ReadUInt16();
    int bpp = f->ReadUByte();
    int descbyte = f->ReadUByte();
    int imgsize = width * height * 4;
    unsigned char* imgdata = (unsigned char*) OVR_ALLOC(imgsize);
    unsigned char buf[16];
    f->Read(imgdata, desclen);
    f->Read(imgdata, palCount * (palSize + 7) >> 3);
    int bpl = width * 4;

    // From the interwebs (very reliable I'm sure):
    //
    // Image Descriptor Byte.                                   
    // Bits 3-0 - number of attribute bits associated with each 
    //            pixel.                                        
    // Bit 4    - reserved.  Must be set to 0.                  
    // Bit 5    - screen origin bit.                            
    //            0 = Origin in lower left-hand corner.         
    //            1 = Origin in upper left-hand corner.         
    //            Must be 0 for Truevision images.              
    // Bits 7-6 - Data storage interleaving flag.               
    //            00 = non-interleaved.                         
    //            01 = two-way (even/odd) interleaving.         
    //            10 = four way interleaving.                   
    //            11 = reserved.                                
    // This entire byte should be set to 0
    OVR_ASSERT ( ( ( bpp == 24 ) && ( descbyte == 0 ) ) || ( ( bpp == 32 ) && ( descbyte == 8 ) ) );
    if ( ( descbyte & 0x10 ) != 0 )
    {
        // TGA is stored top-down rather than bottom-up, so flip the way we read the data to cope.
        OVR_ASSERT ( !"test me" );
        bottomUp = !bottomUp;
    }

    switch (imgtype)
    {
    case 2:
        switch (bpp)
        {
        case 24:
            for (int yc = height-1; yc >= 0; yc--)
            {
                int y = yc;
                if ( bottomUp )
                {
                    y = (height-1) - y;
                }
                for (int x = 0; x < width; x++)
                {
                    f->Read(buf, 3);
                    imgdata[y*bpl+x*4+0] = buf[2];
                    imgdata[y*bpl+x*4+1] = buf[1];
                    imgdata[y*bpl+x*4+2] = buf[0];
                    imgdata[y*bpl+x*4+3] = alpha;
                }
            }
            break;
        case 32:
            for (int yc = height-1; yc >= 0; yc--)
            {
                int y = yc;
                if ( bottomUp )
                {
                    y = (height-1) - y;
                }
                for (int x = 0; x < width; x++)
                {
                    f->Read(buf, 4);
                    imgdata[y*bpl+x*4+0] = buf[2];
                    imgdata[y*bpl+x*4+1] = buf[1];
                    imgdata[y*bpl+x*4+2] = buf[0];
                    if (buf[3] == 255)
                    {
                        imgdata[y*bpl+x*4+3] = alpha;
                    }
                    else
                    {
                        imgdata[y*bpl+x*4+3] = buf[3];
                    }
                    if ( generatePremultAlpha )
                    {
                        // Image is in lerping alpha, but we want premult alpha.
                        imgdata[y*bpl+x*4+0] = (unsigned char)((float)buf[2] * (float)buf[3] / 255.0f);
                        imgdata[y*bpl+x*4+1] = (unsigned char)((float)buf[1] * (float)buf[3] / 255.0f);
                        imgdata[y*bpl+x*4+2] = (unsigned char)((float)buf[0] * (float)buf[3] / 255.0f);
                    }
                }
            }
            break;

        default:
            OVR_ASSERT ( !"Unknown bits per pixel" );
            OVR_FREE(imgdata);
            return NULL;
        }
        break;

    default:
        OVR_ASSERT ( !"unknown file format" );
        OVR_FREE(imgdata);
        return NULL;
    }

    int format = Texture_RGBA|Texture_GenMipmaps;
    if (createSwapTextureSet)
    {
        format |= Texture_SwapTextureSetStatic;
    }
    if(isHdcp)
    {
        format |= Texture_Hdcp;
    }

    // TODO: This should just be a suggestion. In theory we should use a property set by the texture to actually
    // create an sRGB texture, and not do it all the time
    if (srgbAware)
    {
        format |= Texture_SRGB;
    }

    Texture* out = ren->CreateTexture(format, width, height, imgdata);
    if (!out)
    {
        return NULL;
    }

    // Commit static image immediately since we're done rendering to it.
    out->Commit();

    // check for clamp based on texture name
    if(strstr(f->GetFilePath(), "_c."))
    {
        out->SetSampleMode(Sample_Clamp | (anisotropic ? Sample_Anisotropic : 0));
    }
    else if(anisotropic)
    {
        out->SetSampleMode((anisotropic ? Sample_Anisotropic : 0));
    }

    OVR_FREE(imgdata);
    return out;
}

Texture* LoadTextureTgaTopDown(RenderDevice* ren, File* f, int textureLoadFlags, unsigned char alpha)
{
    return LoadTextureTgaEitherWay(ren, f, textureLoadFlags, alpha, false);
}

Texture* LoadTextureTgaBottomUp(RenderDevice* ren, File* f, int textureLoadFlags, unsigned char alpha)
{
    return LoadTextureTgaEitherWay(ren, f, textureLoadFlags, alpha, true);
}


}}
