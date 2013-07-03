/************************************************************************************

Filename    :   WavPlayer_OSX.h
Content     :   An Apple OSX audio handler.
Created     :   March 5, 2013
Authors     :   Robotic Arm Software - Peter Hoff, Dan Goodman, Bryan Croteau

Copyright   :   Copyright 2013 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus LLC license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

************************************************************************************/

#ifndef OVR_WavPlayer_h
#define OVR_WavPlayer_h

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <AudioToolbox/AudioQueue.h>

#define AUDIO_BUFFERS 4

namespace OVR { namespace Platform { namespace OSX {

typedef struct AQCallbackStruct
{
    AudioQueueRef				Queue;
    UInt32						FrameCount;
    AudioQueueBufferRef			Buffers[AUDIO_BUFFERS];
    AudioStreamBasicDescription DataFormat;
    UInt32						PlayPtr;
    UInt32						SampleLen;
    unsigned char*				PCMBuffer;
} AQCallbackStruct;

class WavPlayer
{
public:
    WavPlayer(const char* fileName);
    int PlayAudio();
private:
    bool isDataChunk(unsigned char* buffer, int index);
    int getWord(unsigned char* buffer, int index);
    short getHalf(unsigned char* buffer, int index);
    void *LoadPCM(const char *filename, unsigned long *len);
    int PlayBuffer(void *pcm, unsigned long len);
    static void aqBufferCallback(void *in, AudioQueueRef inQ, AudioQueueBufferRef outQB);

    short		AudioFormat;
    short		NumChannels;
    int			SampleRate;
    int			ByteRate;
    short		BlockAlign;
    short		BitsPerSample;
    const char* FileName;
};

}}}

#endif
