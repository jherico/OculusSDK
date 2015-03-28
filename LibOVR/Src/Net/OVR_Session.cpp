/************************************************************************************

Filename    :   OVR_Session.h
Content     :   One network session that provides connection/disconnection events.
Created     :   June 10, 2014
Authors     :   Kevin Jenkins, Chris Taylor

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

#include "OVR_Session.h"
#include "OVR_PacketizedTCPSocket.h"
#include "Kernel/OVR_Log.h"
#include "Service/Service_NetSessionCommon.h"

namespace OVR { namespace Net {


// The SDK version requested by the user.
SDKVersion RuntimeSDKVersion;


//-----------------------------------------------------------------------------
// Protocol

static const char* OfficialHelloString      = "OculusVR_Hello";
static const char* OfficialAuthorizedString = "OculusVR_Authorized";

bool RPC_C2S_Hello::Serialize(bool writeToBitstream, Net::BitStream* bs)
{
    bs->Serialize(writeToBitstream, HelloString);
    bs->Serialize(writeToBitstream, MajorVersion);
    bs->Serialize(writeToBitstream, MinorVersion);
    if (!bs->Serialize(writeToBitstream, PatchVersion))
        return false;

    // If an older client is connecting to us,
    if (!writeToBitstream && (MajorVersion * 100) + (MinorVersion * 10) + PatchVersion < 121)
    {
        // The following was version code was added to RPC version 1.2
        // without bumping it up to 1.3 and introducing an incompatibility.
        // We can do this because an older server will not read this additional data.
        return true;
    }

    bs->Serialize(writeToBitstream, CodeVersion.ProductVersion);
    bs->Serialize(writeToBitstream, CodeVersion.MajorVersion);
    bs->Serialize(writeToBitstream, CodeVersion.MinorVersion);
    bs->Serialize(writeToBitstream, CodeVersion.RequestedMinorVersion);
    bs->Serialize(writeToBitstream, CodeVersion.PatchVersion);
    bs->Serialize(writeToBitstream, CodeVersion.BuildNumber);
    return bs->Serialize(writeToBitstream, CodeVersion.FeatureVersion);
}

void RPC_C2S_Hello::ClientGenerate(Net::BitStream* bs)
{
    RPC_C2S_Hello hello;
    hello.HelloString  = OfficialHelloString;
    hello.MajorVersion = RPCVersion_Major;
    hello.MinorVersion = RPCVersion_Minor;
    hello.PatchVersion = RPCVersion_Patch;
    OVR_ASSERT(OVR::Net::RuntimeSDKVersion.ProductVersion != UINT16_MAX);
    hello.CodeVersion = OVR::Net::RuntimeSDKVersion; // This should have been set to a value earlier in the first steps of ovr initialization.
    hello.Serialize(true, bs);
}

bool RPC_C2S_Hello::ServerValidate()
{
    // Server checks the protocol version
    return MajorVersion == RPCVersion_Major &&
           MinorVersion <= RPCVersion_Minor &&
           HelloString.CompareNoCase(OfficialHelloString) == 0;
}

bool RPC_S2C_Authorization::Serialize(bool writeToBitstream, Net::BitStream* bs)
{
    bs->Serialize(writeToBitstream, AuthString);
    bs->Serialize(writeToBitstream, MajorVersion);
    bs->Serialize(writeToBitstream, MinorVersion);
    if (!bs->Serialize(writeToBitstream, PatchVersion))
        return false;

    // If an older client is connecting to us,
    if (!writeToBitstream && (MajorVersion * 100) + (MinorVersion * 10) + PatchVersion < 121)
    {
        // The following was version code was added to RPC version 1.2
        // without bumping it up to 1.3 and introducing an incompatibility.
        // We can do this because an older server will not read this additional data.
        return true;
    }

    bs->Serialize(writeToBitstream, CodeVersion.ProductVersion);
    bs->Serialize(writeToBitstream, CodeVersion.MajorVersion);
    bs->Serialize(writeToBitstream, CodeVersion.MinorVersion);
    bs->Serialize(writeToBitstream, CodeVersion.RequestedMinorVersion);
    bs->Serialize(writeToBitstream, CodeVersion.PatchVersion);
    bs->Serialize(writeToBitstream, CodeVersion.BuildNumber);
    return bs->Serialize(writeToBitstream, CodeVersion.FeatureVersion);
}

void RPC_S2C_Authorization::ServerGenerate(Net::BitStream* bs, String errorString)
{
    RPC_S2C_Authorization auth;
    if (errorString.IsEmpty())
    {
        auth.AuthString = OfficialAuthorizedString;
    }
    else
    {
        auth.AuthString = errorString;
    }
    auth.MajorVersion = RPCVersion_Major;
    auth.MinorVersion = RPCVersion_Minor;
    auth.PatchVersion = RPCVersion_Patch;
    // Leave CurrentSDKVersion as it is.
    auth.Serialize(true, bs);
}

bool RPC_S2C_Authorization::ClientValidate()
{
    return AuthString.CompareNoCase(OfficialAuthorizedString) == 0;
}


//-----------------------------------------------------------------------------
// SingleProcess

static bool SingleProcess = false;

void Session::SetSingleProcess(bool enable)
{
    SingleProcess = enable;
}

bool Session::IsSingleProcess()
{
    return SingleProcess;
}


//-----------------------------------------------------------------------------
// Session

void Session::Shutdown()
{
    {
        Lock::Locker locker(&SocketListenersLock);

        const int count = SocketListeners.GetSizeI();
        for (int i = 0; i < count; ++i)
        {
            SocketListeners[i]->Close();
        }
    }

    Lock::Locker locker(&ConnectionsLock);

    const int count = AllConnections.GetSizeI();
    for (int i = 0; i < count; ++i)
    {
        Connection* arrayItem = AllConnections[i].GetPtr();

        if (arrayItem->Transport == TransportType_PacketizedTCP)
        {
            PacketizedTCPConnection* ptcp = (PacketizedTCPConnection*)arrayItem;

            ptcp->pSocket->Close();
        }
    }
}

SessionResult Session::Listen(ListenerDescription* pListenerDescription)
{
    if (pListenerDescription->Transport == TransportType_PacketizedTCP)
    {
        BerkleyListenerDescription* bld = (BerkleyListenerDescription*)pListenerDescription;
        TCPSocket* tcpSocket = (TCPSocket*)bld->BoundSocketToListenWith.GetPtr();

        if (tcpSocket->Listen() < 0)
        {
            return SessionResult_ListenFailure;
        }

        Lock::Locker locker(&SocketListenersLock);
        SocketListeners.PushBack(tcpSocket);
    }
    else
    {
        OVR_ASSERT(false);
    }

    return SessionResult_OK;
}

SessionResult Session::Connect(ConnectParameters *cp)
{
    if (cp->Transport == TransportType_PacketizedTCP)
    {
        ConnectParametersBerkleySocket* cp2 = (ConnectParametersBerkleySocket*)cp;
        Ptr<PacketizedTCPConnection> c;

        {
            Lock::Locker locker(&ConnectionsLock);

            int connIndex;
            Ptr<PacketizedTCPConnection> conn = findConnectionBySocket(AllConnections, cp2->BoundSocketToConnectWith, &connIndex);
            if (conn)
            {
                return SessionResult_AlreadyConnected;
            }

            // If we are already connected, don't create a duplicate connection
            if (FullConnections.GetSizeI() > 0)
            {
                return SessionResult_AlreadyConnected;
            }

            // If we are already connecting, don't create a duplicate connection
            const int count = AllConnections.GetSizeI();
            for (int i = 0; i < count; ++i)
            {
                Connection* arrayItem = AllConnections[i].GetPtr();

                OVR_ASSERT(arrayItem);
                if (arrayItem) {
                    if (arrayItem->State == Client_ConnectedWait
                        || arrayItem->State == Client_Connecting)
                    {
                        return SessionResult_ConnectInProgress;
                    }
                }
            }

            TCPSocketBase* tcpSock = (TCPSocketBase*)cp2->BoundSocketToConnectWith.GetPtr();

            int ret = tcpSock->Connect(&cp2->RemoteAddress);
            if (ret < 0)
            {
                return SessionResult_ConnectFailure;
            }

            Ptr<Connection> newConnection = AllocConnection(cp2->Transport);
            if (!newConnection)
            {
                return SessionResult_ConnectFailure;
            }

            c = (PacketizedTCPConnection*)newConnection.GetPtr();
            c->pSocket = (TCPSocket*) cp2->BoundSocketToConnectWith.GetPtr();
            c->Address = cp2->RemoteAddress;
            c->Transport = cp2->Transport;
            c->SetState(Client_Connecting);

            AllConnections.PushBack(c);
        }

        if (cp2->Blocking)
        {
            c->WaitOnConnecting();
        }

        EConnectionState state = c->State;
        if (state == State_Connected)
        {
            return SessionResult_OK;
        }
        else if (state == Client_Connecting)
        {
            return SessionResult_ConnectInProgress;
        }
        else
        {
            return SessionResult_ConnectFailure;
        }
    }
    else
    {
        OVR_ASSERT(false);
    }

    return SessionResult_OK;
}

static Session* SingleProcessServer = nullptr;

SessionResult Session::ListenPTCP(OVR::Net::BerkleyBindParameters *bbp)
{
    if (Session::IsSingleProcess())
    {
        // Do not actually listen on a socket.
        SingleProcessServer = this;
        return SessionResult_OK;
    }

    Ptr<PacketizedTCPSocket> listenSocket = *new OVR::Net::PacketizedTCPSocket();
    if (listenSocket->Bind(bbp) == INVALID_SOCKET)
    {
        return SessionResult_BindFailure;
    }

    BerkleyListenerDescription bld;
    bld.BoundSocketToListenWith = listenSocket.GetPtr();
    bld.Transport = TransportType_PacketizedTCP;

    return Listen(&bld);
}

SessionResult Session::ConnectPTCP(OVR::Net::BerkleyBindParameters* bbp, SockAddr* remoteAddress, bool blocking)
{
    if (Session::IsSingleProcess())
    {
        OVR_ASSERT(SingleProcessServer); // ListenPTCP() must be called before ConnectPTCP()

        SingleProcessServer->SingleTargetSession = this;
        SingleTargetSession = SingleProcessServer;

        Ptr<PacketizedTCPSocket> s = *new PacketizedTCPSocket;
        SockAddr sa;
        sa.Set("::1", 10101, SOCK_STREAM);

        Ptr<Connection> newConnection = AllocConnection(TransportType_PacketizedTCP);
        if (!newConnection)
        {
            return SessionResult_ConnectFailure;
        }

        PacketizedTCPConnection* c = (PacketizedTCPConnection*)newConnection.GetPtr();
        c->pSocket = s;
        c->Address = &sa;
        c->Transport = TransportType_PacketizedTCP;
        c->SetState(Client_Connecting);
        AllConnections.PushBack(c);

        SingleTargetSession->TCP_OnAccept(s, &sa, INVALID_SOCKET);
        TCP_OnConnected(s);

        return SessionResult_OK;
    }

    ConnectParametersBerkleySocket cp(NULL, remoteAddress, blocking, TransportType_PacketizedTCP);
    Ptr<PacketizedTCPSocket> connectSocket = *new PacketizedTCPSocket();

    cp.BoundSocketToConnectWith = connectSocket.GetPtr();
    if (connectSocket->Bind(bbp) == INVALID_SOCKET)
    {
        return SessionResult_BindFailure;
    }

    return Connect(&cp);
}

Ptr<PacketizedTCPConnection> Session::findConnectionBySockAddr(SockAddr* address)
{
    const int count = AllConnections.GetSizeI();
    for (int i = 0; i < count; ++i)
    {
        Connection* arrayItem = AllConnections[i].GetPtr();

        if (arrayItem->Transport == TransportType_PacketizedTCP)
        {
            PacketizedTCPConnection* conn = (PacketizedTCPConnection*)arrayItem;

            if (conn->Address == *address)
            {
                return conn;
            }
        }
    }

    return 0;
}

int Session::Send(SendParameters *payload)
{
    if (payload->pConnection->Transport == TransportType_PacketizedTCP)
    {
        if (Session::IsSingleProcess())
        {
            OVR_ASSERT(SingleTargetSession->AllConnections.GetSizeI() > 0);
            PacketizedTCPConnection* conn = (PacketizedTCPConnection*)SingleTargetSession->AllConnections[0].GetPtr();
            SingleTargetSession->TCP_OnRecv(conn->pSocket, (uint8_t*)payload->pData, payload->Bytes);
            return payload->Bytes;
        }
        else
        {
            PacketizedTCPConnection* conn = (PacketizedTCPConnection*)payload->pConnection.GetPtr();
            return conn->pSocket->Send(payload->pData, payload->Bytes);
        }
    }

    OVR_ASSERT(false); // Should not reach here
    return 0;
}

void Session::Broadcast(BroadcastParameters *payload)
{
    SendParameters sp;
    sp.Bytes=payload->Bytes;
    sp.pData=payload->pData;

    {
        Lock::Locker locker(&ConnectionsLock);

        const int connectionCount = FullConnections.GetSizeI();
        for (int i = 0; i < connectionCount; ++i)
        {
            sp.pConnection = FullConnections[i];
            Send(&sp);
        }    
    }
}

// DO NOT CALL Poll() FROM MULTIPLE THREADS due to AllBlockingTcpSockets being a member
void Session::Poll(bool listeners)
{
    if (Net::Session::IsSingleProcess())
    {
        // Spend a lot of time sleeping in single process mode
        Thread::MSleep(100);
        return;
    }

    AllBlockingTcpSockets.Clear();

    if (listeners)
    {
        Lock::Locker locker(&SocketListenersLock);

        const int listenerCount = SocketListeners.GetSizeI();
        for (int i = 0; i < listenerCount; ++i)
        {
            AllBlockingTcpSockets.PushBack(SocketListeners[i]);
        }
    }

    {
        Lock::Locker locker(&ConnectionsLock);

        const int connectionCount = AllConnections.GetSizeI();
        for (int i = 0; i < connectionCount; ++i)
        {
            Connection* arrayItem = AllConnections[i].GetPtr();

            if (arrayItem->Transport == TransportType_PacketizedTCP)
            {
                PacketizedTCPConnection* ptcp = (PacketizedTCPConnection*)arrayItem;

                AllBlockingTcpSockets.PushBack(ptcp->pSocket);
            }
            else
            {
                OVR_ASSERT(false);
            }
        }
    }

    const int count = AllBlockingTcpSockets.GetSizeI();
    if (count > 0)
    {
        TCPSocketPollState state;

        // Add all the sockets for polling,
        for (int i = 0; i < count; ++i)
        {
            Net::TCPSocket* sock = AllBlockingTcpSockets[i].GetPtr();

            // If socket handle is invalid,
            if (sock->GetSocketHandle() == INVALID_SOCKET)
            {
                OVR_DEBUG_LOG(("[Session] Detected an invalid socket handle - Treating it as a disconnection."));
                sock->IsConnecting = false;
                TCP_OnClosed(sock);
            }
            else
            {
                state.Add(sock);
            }
        }

        // If polling returns with an event,
        if (state.Poll(AllBlockingTcpSockets[0]->GetBlockingTimeoutUsec(), AllBlockingTcpSockets[0]->GetBlockingTimeoutSec()))
        {
            // Handle any events for each socket
            for (int i = 0; i < count; ++i)
            {
                state.HandleEvent(AllBlockingTcpSockets[i], this);
            }
        }
    }
}

void Session::AddSessionListener(SessionListener* se)
{
    Lock::Locker locker(&SessionListenersLock);

    const int count = SessionListeners.GetSizeI();
    for (int i = 0; i < count; ++i)
    {
        if (SessionListeners[i] == se)
        {
            // Already added
            return;
        }
    }

    SessionListeners.PushBack(se);
    se->OnAddedToSession(this);
}

void Session::RemoveSessionListener(SessionListener* se)
{
    Lock::Locker locker(&SessionListenersLock);

    const int count = SessionListeners.GetSizeI();
    for (int i = 0; i < count; ++i)
    {
        if (SessionListeners[i] == se)
        {
            se->OnRemovedFromSession(this);

            SessionListeners.RemoveAtUnordered(i);
            break;
        }
    }
}

int Session::GetActiveSocketsCount()
{
    return SocketListeners.GetSizeI() + AllConnections.GetSizeI();
}

Ptr<Connection> Session::AllocConnection(TransportType transport)
{
    switch (transport)
    {
    case TransportType_TCP:           return *new TCPConnection();
    case TransportType_PacketizedTCP: return *new PacketizedTCPConnection();
    default:
        OVR_ASSERT(false);
        break;
    }

    return NULL;
}

Ptr<PacketizedTCPConnection> Session::findConnectionBySocket(Array< Ptr<Connection> >& connectionArray, Socket* s, int *connectionIndex)
{
    const int count = connectionArray.GetSizeI();
    for (int i = 0; i < count; ++i)
    {
        Connection* arrayItem = connectionArray[i].GetPtr();

        if (arrayItem->Transport == TransportType_PacketizedTCP)
        {
            PacketizedTCPConnection* ptc = (PacketizedTCPConnection*)arrayItem;

            if (ptc->pSocket == s)
            {
                if (connectionIndex)
                {
                    *connectionIndex = i;
                }
                return ptc;
            }
        }
    }

    return NULL;
}

int Session::invokeSessionListeners(ReceivePayload* rp)
{
    Lock::Locker locker(&SessionListenersLock);

    const int count = SessionListeners.GetSizeI();
    for (int j = 0; j < count; ++j)
    {
        ListenerReceiveResult lrr = LRR_CONTINUE;
        SessionListeners[j]->OnReceive(rp, &lrr);

        if (lrr == LRR_RETURN || lrr == LRR_BREAK)
        {
            break;
        }
    }

    return rp->Bytes;
}

void Session::TCP_OnRecv(Socket* pSocket, uint8_t* pData, int bytesRead)
{
    // KevinJ: 9/2/2014 Fix deadlock - Watchdog calls Broadcast(), which locks ConnectionsLock().
    // Lock::Locker locker(&ConnectionsLock);

    // Look for the connection in the full connection list first
    int connIndex;
    ConnectionsLock.DoLock();
    Ptr<PacketizedTCPConnection> conn = findConnectionBySocket(AllConnections, pSocket, &connIndex);
    ConnectionsLock.Unlock();
    if (conn)
    {
        if (conn->State == State_Connected)
        {
            ReceivePayload rp;
            rp.Bytes = bytesRead;
            rp.pConnection = conn;
            rp.pData = pData;

            // Call listeners
            invokeSessionListeners(&rp);
        }
        else if (conn->State == Client_ConnectedWait)
        {
            // Check the version data from the message
            BitStream bsIn((char*)pData, bytesRead, false);

            RPC_S2C_Authorization auth;
            if (!auth.Serialize(false, &bsIn) ||
                !auth.ClientValidate())
            {
                LogError("{ERR-001} [Session] REJECTED: OVRService did not authorize us: %s", auth.AuthString.ToCStr());

                conn->SetState(State_Zombie);
                invokeSessionEvent(&SessionListener::OnIncompatibleProtocol, conn);
            }
            else
            {
                // Read remote version
                conn->RemoteMajorVersion = auth.MajorVersion;
                conn->RemoteMinorVersion = auth.MinorVersion;
                conn->RemotePatchVersion = auth.PatchVersion;
                conn->RemoteCodeVersion  = auth.CodeVersion;

                // Mark as connected
                conn->SetState(State_Connected);
                ConnectionsLock.DoLock();
                int connIndex2;
                if (findConnectionBySocket(AllConnections, pSocket, &connIndex2)==conn && findConnectionBySocket(FullConnections, pSocket, &connIndex2)==NULL)
                {
                    FullConnections.PushBack(conn);
                    HaveFullConnections.store(true, std::memory_order_relaxed);
                }
                ConnectionsLock.Unlock();
                invokeSessionEvent(&SessionListener::OnConnectionRequestAccepted, conn);
            }
        }
        else if (conn->State == Server_ConnectedWait)
        {
            // Check the version data from the message
            BitStream bsIn((char*)pData, bytesRead, false);

            RPC_C2S_Hello hello;
            if (!hello.Serialize(false, &bsIn) ||
                !hello.ServerValidate())
            {
                LogError("{ERR-002} [Session] REJECTED: Rift application is using an incompatible version %d.%d.%d, feature version %d (my version=%d.%d.%d, feature version %d)",
                         hello.MajorVersion, hello.MinorVersion, hello.PatchVersion, hello.CodeVersion.FeatureVersion,
                         RPCVersion_Major, RPCVersion_Minor, RPCVersion_Patch, OVR_FEATURE_VERSION);

                conn->SetState(State_Zombie);

                // Send auth response
                BitStream bsOut;
                RPC_S2C_Authorization::ServerGenerate(&bsOut, "Incompatible protocol version.  Please make sure your OVRService and SDK are both up to date.");

                SendParameters sp;
                sp.Bytes = bsOut.GetNumberOfBytesUsed();
                sp.pData = bsOut.GetData();
                sp.pConnection = conn;
                Send(&sp);
            }
            else
            {
                if (hello.CodeVersion.FeatureVersion != OVR_FEATURE_VERSION)
                {
                    LogError("[Session] WARNING: Rift application is using a different feature version than the server (server version = %d, app version = %d)",
                        OVR_FEATURE_VERSION, hello.CodeVersion.FeatureVersion);
                }

                // Read remote version
                conn->RemoteMajorVersion = hello.MajorVersion;
                conn->RemoteMinorVersion = hello.MinorVersion;
                conn->RemotePatchVersion = hello.PatchVersion;
                conn->RemoteCodeVersion  = hello.CodeVersion;

                // Send auth response
                BitStream bsOut;
                RPC_S2C_Authorization::ServerGenerate(&bsOut);

                SendParameters sp;
                sp.Bytes = bsOut.GetNumberOfBytesUsed();
                sp.pData = bsOut.GetData();
                sp.pConnection = conn;
                Send(&sp);

                // Mark as connected
                conn->SetState(State_Connected);
                ConnectionsLock.DoLock();
                int connIndex2;
                if (findConnectionBySocket(AllConnections, pSocket, &connIndex2)==conn && findConnectionBySocket(FullConnections, pSocket, &connIndex2)==NULL)
                {
                    FullConnections.PushBack(conn);
                    HaveFullConnections.store(true, std::memory_order_relaxed);
                }
                ConnectionsLock.Unlock();
                invokeSessionEvent(&SessionListener::OnNewIncomingConnection, conn);

            }
        }
        else
        {
            OVR_ASSERT(false);
        }
    }
}

void Session::TCP_OnClosed(TCPSocket* s)
{
    Lock::Locker locker(&ConnectionsLock);

    // If found in the full connection list,
    int connIndex = 0;
    Ptr<PacketizedTCPConnection> conn = findConnectionBySocket(AllConnections, s, &connIndex);
    if (conn)
    {
        AllConnections.RemoveAtUnordered(connIndex);

        // If in the full connection list,
        if (findConnectionBySocket(FullConnections, s, &connIndex))
        {
            FullConnections.RemoveAtUnordered(connIndex);
            if (FullConnections.GetSizeI() < 1)
            {
                HaveFullConnections.store(false, std::memory_order_relaxed);
            }
        }

        // Generate an appropriate event for the current state
        switch (conn->State)
        {
        case Client_Connecting:
            invokeSessionEvent(&SessionListener::OnConnectionAttemptFailed, conn);
            break;
        case Client_ConnectedWait:
        case Server_ConnectedWait:
            invokeSessionEvent(&SessionListener::OnHandshakeAttemptFailed, conn);
            break;
        case State_Connected:
        case State_Zombie:
            invokeSessionEvent(&SessionListener::OnDisconnected, conn);
            break;
        default:
            OVR_ASSERT(false);
            break;
        }

        conn->SetState(State_Zombie);
    }
}

void Session::TCP_OnAccept(TCPSocket* pListener, SockAddr* pSockAddr, SocketHandle newSock)
{
    OVR_UNUSED(pListener);
    Ptr<PacketizedTCPSocket> newSocket = *new PacketizedTCPSocket(newSock, false);

    OVR_ASSERT(pListener->Transport == TransportType_PacketizedTCP);

    // If pSockAddr is not localhost, then close newSock
    if (!pSockAddr->IsLocalhost())
    {
        newSocket->Close();
        return;
    }

    if (newSocket)
    {
        Ptr<Connection> b = AllocConnection(TransportType_PacketizedTCP);
        Ptr<PacketizedTCPConnection> c = (PacketizedTCPConnection*)b.GetPtr();
        c->pSocket = newSocket;
        c->Address = *pSockAddr;
        c->State = Server_ConnectedWait;

        {
            Lock::Locker locker(&ConnectionsLock);
            AllConnections.PushBack(c);
        }

        // Server does not send the first packet.  It waits for the client to send its version
    }
}

void Session::TCP_OnConnected(TCPSocket *s)
{
    Lock::Locker locker(&ConnectionsLock);

    // If connection was found,
    PacketizedTCPConnection* conn = findConnectionBySocket(AllConnections, s);
    if (conn)
    {
        OVR_ASSERT(conn->State == Client_Connecting);

        // Just update state but do not generate any notifications yet
        conn->SetState(Client_ConnectedWait);

        // Send hello message
        BitStream bsOut;
        RPC_C2S_Hello::ClientGenerate(&bsOut);

        SendParameters sp;
        sp.Bytes = bsOut.GetNumberOfBytesUsed();
        sp.pData = bsOut.GetData();
        sp.pConnection = conn;
        Send(&sp);
    }
}

void Session::invokeSessionEvent(void(SessionListener::*f)(Connection*), Connection* conn)
{
    Lock::Locker locker(&SessionListenersLock);

    const int count = SessionListeners.GetSizeI();
    for (int i = 0; i < count; ++i)
    {
        (SessionListeners[i]->*f)(conn);
    }
}

Ptr<Connection> Session::GetConnectionAtIndex(int index)
{
    Lock::Locker locker(&ConnectionsLock);

    const int count = FullConnections.GetSizeI();

    if (index < count)
    {
        return FullConnections[index];
    }

    return NULL;
}


}} // OVR::Net
