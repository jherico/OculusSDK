/************************************************************************************

Filename    :   OVR_System.cpp
Content     :   General kernel initialization/cleanup, including that
                of the memory allocator.
Created     :   September 19, 2012
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

#include "OVR_SerialFormat.h"

#ifdef SERIAL_FORMAT_UNIT_TEST
#include "Kernel/OVR_Log.h"
#include "Kernel/OVR_Rand.h"
#endif


#include <cctype>
#include <ctime>

namespace OVR {


//// Serial Format Detection

SerialFormatType DetectBufferFormat(uint8_t firstByte, int sizeInBytes)
{
	switch (firstByte)
	{
	case SerialFormatType_DK2:
		if (sizeInBytes == 12)
		{
			return SerialFormatType_DK2;
		}
		break;
	default:
		break;
	}

	return SerialFormatType_Invalid;
}


//// DK2 Helpers

static bool ValidDK2ProductId(int x)
{
	switch (x)
	{
	case DK2ProductId_DK1:
	case DK2ProductId_DK2:
	case DK2ProductId_Refurb:
		return true;
	default:
		break;
	}

	return false;
}

static bool ValidDK2PartId(int x)
{
	switch (x)
	{
	case DK2PartId_HMD:
	case DK2PartId_PTC:
	case DK2PartId_Carton:
		return true;
	default:
		break;
	}

	return false;
}


//// DK2BinarySerialFormat

bool DK2BinarySerialFormat::FromBuffer(const uint8_t buffer[12], bool allowUnknownTypes)
{
	// Format Type must be 0
	
	int formatType = buffer[0];

	if (formatType != SerialFormatType_DK2)
	{
		return false;
	}

	// Product Id

	int productId = buffer[1] >> 4;

	if (!allowUnknownTypes && !ValidDK2ProductId(productId))
	{
		return false;
	}

	ProductId = (DK2ProductId)productId;

	// Part Id

	int partId = buffer[1] & 15;

	if (!allowUnknownTypes && !ValidDK2PartId(partId))
	{
		return false;
	}

	PartId = (DK2PartId)partId;

	// Minutes Since Epoch (May 1, 2014)

	MinutesSinceEpoch = buffer[4] | ((uint32_t)buffer[3] << 8) | ((uint32_t)buffer[2] << 16);

	// Unit number on that day

	UnitNumber = buffer[6] | ((uint32_t)buffer[5] << 8);

	// Hash of MAC address

	MacHash[0] = buffer[7];
	MacHash[1] = buffer[8];
	MacHash[2] = buffer[9];
	MacHash[3] = buffer[10];
	MacHash[4] = buffer[11];

	return true;
}

void DK2BinarySerialFormat::ToBuffer(uint8_t buffer[12])
{
	// Serialize to buffer
	buffer[0] = SerialFormatType_DK2;
	buffer[1] = (uint8_t)((ProductId << 4) | (PartId));
	buffer[2] = (uint8_t)(MinutesSinceEpoch >> 16);
	buffer[3] = (uint8_t)(MinutesSinceEpoch >> 8);
	buffer[4] = (uint8_t)MinutesSinceEpoch;
	buffer[5] = (uint8_t)(UnitNumber >> 8);
	buffer[6] = (uint8_t)UnitNumber;

	buffer[7] = MacHash[0];
	buffer[8] = MacHash[1];
	buffer[9] = MacHash[2];
	buffer[10] = MacHash[3];
	buffer[11] = MacHash[4];
}

bool DK2BinarySerialFormat::operator==(const DK2BinarySerialFormat& rhs)
{
	if (ProductId != rhs.ProductId)
		return false;
	if (PartId != rhs.PartId)
		return false;
	if (MinutesSinceEpoch != rhs.MinutesSinceEpoch)
		return false;
	if (UnitNumber != rhs.UnitNumber)
		return false;
	for (int ii = 0; ii < 5; ++ii)
	{
		if (MacHash[ii] != rhs.MacHash[ii])
			return false;
	}
	return true;
}


//// DK2PrintedSerialFormat

// Base-32 Crockford decoding rules:
// 0 o O => 0
// 1 i | I L l => 1
// 2, 3, 4, 5, 6, 7, 8, 9 => 2 - 9
// a, b, c, d, e, f, g, h => 10 - 17
// j, k => 18, 19
// m, n => 20, 21
// p, q, r, s, t => 22, 23, 24, 25, 26
// v, w, x, y, z => 27, 28, 29, 30, 31
static const char Base32FromChar[256] = {
	// Null - Unit Separator
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0 - 15
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 16 - 31
	// (sp)!"#$%&'()*+,-./
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 32 - 47
	// 0123456789:;<=>?
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1, // 48 - 63
	// @A-Z[\]^_
	-1, 10, 11, 12, 13, 14, 15, 16, 17,  1, 18, 19,  1, 20, 21,  0, // 64 - 79
	22, 23, 24, 25, 26, -1, 27, 28, 29, 30, 31, -1, -1, -1, -1, -1, // 80 - 95
	// `a-z{|}~DEL
	-1, 10, 11, 12, 13, 14, 15, 16, 17,  1, 18, 19,  1, 20, 21,  0, // 96 - 111
	22, 23, 24, 25, 26, -1, 27, 28, 29, 30, 31, -1,  1, -1, -1, -1, // 112 - 127

	// Extended ASCII:
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,

	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,

	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,

	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

// Base-32 Crockford encoding rules:
// 0-9 => 0-9
// 10 - 17 => a, b, c, d, e, f, g, h
// 18, 19 => j, k
// 20, 21 => m, n
// 22, 23, 24, 25, 26 => p, q, r, s, t
// 27, 28, 29, 30, 31 => v, w, x, y, z
static const char* CharFromBase32 = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

bool DK2PrintedSerialFormat::FromBase32(const char* str, bool allowUnknownTypes)
{
	// Note: Truncated strings get caught by returning negative values from the table like other invalid characters

	// Product Id

	int productId = Base32FromChar[(unsigned char)str[0]];
	if (productId < 0 || (!allowUnknownTypes && !ValidDK2ProductId(productId)))
	{
		return false;
	}

	ProductId = (DK2ProductId)productId;

	// Label Type

	int labelType = Base32FromChar[(unsigned char)str[1]];
	if (labelType < 0 || (!allowUnknownTypes && !ValidDK2PartId(labelType)))
	{
		return false;
	}

	LabelType = (DK2LabelType)labelType;

	uint8_t dataBytes[7];
	for (int ii = 0; ii < 7; ++ii)
	{
		int c = Base32FromChar[(unsigned char)str[2 + ii]];
		if (c < 0) return false;
		dataBytes[ii] = (uint8_t)c;
	}

	// Minutes Since Epoch

	MinutesSinceEpoch = dataBytes[3] | ((uint32_t)dataBytes[2] << 5) | ((uint32_t)dataBytes[1] << 10) | ((uint32_t)dataBytes[0] << 15);

	// Unit Number

	UnitNumber = dataBytes[6] | ((uint32_t)dataBytes[5] << 5) | ((uint32_t)dataBytes[4] << 10);

	// MAC Hash

	for (int ii = 0; ii < 3; ++ii)
	{
		int c = Base32FromChar[(unsigned char)str[9 + ii]];
		if (c < 0)
		{
			return false;
		}

		MacHashLow[ii] = (uint8_t)c;
	}

	// String must be exactly 12 characters
	if (str[12] != '\0')
	{
		return false;
	}

	return true;
}

String DK2PrintedSerialFormat::ToBase32()
{
	String s;

	s += CharFromBase32[ProductId];
	s += CharFromBase32[LabelType];
	s += CharFromBase32[(MinutesSinceEpoch >> 15) & 31];
	s += CharFromBase32[(MinutesSinceEpoch >> 10) & 31];
	s += CharFromBase32[(MinutesSinceEpoch >> 5) & 31];
	s += CharFromBase32[MinutesSinceEpoch & 31];
	s += CharFromBase32[(UnitNumber >> 10) & 31];
	s += CharFromBase32[(UnitNumber >> 5) & 31];
	s += CharFromBase32[UnitNumber & 31];
	s += CharFromBase32[MacHashLow[0] & 31];
	s += CharFromBase32[MacHashLow[1] & 31];
	s += CharFromBase32[MacHashLow[2] & 31];

	return s;
}

bool DK2PrintedSerialFormat::operator==(const DK2PrintedSerialFormat& rhs)
{
	if (ProductId != rhs.ProductId)
		return false;
	if (LabelType != rhs.LabelType)
		return false;
	if (MinutesSinceEpoch != rhs.MinutesSinceEpoch)
		return false;
	if (UnitNumber != rhs.UnitNumber)
		return false;
	for (int ii = 0; ii < 3; ++ii)
	{
		if (MacHashLow[ii] != rhs.MacHashLow[ii])
			return false;
	}
	return true;
}

bool DK2PrintedSerialFormat::operator==(const DK2BinarySerialFormat& rhs)
{
	if (ProductId != rhs.ProductId)
		return false;
	if (LabelType != rhs.PartId)
		return false;
	if (MinutesSinceEpoch != rhs.MinutesSinceEpoch)
		return false;
	if (UnitNumber != rhs.UnitNumber)
		return false;
	for (int ii = 0; ii < 3; ++ii)
	{
		if (MacHashLow[ii] != (rhs.MacHash[ii] & 31))
			return false;
	}
	return true;
}

void DK2PrintedSerialFormat::FromBinary(const DK2BinarySerialFormat& bin)
{
	ProductId = bin.ProductId;
	LabelType = bin.PartId;
	MinutesSinceEpoch = bin.MinutesSinceEpoch;
	UnitNumber = bin.UnitNumber;
	MacHashLow[0] = bin.MacHash[0] & 31;
	MacHashLow[1] = bin.MacHash[1] & 31;
	MacHashLow[2] = bin.MacHash[2] & 31;
}




} // OVR
