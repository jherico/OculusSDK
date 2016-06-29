/************************************************************************************

Filename    :   WavPlayer_OSX.h
Content     :   A DDS file loader for cross-platform compressed texture support.
Created     :   March 5, 2013
Authors     :   Peter Hoff, Dan Goodman, Bryan Croteau

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

#include <dxgi.h>

namespace OVR { namespace Render {

static const size_t   OVR_DDS_PF_FOURCC = 0x4;
static const uint32_t OVR_DXT1_MAGIC_NUMBER = 0x31545844; // "DXT1"
static const uint32_t OVR_DXT2_MAGIC_NUMBER = 0x32545844; // "DXT2"
static const uint32_t OVR_DXT3_MAGIC_NUMBER = 0x33545844; // "DXT3"
static const uint32_t OVR_DXT4_MAGIC_NUMBER = 0x34545844; // "DXT4"
static const uint32_t OVR_DXT5_MAGIC_NUMBER = 0x35545844; // "DXT5"
static const uint32_t OVR_DX10_MAGIC_NUMBER = 0x30315844; // "DX10" - Means use the extended header

struct OVR_DDS_PIXELFORMAT
{
    uint32_t Size;
    uint32_t Flags;
    uint32_t FourCC;
    uint32_t RGBBitCount;
    uint32_t RBitMask;
    uint32_t GBitMask;
    uint32_t BBitMask;
    uint32_t ABitMask;
};

struct OVR_DDS_HEADER
{
    uint32_t				Size;
    uint32_t				Flags;
    uint32_t				Height;
    uint32_t				Width;
    uint32_t				PitchOrLinearSize;
    uint32_t				Depth;
    uint32_t				MipMapCount;
    uint32_t				Reserved1[11];
    OVR_DDS_PIXELFORMAT     PixelFormat;
    uint32_t				Caps;
    uint32_t				Caps2;
    uint32_t				Caps3;
    uint32_t				Caps4;
    uint32_t				Reserved2;
};

struct OVR_DDS_HEADER_DXT10
{
    DXGI_FORMAT dxgiFormat;
    uint32_t    resourceDimension;
    uint32_t    miscFlag; // see DDS_RESOURCE_MISC_FLAG
    uint32_t    arraySize;
    uint32_t    miscFlags2; // see DDS_MISC_FLAGS2
};

struct OVR_FULL_DDS_HEADER
{
    OVR_DDS_HEADER          DX9Header;
    OVR_DDS_HEADER_DXT10    DX10Header;
};


// Returns -1 on failure, or a valid TextureFormat value on success
static inline int InterpretPixelFormatFourCC(uint32_t fourCC) {
	switch (fourCC) {
	case OVR_DXT1_MAGIC_NUMBER: return Texture_BC1;
	case OVR_DXT2_MAGIC_NUMBER: return Texture_BC2;
	case OVR_DXT3_MAGIC_NUMBER: return Texture_BC2;
	case OVR_DXT4_MAGIC_NUMBER: return Texture_BC3;
	case OVR_DXT5_MAGIC_NUMBER: return Texture_BC3;
	}

	// Unrecognized FourCC
	return -1;
}

Texture* LoadTextureDDSTopDown(RenderDevice* ren, File* f, int textureLoadFlags)
{
    bool srgbAware = (textureLoadFlags & TextureLoad_SrgbAware) != 0;
    bool anisotropic = (textureLoadFlags & TextureLoad_Anisotropic) != 0;

    OVR_DDS_HEADER header;
    unsigned char filecode[4];

    f->Read(filecode, 4);
    if (strncmp((const char*)filecode, "DDS ", 4) != 0)
    {
        return NULL;
    }

    f->Read((unsigned char*)(&header), sizeof(header));

    int width  = header.Width;
    int height = header.Height;

    int format = Texture_RGBA;

    uint32_t mipCount = header.MipMapCount;
    if(mipCount <= 0)
    {
        mipCount = 1;
    }
    if (header.PixelFormat.Flags & OVR_DDS_PF_FOURCC)
    {
        if (header.PixelFormat.FourCC == OVR_DX10_MAGIC_NUMBER)
        {
            OVR_DDS_HEADER_DXT10 dx10Header;
            f->Read((unsigned char*)(&dx10Header), sizeof(dx10Header));

            switch (dx10Header.dxgiFormat)
            {
                case DXGI_FORMAT_BC1_UNORM:
                case DXGI_FORMAT_BC1_UNORM_SRGB:
                    format = Texture_BC1;
                    break;
                case DXGI_FORMAT_BC2_UNORM:
                case DXGI_FORMAT_BC2_UNORM_SRGB:
                    format = Texture_BC2;
                    break;
                case DXGI_FORMAT_BC3_UNORM:
                case DXGI_FORMAT_BC3_UNORM_SRGB:
                    format = Texture_BC3;
                    break;
                case DXGI_FORMAT_BC6H_SF16:
                    format = Texture_BC6S;
                    break;
                case DXGI_FORMAT_BC6H_UF16:
                    format = Texture_BC6U;
                    break;
                case DXGI_FORMAT_BC7_UNORM:
                case DXGI_FORMAT_BC7_UNORM_SRGB:
                    format = Texture_BC7;
                    break;
                default:
                    OVR_ASSERT(false);
                    // Add more formats and you encounter dds files that need them
                    break;
            }
        }
        else
        {
            format = InterpretPixelFormatFourCC(header.PixelFormat.FourCC);
            if (format == -1) {
                return NULL;
            }
        }
    }

    // TODO: Should not blindly add srgb as a format flag, and instead should rely on some data driver flag per-texture
    // The problem is that currently we do not have a way to data drive such a flag
    if (srgbAware)
    {
        format |= Texture_SRGB;
    }

    if (textureLoadFlags & TextureLoad_SwapTextureSet)
    {
        format |= Texture_SwapTextureSetStatic;
    }

    int            byteLen = f->BytesAvailable();
    unsigned char* bytes   = new unsigned char[byteLen];
    f->Read(bytes, byteLen);
    Texture* out = ren->CreateTexture(format, (int)width, (int)height, bytes, mipCount);
	if (!out) {
		return NULL;
	}

    if(strstr(f->GetFilePath(), "_c."))
    {
        out->SetSampleMode(Sample_Clamp | (anisotropic ? Sample_Anisotropic : 0));
    }
    else
    {
        out->SetSampleMode((anisotropic ? Sample_Anisotropic : 0));
    }

    delete[] bytes;

    return out;
}


}}

#ifdef OVR_DEFINE_NEW
#define new OVR_DEFINE_NEW
#endif