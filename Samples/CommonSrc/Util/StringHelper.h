/************************************************************************************

Filename    :   StringHelper.h
Content     :   Helper methods for strings
Created     :   June 10, 2016
Authors     :   Somay Jain

Copyright   :   Copyright 2016 Oculus VR, LLC. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*************************************************************************************/

#ifndef OVR_StringHelper_h
#define OVR_StringHelper_h

#include <string>

// Scans file path finding filename start and extension start, fills in their addess.
void ScanFilePath(const char* url, const char** pfilename, const char** pext);

// Scans till the end of protocol. Returns first character past protocol,
// 0 if not found.
//  - protocol: 'file://', 'http://'
const char* ScanPathProtocol(const char* url);

//--------------------------------------------------------------------
// ***** String Path API implementation

bool HasAbsolutePath(const char* url);

bool HasExtension(const char* path);

bool HasProtocol(const char* path);

std::string GetPath(std::string name);

std::string GetProtocol(std::string name);

std::string GetFilename(std::string name);

std::string GetExtension(std::string name);

void StripExtension(std::string& name);

void StripProtocol(std::string& name);

#endif // OVR_StringHelper_h