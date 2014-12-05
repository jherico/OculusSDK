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

namespace OVR { namespace Render {

static const size_t   OVR_DDS_PF_FOURCC = 0x4;
static const uint32_t OVR_DXT1_MAGIC_NUMBER = 0x31545844; // "DXT1"
static const uint32_t OVR_DXT2_MAGIC_NUMBER = 0x32545844; // "DXT2"
static const uint32_t OVR_DXT3_MAGIC_NUMBER = 0x33545844; // "DXT3"
static const uint32_t OVR_DXT4_MAGIC_NUMBER = 0x34545844; // "DXT4"
static const uint32_t OVR_DXT5_MAGIC_NUMBER = 0x35545844; // "DXT5"

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

// Returns -1 on failure, or a valid TextureFormat value on success
static inline int InterpretPixelFormatFourCC(uint32_t fourCC) {
	switch (fourCC) {
	case OVR_DXT1_MAGIC_NUMBER: return Texture_DXT1;
	case OVR_DXT2_MAGIC_NUMBER: return Texture_DXT3;
	case OVR_DXT3_MAGIC_NUMBER: return Texture_DXT3;
	case OVR_DXT4_MAGIC_NUMBER: return Texture_DXT5;
	case OVR_DXT5_MAGIC_NUMBER: return Texture_DXT5;
	}

	// Unrecognized FourCC
	return -1;
}

Texture* LoadTextureDDS(RenderDevice* ren, File* f)
{
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
    if(header.PixelFormat.Flags & OVR_DDS_PF_FOURCC)
    {
		format = InterpretPixelFormatFourCC(header.PixelFormat.FourCC);
		if (format == -1) {
			return NULL;
		}
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
        out->SetSampleMode(Sample_Clamp);
    }
    OVR_FREE(bytes);
    return out;
}


}}

#ifdef OVR_DEFINE_NEW
#define new OVR_DEFINE_NEW
#endif