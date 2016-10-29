/************************************************************************************

Filename    :   Logger.h
Content     :   A simple logging library for Oculus World Demo
Created     :   August 6, 2016

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

#pragma once

#if defined(_WIN32)
    #include <Windows.h>
#endif

#include <stdio.h>

#define LOG_MODE_STRING " {DEBUG}\t "

template<typename... Args>
inline void WriteLog(Args&&... args)
{
#if defined(_WIN32)
    static const int headerBufferBytes = 1024; // 1 KiB
    char headerBuffer[headerBufferBytes];

    // Get timestamp string
    GetCurrentTimestamp(headerBuffer, headerBufferBytes);

    char buffer[2048];
    int written = snprintf(buffer, sizeof(buffer), args...);
    if (written <= 0 || written >= (int)sizeof(buffer))
    {
        return;
    }

    // Output the header 
    OutputDebugStringA(headerBuffer);
    OutputDebugStringA(LOG_MODE_STRING);
    
    OutputDebugStringA(buffer);
    if (buffer[written - 1] != '\n')
    {
        OutputDebugStringA("\n");
    }

#else
    printf(LOG_MODE_STRING);
    printf(args...);
    printf("\n");
#endif
}


static void GetCurrentTimestamp(char* buffer, int bufferBytes)
{
    SYSTEMTIME time;
    ::GetLocalTime(&time);
    // GetDateFormat and GetTimeFormat returns the number of characters written to the
    // buffer if successful, including the trailing '\0'; and return 0 on failure.
    char dateBuffer[16];
    int  dateBufferLength;
    int  writtenChars = ::GetDateFormatA(LOCALE_USER_DEFAULT, 0, &time, "dd/MM ", dateBuffer,
                                         sizeof(dateBuffer));
    if (writtenChars <= 0)
    {
        // Failure
        buffer[0] = '\0';
        return;
    }
    dateBufferLength = (writtenChars - 1);

    char timeBuffer[32];
    int  timeBufferLength;
    writtenChars = ::GetTimeFormatA(LOCALE_USER_DEFAULT, 0, &time, "HH:mm:ss", timeBuffer,
                                    sizeof(timeBuffer));
    if (writtenChars <= 0)
    {
        // Failure
        buffer[0] = '\0';
        return;
    }
    timeBufferLength = (writtenChars - 1);

    // Append milliseconds
    const char msBuffer[5] =
    {
        (char)('.'),
        (char)(((time.wMilliseconds / 100) % 10) + '0'),
        (char)(((time.wMilliseconds / 10) % 10) + '0'),
        (char)((time.wMilliseconds % 10) + '0'),
        (char)('\0')
    };

    const int writeSum = (dateBufferLength + timeBufferLength + sizeof(msBuffer));

    if (bufferBytes < writeSum)
    {
        buffer[0] = '\0';
        return;
    }

    strcpy_s(buffer, dateBufferLength + 1, dateBuffer);
    strcpy_s(buffer + dateBufferLength, timeBufferLength + 1, timeBuffer);
    strcpy_s(buffer + dateBufferLength + timeBufferLength, sizeof(msBuffer), msBuffer);
}
