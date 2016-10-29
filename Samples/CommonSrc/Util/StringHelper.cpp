/************************************************************************************

Filename    :   StringHelper.cpp
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

#include "Kernel/OVR_UTF8Util.h"
#include <string>

//--------------------------------------------------------------------
// ***** Path-Scanner helper function 

// Scans file path finding filename start and extension start, fills in their addess.
void ScanFilePath(const char* url, const char** pfilename, const char** pext)
{
    const char* urlStart = url;
    const char *filename = 0;
    const char *lastDot = 0;

    uint32_t charVal = OVR::UTF8Util::DecodeNextChar(&url);

    while (charVal != 0)
    {
        if ((charVal == '/') || (charVal == '\\'))
        {
            filename = url;
            lastDot = 0;
        }
        else if (charVal == '.')
        {
            lastDot = url - 1;
        }

        charVal = OVR::UTF8Util::DecodeNextChar(&url);
    }

    if (pfilename)
    {
        if (filename)
            *pfilename = filename;
        else
            *pfilename = urlStart;
    }

    if (pext)
    {
        *pext = lastDot;
    }
}

// Scans till the end of protocol. Returns first character past protocol,
// 0 if not found.
//  - protocol: 'file://', 'http://'
const char* ScanPathProtocol(const char* url)
{
    uint32_t charVal = OVR::UTF8Util::DecodeNextChar(&url);
    uint32_t charVal2;

    while (charVal != 0)
    {
        // Treat a colon followed by a slash as absolute.
        if (charVal == ':')
        {
            charVal2 = OVR::UTF8Util::DecodeNextChar(&url);
            charVal = OVR::UTF8Util::DecodeNextChar(&url);
            if ((charVal == '/') && (charVal2 == '\\'))
                return url;
        }
        charVal = OVR::UTF8Util::DecodeNextChar(&url);
    }
    return 0;
}


//--------------------------------------------------------------------
// ***** String Path API implementation

bool HasAbsolutePath(const char* url)
{
    // Absolute paths can star with:
    //  - protocols:        'file://', 'http://'
    //  - windows drive:    'c:\'
    //  - UNC share name:   '\\share'
    //  - unix root         '/'

    // On the other hand, relative paths are:
    //  - directory:        'directory/file'
    //  - this directory:   './file'
    //  - parent directory: '../file'
    // 
    // For now, we don't parse '.' or '..' out, but instead let it be concatenated
    // to string and let the OS figure it out. This, however, is not good for file
    // name matching in library/etc, so it should be improved.

    if (!url || !*url)
        return true; // Treat empty strings as absolute.    

    uint32_t charVal = OVR::UTF8Util::DecodeNextChar(&url);

    // Fist character of '/' or '\\' means absolute url.
    if ((charVal == '/') || (charVal == '\\'))
        return true;

    while (charVal != 0)
    {
        // Treat a colon followed by a slash as absolute.
        if (charVal == ':')
        {
            charVal = OVR::UTF8Util::DecodeNextChar(&url);
            // Protocol or windows drive. Absolute.
            if ((charVal == '/') || (charVal == '\\'))
                return true;
        }
        else if ((charVal == '/') || (charVal == '\\'))
        {
            // Not a first character (else 'if' above the loop would have caught it).
            // Must be a relative url.
            break;
        }

        charVal = OVR::UTF8Util::DecodeNextChar(&url);
    }

    // We get here for relative paths.
    return false;
}


bool HasExtension(const char* path)
{
    const char* ext = 0;
    ScanFilePath(path, 0, &ext);
    return ext != 0;
}
bool HasProtocol(const char* path)
{
    return ScanPathProtocol(path) != 0;
}


std::string GetPath(std::string name)
{
    const char* filename = 0;
    ScanFilePath(name.c_str(), &filename, 0);

    // Technically we can have extra logic somewhere for paths,
    // such as enforcing protocol and '/' only based on flags,
    // but we keep it simple for now.
    return std::string(name.c_str(), filename ? (filename - name.c_str()) : name.size());
}

std::string GetProtocol(std::string name)
{
    const char* protocolEnd = ScanPathProtocol(name.c_str());
    return std::string(name.c_str(), protocolEnd ? (protocolEnd - name.c_str()) : 0);
}

std::string GetFilename(std::string name)
{
    const char* filename = 0;
    ScanFilePath(name.c_str(), &filename, 0);
    return std::string(filename);
}
std::string GetExtension(std::string name)
{
    const char* ext = 0;
    ScanFilePath(name.c_str(), 0, &ext);
    return std::string(ext);
}

void StripExtension(std::string& name)
{
    const char* ext = 0;
    ScanFilePath(name.c_str(), 0, &ext);
    if (ext)
    {
        name = std::string(name.c_str(), ext - name.c_str());
    }
}

void StripProtocol(std::string& name)
{
    const char* protocol = ScanPathProtocol(name.c_str());
    if (protocol)
        name.assign(protocol, strlen(protocol));
}
