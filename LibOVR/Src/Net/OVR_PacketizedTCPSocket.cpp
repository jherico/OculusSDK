/************************************************************************************

Filename    :   OVR_PacketizedTCPSocket.cpp
Content     :   TCP with automated message framing.
Created     :   June 10, 2014
Authors     :   Kevin Jenkins

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

#include "OVR_PacketizedTCPSocket.h"

namespace OVR { namespace Net {


//-----------------------------------------------------------------------------
// Constants

static const int LENGTH_FIELD_BYTES = 4;


//-----------------------------------------------------------------------------
// PacketizedTCPSocket

PacketizedTCPSocket::PacketizedTCPSocket()
{
    pRecvBuff = 0;
    pRecvBuffSize = 0;
    Transport = TransportType_PacketizedTCP;
}

PacketizedTCPSocket::PacketizedTCPSocket(SocketHandle _sock, bool isListenSocket) : PacketizedTCPSocketBase(_sock, isListenSocket)
{
    pRecvBuff = 0;
    pRecvBuffSize = 0;
    Transport = TransportType_PacketizedTCP;
}

PacketizedTCPSocket::~PacketizedTCPSocket()
{
    OVR_FREE(pRecvBuff);
}

int PacketizedTCPSocket::Send(const void* pData, int bytes)
{
    int retval = -1; // Default return value on error

    if (bytes > 0)
    {
        const void* buffers[2];
        char fourBytes[4];
        const uint32_t lengthWord = (uint32_t)bytes;
        static_assert(LENGTH_FIELD_BYTES == 4, "Update this part");
        fourBytes[0] = (uint8_t)lengthWord;
        fourBytes[1] = (uint8_t)(lengthWord >> 8);
        fourBytes[2] = (uint8_t)(lengthWord >> 16);
        fourBytes[3] = (uint8_t)(lengthWord >> 24);
        buffers[0] = fourBytes;
        buffers[1] = pData;

        int buffersLengths[2];
        buffersLengths[0] = LENGTH_FIELD_BYTES;
        buffersLengths[1] = bytes;

        // Send it
        retval = PacketizedTCPSocketBase::Send(buffers, buffersLengths, 2) - LENGTH_FIELD_BYTES;
    }

    return retval;
}

void PacketizedTCPSocket::OnRecv(SocketEvent_TCP* eventHandler, uint8_t* pData, int bytesRead)
{
    uint8_t* dataSource = nullptr;
    int dataSourceSize = 0;

    recvBuffLock.DoLock();

    if (pRecvBuff == nullptr)
    {
        dataSource = pData;
        dataSourceSize = bytesRead;
    }
    else
    {
        uint8_t* pRecvBuffNew = (uint8_t*)OVR_REALLOC(pRecvBuff, bytesRead + pRecvBuffSize);
        if (!pRecvBuffNew)
        {
            OVR_FREE(pRecvBuff);
            pRecvBuff = nullptr;
            pRecvBuffSize = 0;
            recvBuffLock.Unlock();
            return;
        }
        else
        {
            pRecvBuff = pRecvBuffNew;

            memcpy(pRecvBuff + pRecvBuffSize, pData, bytesRead);

            dataSourceSize = pRecvBuffSize + bytesRead;
            dataSource = pRecvBuff;
        }
    }

    int bytesReadFromStream;
    while (bytesReadFromStream = BytesFromStream(dataSource, dataSourceSize),
           LENGTH_FIELD_BYTES + bytesReadFromStream <= dataSourceSize)
    {
        dataSource += LENGTH_FIELD_BYTES;
        dataSourceSize -= LENGTH_FIELD_BYTES;

        TCPSocket::OnRecv(eventHandler, dataSource, bytesReadFromStream);

        dataSource += bytesReadFromStream;
        dataSourceSize -= bytesReadFromStream;
    }

    if (dataSourceSize > 0)
    {
        if (dataSource != nullptr)
        {
            if (pRecvBuff == nullptr)
            {
                pRecvBuff = (uint8_t*)OVR_ALLOC(dataSourceSize);
                if (!pRecvBuff)
                {
                    pRecvBuffSize = 0;
                    recvBuffLock.Unlock();
                    return;
                }
                else
                {
                    memcpy(pRecvBuff, dataSource, dataSourceSize);
                }
            }
            else
            {
                memmove(pRecvBuff, dataSource, dataSourceSize);
            }
        }
    }
    else
    {
        if (pRecvBuff != nullptr)
            OVR_FREE(pRecvBuff);

        pRecvBuff = nullptr;
    }
    pRecvBuffSize = dataSourceSize;

    recvBuffLock.Unlock();
}

int PacketizedTCPSocket::BytesFromStream(uint8_t* pData, int bytesRead)
{
    if (pData != 0 && bytesRead >= LENGTH_FIELD_BYTES)
    {
        static_assert(LENGTH_FIELD_BYTES == 4, "Update this part");
        return pData[0] | ((uint32_t)pData[1] << 8) | ((uint32_t)pData[2] << 16) | ((uint32_t)pData[3] << 24);
    }

    return 0;
}


}} // namespace OVR::Net
