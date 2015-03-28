/************************************************************************************

Filename    :   Util_MatFile.cpp
Content     :   Matlab .MAT file access functions
Created     :   June 1, 2014
Authors     :   Neil Konzen

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.

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

*************************************************************************************/

#include "Util_MatFile.h"

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_Alg.h"
#include "Kernel/OVR_Std.h"

OVR_DISABLE_MSVC_WARNING(4996) //  'fopen': This function or variable may be unsafe. Consider using fopen_s inst


namespace OVR { namespace Util {

using namespace OVR::Alg;


// Data structures relating to MATLAB .MAT binary files
#define	FX_FORM_IEEE_LE		0000u
#define FX_FORM_IEEE_BE		1000u
#define FX_FORM_VAX_D_FLOAT 2000u
#define FX_FORM_VAX_G_FLOAT 3000u
#define FX_FORM_CRAY		4000u
#define FX_FORM(type)		((((type) / 1000u) % 10u) * 1000u)

#define FX_PREC_UINT8 		50u
#define FX_PREC_INTU		40u
#define FX_PREC_INTS		30u
#define FX_PREC_LONG		20u
#define FX_PREC_SINGLE		10u
#define FX_PREC_DOUBLE		00u
#define FX_PREC(type)		((((type) / 10u) % 10u) * 10u)

// Note that the elements of a text matrix are stored as floating-point numbers
// between 0 and 255 representing ASCII-encoded characters.
#define FX_MAT_NUMERIC	   0u
#define FX_MAT_TEXT		   1u
#define FX_MAT_SPARSE	   2u
#define FX_MAT(type)		((type) % 10u)

struct Fmatrix
{
	uint32_t type;	    //	Type - see #defines
	uint32_t mrows;     //	Row dimension - NOTE: Column dimension for C Arrays!
	uint32_t ncols;	    // 	Column dimension - NOTE: Row dimension for C Arrays!
	uint32_t imagf;	    //	1=complex, 0=real
	uint32_t namelen;	// 	length including zero terminator
};


uint32_t MatFile::GetMatlabType(ValueType type, size_t& valueSize)
{
    switch (type)
    {
    case ByteValue: valueSize = sizeof(uint8_t); return FX_PREC_UINT8;
    case UInt16Value: valueSize = sizeof(uint16_t); return FX_PREC_INTU;
    case Int16Value: valueSize = sizeof(int16_t); return FX_PREC_INTS;
    case UInt32Value: valueSize = sizeof(uint32_t); return FX_PREC_LONG;    // Not directly supported by matlab!
    case Int32Value: valueSize = sizeof(int32_t); return FX_PREC_LONG;
    case FloatValue: valueSize = sizeof(float); return FX_PREC_SINGLE;
    case DoubleValue: valueSize = sizeof(double); return FX_PREC_DOUBLE;
    case StringValue: valueSize = sizeof(char); return FX_MAT_TEXT; // special case for string arrays
    default:
        OVR_ASSERT(false);
        valueSize = 0;
        return 0;
    }
}

MatFile::ValueType MatFile::GetValueType(uint32_t matlabType, size_t& valueSize)
{
    switch (matlabType)
    {
    case FX_PREC_UINT8: valueSize = sizeof(uint8_t); return ByteValue;
    case FX_PREC_INTU: valueSize = sizeof(uint16_t); return UInt16Value;
    case FX_PREC_INTS: valueSize = sizeof(int16_t); return Int16Value;
    case FX_PREC_LONG: valueSize = sizeof(int32_t); return Int32Value;
    case FX_PREC_SINGLE: valueSize = sizeof(float); return FloatValue;
    case FX_PREC_DOUBLE: valueSize = sizeof(double); return DoubleValue;
    case FX_MAT_TEXT: valueSize = sizeof(char); return StringValue;
    default:
        OVR_ASSERT(false);
        valueSize = 0;
        return UnknownValue;
    }
}

MatFile::MatFile(void)
{
	m_f = NULL;
}

MatFile::~MatFile(void)
{
	if (m_f)
		fclose(m_f);
	m_f = NULL;
}

// Matlab arrays are stored column-major, while C/C++ arrays are stored row-major.
// This means that a C array appears to Matlab transposed, and vice versa.
// To deal with this we swap the row and column values stored in the Matlab matrix header.

bool MatFile::Open(const char* pszFile, bool write)
{
    OVR_ASSERT(!m_f);
    m_f = fopen(pszFile, write ? "wb" : "rb");
    return (m_f != nullptr);
}

void MatFile::Close()
{
	if (m_f)
	{
		fclose(m_f);
		m_f = NULL;
	}
}

int MatFile::ReadString(const char* name, char* text, size_t maxTextSize)
{
	int rows, cols;
	ValueType valueType;

	maxTextSize = Alg::Min(maxTextSize, INT_MAX/sizeof(double)/2);

	if (!GetMatrixInfo(name, valueType, rows, cols))
		return 0;

	if (valueType != StringValue)
		return 0;

	int count = rows * cols;	// character count, not including zero terminator

	double* doubles = new double[count];
	ReadMatrixValues(doubles, StringValue, count, 1);

	if (maxTextSize > 0 && count > 0)
	{
		count = (int)Alg::Min(count, (int)(maxTextSize-1));
		for (int i = 0; i < count; i++)
			text[i] = (char)doubles[i];
		text[count] = 0;	// Always zero terminate
	}

	delete[] doubles;

	return count;
}

bool MatFile::WriteString(const char* name, const char* string)
{
	int length = (int)Alg::Min(strlen(string), INT_MAX/sizeof(double)/2);

	double* doubles = new double[length];
	for (int i = 0; i < length; i++)
		doubles[i] = (double)((unsigned char)string[i]);

	bool ok = WriteMatrix(name, doubles, StringValue, (int)length, 1);

	delete[] doubles;

	return ok;
}

void* MatFile::ReadMatrix(const char* name, ValueType valueType, int& rows, int& cols)
{
	ValueType fileValueType;
	if (!GetMatrixInfo(name, fileValueType, rows, cols))
		return NULL;

	int valueCount = rows * cols;

	void* values = NULL;
	switch (fileValueType)
	{
	case StringValue:	// Text matrices are stored as doubles
	case DoubleValue:
		values = new double[valueCount];
		break;
	case FloatValue:
		values = new float[valueCount];
		break;
	case ByteValue:
		values = new uint8_t[valueCount];
		break;
	case Int16Value:
		values = new int16_t[valueCount];
		break;
	case UInt16Value:
		values = new uint16_t[valueCount];
		break;
	case Int32Value:
    /*case UInt32Value: -- not directly supported by matlab -v4 files */
        values = new int32_t[valueCount];
		break;
	default:
        OVR_ASSERT(false);
		return NULL;
	}

	bool ok = ReadMatrixValues(values, fileValueType, rows, cols);

	if (ok)
		values = ConvertVector(values, valueCount, fileValueType, valueType);

	if (!ok)
	{
		delete[] (char*)values;
		values = NULL;
	}

    OVR_ASSERT(values);
	return values;
}

void* MatFile::ConvertVector(void* fromValues, int valueCount, ValueType fromType, ValueType toType)
{
	// Special case: Always convert characters stored as doubles to a char array
	if (fromType == StringValue)
		fromType = DoubleValue;

	if (fromType == toType)
		return fromValues;

    // UInt32 values are stored as Int32 values by Matlab
    if (fromType == Int32Value && toType == UInt32Value)
        return fromValues;

    // When a .mat file is saved by Matlab, many datatypes are converted to double.
	// We support conversion of doubles to some other types: float, long, byte, char
	// and strings and floats to doubles.
	// convert singles to doubles
	bool ok = true;
	if (fromType == DoubleValue)
	{
        const double* fromDoubles = (const double*)fromValues;
		if (toType == FloatValue)
		{
			float* newValues = new float[valueCount];
			for (int i = 0; i < valueCount; i++)
				newValues[i] = (float)fromDoubles[i];
			delete[] (char*)fromValues;
			fromValues = newValues;
		}
		else if (toType == Int32Value)
		{
			int32_t* newValues = new int32_t[valueCount];
			for (int i = 0; i < valueCount; i++)
				newValues[i] = (int32_t)fromDoubles[i];
			delete[] (char*)fromValues;
			fromValues = newValues;
		}
        else if (toType == UInt32Value)
        {
            uint32_t* newValues = new uint32_t[valueCount];
            for (int i = 0; i < valueCount; i++)
                newValues[i] = (uint32_t)fromDoubles[i];
            delete[] (char*)fromValues;
            fromValues = newValues;
        }
		else if (toType == Int16Value)
		{
			int16_t* newValues = new int16_t[valueCount];
			for (int i = 0; i < valueCount; i++)
				newValues[i] = (int16_t)fromDoubles[i];
			delete[] (char*)fromValues;
			fromValues = newValues;
		}
		else if (toType == UInt16Value)
		{
			uint16_t* newValues = new uint16_t[valueCount];
			for (int i = 0; i < valueCount; i++)
				newValues[i] = (uint16_t)fromDoubles[i];
			delete[] (char*)fromValues;
			fromValues = newValues;
		}
		else if (toType == ByteValue)
		{
			uint8_t* newValues = new uint8_t[valueCount];
			for (int i = 0; i < valueCount; i++)
				newValues[i] = (uint8_t)fromDoubles[i];
			delete[] (char*)fromValues;
			fromValues = newValues;
		}
		else if (toType == StringValue)
		{
			char* newValues = new char[valueCount];
			for (int i = 0; i < valueCount; i++)
				newValues[i] = (char)fromDoubles[i];
			delete[] (char*)fromValues;
			fromValues = newValues;
		}
		else
		{
			// unsupported type conversion
			ok = false;
		}
	}
	else
	{
		ok = false;	// only conversions from doubles supported
	}

	if (!ok)
	{
        OVR_ASSERT(false);
		delete[] (char*)fromValues;
		fromValues = NULL;
	}

	return fromValues;
}

bool MatFile::GetMatrixInfo(const char* name, ValueType& valueType, int& rows, int& cols)
{
    OVR_ASSERT(m_f);
	fseek(m_f, 0, SEEK_SET);	// rewind to start of file
	
	static const int maxVarNameLen = 255;
	char varName[maxVarNameLen+1];

	while (ReadMatrixInfo(varName, maxVarNameLen, valueType, rows, cols))
	{
		if (OVR_stricmp(name, varName) == 0)
			return true;
		// skip over data to next one
		ReadMatrixValues(NULL, valueType, rows, cols);
	}

	return false;
}

bool MatFile::ReadMatrixInfo(char name[], size_t maxNameSize, ValueType& valueType, int& rows, int& cols)
{
	if (name && maxNameSize > 0)
		name[0] = 0;

	valueType = UnknownValue;
	rows = 0;
	cols = 0;

    OVR_ASSERT(m_f);
	if (!m_f)
		return false;

	Fmatrix header;
	if (fread(&header, sizeof(header), 1, m_f) != 1)
		return false;

	// Read transpose of row and column values stored in the file
	cols = header.mrows;
	rows = header.ncols;

	if (FX_FORM(header.type) != FX_FORM_IEEE_LE)
	{
        OVR_ASSERT(false);
		return false;
	}

	// Imaginary not supported
	if (header.imagf != 0)
	{
        OVR_ASSERT(false);
		return false;
	}

	// sparse matrices not supported
	if (FX_MAT(header.type) == FX_MAT_SPARSE)
	{
        OVR_ASSERT(false);
		return false;
	}

	// Special case for strings as text matrixes: they are stored as doubles(!)
	if (FX_MAT(header.type) == FX_MAT_TEXT)
	{
		valueType = StringValue;
	}
	else
	{
        // only numeric types supported
        if (FX_MAT(header.type) != FX_MAT_NUMERIC)
        {
            OVR_ASSERT(false);
            return false;
        }
        size_t valueSize;
        valueType = GetValueType(FX_PREC(header.type), valueSize);
	}

	// Read in name
    OVR_ASSERT(maxNameSize >= header.namelen);
	if (maxNameSize < header.namelen)
		return false;

	if (fread(name, sizeof(char), header.namelen, m_f) != header.namelen)
		return false;

	return true;
}

bool MatFile::ReadMatrixValues(void* values, ValueType valueType, int rows, int cols)
{
    OVR_ASSERT(m_f);
	if (!m_f)
		return false;

    OVR_ASSERT(rows*cols > 0);

	size_t valueCount = (size_t)(rows * cols);
    size_t valueSize = 0;
    GetMatlabType(valueType, valueSize);
    if (valueSize == 0)
        return false;

    // If no values pointer specified, skip over data without reading
	if (!values)
	{
		if (fseek(m_f, (long)(valueSize * valueCount), SEEK_CUR) != 0)
			return false;
	}
	else
	{
		if (fread(values, valueSize, valueCount, m_f) != valueCount)
			return false;
	}

    return true;
}

bool MatFile::WriteMatrix(const char* name, const void* values, ValueType valueType, int rows, int cols)
{
	if (!m_f)
		return false;

    OVR_ASSERT(rows*cols > 0);

	size_t valueCount = (size_t)(rows * cols);
	size_t valueSize = 0;
    uint32_t matlabType = GetMatlabType(valueType, valueSize);
    if (valueSize == 0)
        return false;

	Fmatrix header;
	if (valueType == StringValue)
	{
		header.type = (FX_FORM_IEEE_LE + FX_MAT_TEXT);
	}
	else
	{
		header.type = (FX_FORM_IEEE_LE + FX_MAT_NUMERIC) + matlabType;
	}

	// NOTE: We store transposed dimensions!
	header.mrows = cols;
	header.ncols = rows;
	header.imagf = 0;
	header.namelen = (uint32_t)(strlen(name) + 1);
    OVR_ASSERT(header.namelen > 1);

	if (fwrite(&header, sizeof(header), 1, m_f) != 1)
		return false;
	if (fwrite(name, sizeof(char), header.namelen, m_f) != header.namelen)
		return false;
	if (fwrite(values, valueSize, valueCount, m_f) != valueCount)
		return false;

    return true;
}


}} // namespace OVR::Util
