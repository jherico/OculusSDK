/************************************************************************************

Filename    :   Util_MatFile.h
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

#ifndef OVR_Util_MatFile_h
#define OVR_Util_MatFile_h

#include "Kernel/OVR_Types.h"

#include <stdint.h>
#include <stdio.h>

// Read and write MatLab .MAT data files, in MatLab Version 4 format
namespace OVR { namespace Util {


class MatFile
{
public:
	MatFile();
	~MatFile(void);

	bool Open(const char* pszFile, bool write);
	void Close();

	// Matrix element value types
    enum ValueType {
        UnknownValue = 0,
        ByteValue = 1,
        UInt16Value = 2,
        Int16Value = 3,
        UInt32Value = 4,    // NOTE: Matlab -v4 don't support UInt32 directly: values stored as Int32.
        Int32Value = 5,
        FloatValue = 6,
        DoubleValue = 7,
        StringValue = 8
    };

	// NOTE: A stored matrix is TRANSPOSED when read into MatLab
    // MatLab uses Fortran column-major matrix storage conventions

	// Write a matrix, organized as rows x columns
	// Vectors should be written with 1 column: they will appear in Matlab as a 1-row matrix.
	// NOTE: this function reads StringValue matrices as an array of doubles (!)
	bool WriteMatrix(const char* name, const void* values, ValueType valueType, int rows, int cols);

	bool WriteMatrix(const char* name, const uint8_t* values, int rows, int cols = 1) { return WriteMatrix(name, values, ByteValue, rows, cols); }
	bool WriteMatrix(const char* name, const uint16_t* values, int rows, int cols = 1){ return WriteMatrix(name, values, UInt16Value, rows, cols); }
	bool WriteMatrix(const char* name, const int16_t* values, int rows, int cols = 1) { return WriteMatrix(name, values, Int16Value, rows, cols); }
	bool WriteMatrix(const char* name, const int32_t* values, int rows, int cols = 1) { return WriteMatrix(name, values, Int32Value, rows, cols); }
	bool WriteMatrix(const char* name, const float* values, int rows, int cols = 1)   { return WriteMatrix(name, values, FloatValue, rows, cols); }
	bool WriteMatrix(const char* name, const double* values, int rows, int cols = 1)  { return WriteMatrix(name, values, DoubleValue, rows, cols); }
	bool WriteString(const char* name, const char* value);

    // NOTE: Matlab doesn't directly support uint32_t type: these values are saved and loaded as Int32Values
    bool WriteMatrix(const char* name, const uint32_t* values, int rows, int cols = 1){ return WriteMatrix(name, values, Int32Value, rows, cols); }

	bool GetMatrixInfo(const char* name, ValueType& valueType, int& rows, int& cols);

	uint8_t*  ReadByteMatrix(const char* name, int& rows, int& cols)		{ return (uint8_t*)ReadMatrix(name, ByteValue, rows, cols); }
	uint16_t* ReadUInt16Matrix(const char* name, int& rows, int& cols)	    { return (uint16_t*)ReadMatrix(name, UInt16Value, rows, cols); }
	int16_t*  ReadInt16Matrix(const char* name, int& rows, int& cols)		{ return (int16_t*)ReadMatrix(name, Int16Value, rows, cols); }
	int32_t*  ReadInt32Matrix(const char* name, int& rows, int& cols)		{ return (int32_t*)ReadMatrix(name, Int32Value, rows, cols); }
    float*    ReadFloatMatrix(const char* name, int& rows, int& cols)		    { return (float*)ReadMatrix(name, FloatValue, rows, cols); }
	double*   ReadDoubleMatrix(const char* name, int& rows, int& cols)	    { return (double*)ReadMatrix(name, DoubleValue, rows, cols); }

    // NOTE: Matlab doesn't directly support uint32_t type: these values are saved and loaded as Int32Values
    uint32_t* ReadUInt32Matrix(const char* name, int& rows, int& cols)		{ return (uint32_t*)ReadMatrix(name, Int32Value, rows, cols); }

	int ReadString(const char* name, char* string, size_t maxStringSize);

	// Read matrix values. This function performs (some) data type conversions to specified valueType in cases where data is stored in .mat file as doubles.
	bool ReadMatrixValues(void* values, ValueType valueType, int rows, int cols);

private:
    static uint32_t GetMatlabType(ValueType valueType, size_t& valueSize);	// returns Matlab FX_* type and size in bytes of data type element
    static ValueType GetValueType(uint32_t matlabType, size_t& valueSize);

	// Read a matrix, organized as column-major rows x columns. Caller must delete returned vectors
	void* ReadMatrix(const char* name, ValueType valueType, int& rows, int& cols);

	// Read next matrix name, value type, and dimensions
	bool ReadMatrixInfo(char name[/*maxNameSize*/], size_t maxNameSize, ValueType& valueType, int& rows, int& cols);

	void* ConvertVector(void* fromValues, int valueCount, ValueType fromType, ValueType toType);

private:
	FILE* m_f;
};


}}	// namespace OVR::Util

#endif // OVR_Util_MatFile_h
