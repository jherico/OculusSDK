/************************************************************************************

Filename    :   OVR_RPC1.cpp
Content     :   A network plugin that provides remote procedure call functionality.
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

#include "OVR_RPC1.h"
#include "OVR_BitStream.h"
#include "Kernel/OVR_Threads.h" // Thread::MSleep
#include "OVR_MessageIDTypes.h"
#include "Service/Service_NetSessionCommon.h"

namespace OVR { namespace Net { namespace Plugins {


//-----------------------------------------------------------------------------
// Types

enum {
    ID_RPC4_SIGNAL,
    CALL_BLOCKING,
    RPC_ERROR_FUNCTION_NOT_REGISTERED,
    ID_RPC4_RETURN,
};


//-----------------------------------------------------------------------------
// RPC1

RPC1::RPC1() :
    RegisteredBlockingFunctions(),
    SlotHash(),
    SingleRPCLock(),
    CallBlockingMutex(),
    CallBlockingWait(),
    BlockingReturnValue(),
    BlockingOnThisConnection(),
    BlockingCallSuccess(false)
{
}

RPC1::~RPC1()
{
}

void RPC1::RegisterSlot(String sharedIdentifier,  CallbackListener<RPCSlot>* rpcSlotObserver)
{
    SlotHash.AddListener(sharedIdentifier, rpcSlotObserver);
}

bool RPC1::RegisterBlockingFunction(String uniqueID, RPCDelegate blockingFunction)
{
    if (RegisteredBlockingFunctions.Get(uniqueID))
        return false;

    RegisteredBlockingFunctions.Set(uniqueID, blockingFunction);
    return true;
}

void RPC1::UnregisterBlockingFunction(String uniqueID)
{
    RegisteredBlockingFunctions.Remove(uniqueID);
}

OVRError RPC1::CallBlocking(String uniqueID, BitStream& bitStream, Connection* pConnection, BitStream* returnData)
{
    // If invalid parameters,
    if (!pConnection)
    {
        // Note: This may happen if the endpoint disconnects just before the call
        return OVR_MAKE_ERROR(ovrError_ServiceError, "No connection");
    }

    BitStream out;
    out.Write((MessageID) OVRID_RPC1);
    out.Write((MessageID) CALL_BLOCKING);
    out.Write(uniqueID);

    bitStream.ResetReadPointer();
    out.AlignWriteToByteBoundary();
    out.Write(bitStream);

    SendParameters sp(pConnection, out.GetData(), out.GetNumberOfBytesUsed());

    if (returnData)
    {
        returnData->Reset();
    }

    // Only one thread call at a time
    Lock::Locker singleRPCLocker(&SingleRPCLock);

    // Note this does not prevent multiple calls at a time because .Wait will unlock it below.
    // The purpose of this mutex is to synchronize the polling thread and this one, not prevent
    // multiple threads from invoking RPC.
    Mutex::Locker locker(&CallBlockingMutex);

    BlockingReturnValue.Reset();
    BlockingOnThisConnection = pConnection;

    int bytesSent = pSession->Send(sp);
    if (bytesSent == sp.Bytes)
    {
        while (BlockingOnThisConnection == pConnection)
        {
            CallBlockingWait.Wait(&CallBlockingMutex);
        }
    }
    else
    {
        return OVR_MAKE_ERROR(ovrError_ServiceError, "Send fail");
    }

    if (!BlockingCallSuccess)
    {
        return OVR_MAKE_ERROR(ovrError_ServiceError, "Blocking call not handled");
    }

    OVRError err = OVRError::Success();

    // For backwards compatibility:
    // With RPC-1.4.0 we introduced OVRError return values from RPC blocking calls.
    if (pConnection->RemoteMinorVersion >= 4)
    {
        Service::NetSessionCommon::SerializeOVRError(BlockingReturnValue, err, false);
        if (!err.Succeeded())
        {
            return err;
        }
    }

    if (returnData)
    {
        returnData->Write(BlockingReturnValue);
        returnData->ResetReadPointer();
    }

    return err;
}

bool RPC1::Signal(String sharedIdentifier, BitStream& bitStream, Connection* pConnection)
{
    BitStream out;
    out.Write((MessageID) OVRID_RPC1);
    out.Write((MessageID) ID_RPC4_SIGNAL);
    out.Write(sharedIdentifier);

    bitStream.ResetReadPointer();
    out.AlignWriteToByteBoundary();
    out.Write(bitStream);

    SendParameters sp(pConnection, out.GetData(), out.GetNumberOfBytesUsed());
    int32_t bytesSent = pSession->Send(sp);
    return bytesSent == sp.Bytes;
}

void RPC1::BroadcastSignal(String sharedIdentifier, BitStream& bitStream)
{
    BitStream out;
    out.Write((MessageID) OVRID_RPC1);
    out.Write((MessageID) ID_RPC4_SIGNAL);
    out.Write(sharedIdentifier);

    bitStream.ResetReadPointer();
    out.AlignWriteToByteBoundary();
    out.Write(bitStream);

    BroadcastParameters p(out.GetData(), out.GetNumberOfBytesUsed());
    pSession->Broadcast(p);
}

void RPC1::OnReceive(ReceivePayload const& pPayload, ListenerReceiveResult& lrrOut)
{
    OVR_UNUSED(lrrOut);

    if (pPayload.pData[0] == OVRID_RPC1)
    {
        OVR_ASSERT(pPayload.Bytes >= 2);

        BitStream bsIn((char*)pPayload.pData, pPayload.Bytes, false);
        bsIn.IgnoreBytes(2);

        if (pPayload.pData[1] == RPC_ERROR_FUNCTION_NOT_REGISTERED)
        {
            Mutex::Locker locker(&CallBlockingMutex);

            BlockingReturnValue.Reset();
            BlockingOnThisConnection = 0;
            BlockingCallSuccess = false;
            CallBlockingWait.NotifyAll();
        }
        else if (pPayload.pData[1] == ID_RPC4_RETURN)
        {
            Mutex::Locker locker(&CallBlockingMutex);

            BlockingReturnValue.Reset();
            BlockingReturnValue.Write(bsIn);
            BlockingOnThisConnection = 0;
            BlockingCallSuccess = true;
            CallBlockingWait.NotifyAll();
        }
        else if (pPayload.pData[1] == CALL_BLOCKING)
        {
            String uniqueId;
            bsIn.Read(uniqueId);

            RPCDelegate* bf = RegisteredBlockingFunctions.Get(uniqueId);
            if (!bf)
            {
                BitStream bsOut;
                bsOut.Write((unsigned char) OVRID_RPC1);
                bsOut.Write((unsigned char) RPC_ERROR_FUNCTION_NOT_REGISTERED);

                SendParameters sp(pPayload.pConnection, bsOut.GetData(), bsOut.GetNumberOfBytesUsed());
                pSession->Send(sp);

                return;
            }

            BitStream returnData;
            bsIn.AlignReadToByteBoundary();
            OVRError err = (*bf)(bsIn, returnData, pPayload);

            BitStream out;
            out.Write((MessageID) OVRID_RPC1);
            out.Write((MessageID) ID_RPC4_RETURN);
            returnData.ResetReadPointer();
            out.AlignWriteToByteBoundary();

            // For backwards compatibility:
            // With RPC-1.4.0 we introduced OVRError return values from RPC blocking calls.
            if (pPayload.pConnection->RemoteMinorVersion >= 4)
            {
                Service::NetSessionCommon::SerializeOVRError(out, err);
            }

            out.Write(returnData);

            SendParameters sp(pPayload.pConnection, out.GetData(), out.GetNumberOfBytesUsed());
            pSession->Send(sp);
        }
        else if (pPayload.pData[1]==ID_RPC4_SIGNAL)
        {
            String sharedIdentifier;
            bsIn.Read(sharedIdentifier);

            CallbackEmitter<RPCSlot>* o = SlotHash.GetKey(sharedIdentifier);

            if (o)
            {
                bsIn.AlignReadToByteBoundary();

                BitStream serializedParameters(bsIn.GetData() + bsIn.GetReadOffset()/8, bsIn.GetNumberOfUnreadBits()/8, false);

                o->Call(serializedParameters, pPayload);
            }
        }
    }
}

void RPC1::OnDisconnected(Connection* conn)
{
    if (BlockingOnThisConnection == conn)
    {
        BlockingOnThisConnection = 0;
        BlockingCallSuccess = false;
        CallBlockingWait.NotifyAll();
    }
}

void RPC1::OnConnected(Connection* conn)
{
    OVR_UNUSED(conn);
}


}}} // namespace OVR::Net::Plugins
