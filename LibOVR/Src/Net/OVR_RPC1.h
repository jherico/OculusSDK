/************************************************************************************

PublicHeader:   n/a
Filename    :   OVR_RPC1.h
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

#ifndef OVR_Net_RPC_h
#define OVR_Net_RPC_h

#include "OVR_NetworkPlugin.h"
#include "Kernel/OVR_Hash.h"
#include "Kernel/OVR_String.h"
#include "OVR_BitStream.h"
#include "Kernel/OVR_Threads.h"
#include "Kernel/OVR_Delegates.h"
#include "Kernel/OVR_Callbacks.h"

namespace OVR { namespace Net { namespace Plugins {


typedef Delegate3<OVRError, BitStream&, BitStream&, ReceivePayload const&> RPCDelegate;
typedef Delegate2<void, BitStream&, ReceivePayload const&> RPCSlot;

/// NetworkPlugin that maps strings to function pointers. Can invoke the functions using blocking calls with return values, or signal/slots. Networked parameters serialized with BitStream
class RPC1 : public NetworkPlugin, public NewOverrideBase
{
public:
    RPC1();
    virtual ~RPC1();

    /// Register a slot, which is a function pointer to one or more implementations that supports this function signature
    /// When a signal occurs, all slots with the same identifier are called.
    /// \param[in] sharedIdentifier A string to identify the slot. Recommended to be the same as the name of the function.
    /// \param[in] functionPtr Pointer to the function.
    /// \param[in] callPriority Slots are called by order of the highest callPriority first. For slots with the same priority, they are called in the order they are registered
    void RegisterSlot(String sharedIdentifier,  CallbackListener<RPCSlot>* rpcSlotListener);

    /// \brief Same as \a RegisterFunction, but is called with CallBlocking() instead of Call() and returns a value to the caller
    /// \return true if successfully called, false if there is already a blockingFunction registered for the uniqueID.
    /// \note This function doesn't generate an OVRError upon failure; it merely returns false and expects the caller to act accordingly. 
    bool RegisterBlockingFunction(String uniqueID, RPCDelegate blockingFunction);

    /// \brief Same as UnregisterFunction, except for a blocking function
    void UnregisterBlockingFunction(String uniqueID);

    /// \brief Same as call, but don't return until the remote system replies.
    /// Broadcasting parameter does not exist, this can only call one remote system
    /// \note This function does not return until the remote system responds, disconnects, or was never connected to begin with
    /// \note This function doesn't generate an OVRError upon failure; it merely returns false and expects the caller to act accordingly. All errors are assumed to be connection errors.
    /// \param[in] Identifier originally passed to RegisterBlockingFunction() on the remote system(s)
    /// \param[in] bitStream bitStream encoded data to send to the function callback
    /// \param[in] pConnection connection to send on
    /// \param[out] returnData Written to by the function registered with RegisterBlockingFunction.
    /// \return true if successfully called. False on disconnect, function not registered, or not connected to begin with
    OVRError CallBlocking(String uniqueID, BitStream& bitStream, Connection* pConnection, BitStream* returnData = nullptr);

    /// Calls zero or more functions identified by sharedIdentifier registered with RegisterSlot()
    /// \note This function doesn't generate an OVRError upon failure; it merely returns false and expects the caller to act accordingly. All errors are assumed to be connection errors.
    /// \param[in] sharedIdentifier parameter of the same name passed to RegisterSlot() on the remote system
    /// \param[in] bitStream bitStream encoded data to send to the function callback
    /// \param[in] pConnection connection to send on
    bool Signal(String sharedIdentifier, BitStream& bitStream, Connection* pConnection);
    void BroadcastSignal(String sharedIdentifier, BitStream& bitStream);

protected:
    virtual void OnReceive(ReceivePayload const& pPayload, ListenerReceiveResult& lrrOut) OVR_OVERRIDE;
    virtual void OnDisconnected(Connection* conn) OVR_OVERRIDE;
    virtual void OnConnected(Connection* conn) OVR_OVERRIDE;

    Hash<String, RPCDelegate, String::HashFunctor> RegisteredBlockingFunctions;

    CallbackHash<RPCSlot> SlotHash;

    // Synchronization for RPC caller
    Lock            SingleRPCLock;
    Mutex           CallBlockingMutex;
    WaitCondition   CallBlockingWait;

    BitStream       BlockingReturnValue;
    Ptr<Connection> BlockingOnThisConnection;
    bool            BlockingCallSuccess;
};


}}} // namespace OVR::Net::Plugins

#endif // OVR_Net_RPC_h
