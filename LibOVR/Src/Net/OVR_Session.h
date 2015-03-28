/************************************************************************************

PublicHeader:   n/a
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

#ifndef OVR_Session_h
#define OVR_Session_h

#include <atomic>

#include <OVR_Version.h>
#include "OVR_Socket.h"
#include "OVR_PacketizedTCPSocket.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_Threads.h"
#include "Kernel/OVR_RefCount.h"
#include <stdint.h>


namespace OVR { namespace Net {

class Session;


//-----------------------------------------------------------------------------
// Based on Semantic Versioning ( http://semver.org/ )
//
// Please update changelog below:
// 1.0.0 - [SDK 0.4.0] Initial version (July 21, 2014)
// 1.1.0 - [SDK 0.4.1] Add Get/SetDriverMode_1, HMDCountUpdate_1 Version mismatch results (July 28, 2014)
// 1.2.0 - [SDK 0.4.4]
// 1.2.1 - [SDK 0.5.0] Added DyLib model and SDKVersion
// 1.3.0 - [SDK 0.5.0] Multiple shared memory regions for different objects
//-----------------------------------------------------------------------------

static const uint16_t RPCVersion_Major = 1; // MAJOR version when you make incompatible API changes,
static const uint16_t RPCVersion_Minor = 3; // MINOR version when you add functionality in a backwards-compatible manner, and
static const uint16_t RPCVersion_Patch = 0; // PATCH version when you make backwards-compatible bug fixes.

#define OVR_FEATURE_VERSION 0


struct SDKVersion
{
    uint16_t ProductVersion;        // CAPI DLL product number, 0 before first consumer release
    uint16_t MajorVersion;          // CAPI DLL version major number
    uint16_t MinorVersion;          // CAPI DLL version minor number
    uint16_t RequestedMinorVersion; // Number provided by game in ovr_Initialize() arguments
    uint16_t PatchVersion;          // CAPI DLL version patch number
    uint16_t BuildNumber;           // Number increments per build
    uint16_t FeatureVersion;        // CAPI DLL feature version number

    SDKVersion()
    {
        Reset();
    }

    void Reset()
    {
        ProductVersion        = MajorVersion = MinorVersion = UINT16_MAX;
        RequestedMinorVersion = PatchVersion = BuildNumber = UINT16_MAX;
        FeatureVersion        = UINT16_MAX;
    }

    void SetCurrent()
    {
        ProductVersion        = OVR_PRODUCT_VERSION;
        MajorVersion          = OVR_MAJOR_VERSION;
        MinorVersion          = OVR_MINOR_VERSION;
        RequestedMinorVersion = OVR_MINOR_VERSION;
        PatchVersion          = OVR_PATCH_VERSION;
        BuildNumber           = OVR_BUILD_NUMBER;
        FeatureVersion        = OVR_FEATURE_VERSION;
    }
};

// This is the version that the OVR_CAPI client passes on to the server.  It's a global variable
// because it needs to be initialized in ovr_Initialize but read in the OVR_Session module.
// This variable exists as a global in the server but it has no meaning.
extern SDKVersion RuntimeSDKVersion;


// Client starts communication by sending its version number.
struct RPC_C2S_Hello
{
    RPC_C2S_Hello() :
        MajorVersion(0),
        MinorVersion(0),
        PatchVersion(0),
        CodeVersion()
    {
        CodeVersion.SetCurrent();
    }

    String HelloString;

    // Client protocol version info
    uint16_t MajorVersion, MinorVersion, PatchVersion;

    // Client runtime code version info
    SDKVersion CodeVersion;

    bool Serialize(bool writeToBitstream, Net::BitStream* bs);
    static void ClientGenerate(Net::BitStream* bs);
    bool ServerValidate();
};

// Server responds with an authorization accepted message, including the server's version number
struct RPC_S2C_Authorization
{
    RPC_S2C_Authorization() :
        MajorVersion(0),
        MinorVersion(0),
        PatchVersion(0),
        CodeVersion()
    {
        CodeVersion.SetCurrent();
    }

    String AuthString;

    // Server version info
    uint16_t MajorVersion, MinorVersion, PatchVersion;

    // The SDK version that the server was built with.
    // There's no concept of the server requesting an SDK version like the client does.
    SDKVersion CodeVersion;

    bool Serialize(bool writeToBitstream, Net::BitStream* bs);
    static void ServerGenerate(Net::BitStream* bs, String errorString = "");
    bool ClientValidate();
};


//-----------------------------------------------------------------------------
// Result of a session function
enum SessionResult
{
    SessionResult_OK,
    SessionResult_BindFailure,
    SessionResult_ListenFailure,
    SessionResult_ConnectFailure,
    SessionResult_ConnectInProgress,
    SessionResult_AlreadyConnected,
};


//-----------------------------------------------------------------------------
// Connection state
enum EConnectionState
{
    State_Zombie,          // Disconnected

    // Client-only:
    Client_Connecting,     // Waiting for TCP connection
    Client_ConnectedWait,  // Connected! Waiting for server to authorize

    // Server-only:
    Server_ConnectedWait,  // Connected! Waiting for client handshake

    State_Connected        // Connected
};


//-----------------------------------------------------------------------------
// Generic connection over any transport
class Connection : public RefCountBase<Connection>
{
public:
    Connection() :
        Transport(TransportType_None),
        State(State_Zombie),
        RemoteMajorVersion(0),
        RemoteMinorVersion(0),
        RemotePatchVersion(0),
        RemoteCodeVersion()
    {
    }
    virtual ~Connection() // Allow delete from base
    {
    }

public:
    virtual void SetState(EConnectionState s) {State = s;}

    TransportType    Transport;
    EConnectionState State;

    // Version number read from remote host just before connection completes
    int              RemoteMajorVersion;    // RPC version
    int              RemoteMinorVersion;
    int              RemotePatchVersion;
    SDKVersion       RemoteCodeVersion;
};


//-----------------------------------------------------------------------------
// Generic network connection over any network transport
class NetworkConnection : public Connection
{
protected:
    NetworkConnection()
    {
    }
    virtual ~NetworkConnection()
    {
    }

public:
    // Thread-safe interface to set or wait on a connection state change.
    // All modifications of the connection state should go through this function,
    // on the client side.
    void SetState(EConnectionState s)
    {
        Mutex::Locker locker(&StateMutex);

        if (s != State)
        {
            State = s;

            if (State != Client_Connecting &&
                State != Client_ConnectedWait)
            {
                ConnectingWait.NotifyAll();
            }
        }
    }

    // Call this function to wait for the state to change to a connected state.
    void WaitOnConnecting()
    {
        Mutex::Locker locker(&StateMutex);

        while (State == Client_Connecting || State == Client_ConnectedWait)
        {
            ConnectingWait.Wait(&StateMutex);
        }
    }

    SockAddr      Address;
    Mutex         StateMutex;
    WaitCondition ConnectingWait;
};


//-----------------------------------------------------------------------------
// TCP Connection
class TCPConnection : public NetworkConnection
{
public:
    TCPConnection()
    {
    }
    virtual ~TCPConnection()
    {
    }

public:
    Ptr<TCPSocket> pSocket;
};


//-----------------------------------------------------------------------------
// Packetized TCP Connection
class PacketizedTCPConnection : public TCPConnection
{
public:
    PacketizedTCPConnection()
    {
        Transport = TransportType_PacketizedTCP;
    }
    virtual ~PacketizedTCPConnection()
    {
    }
};


//-----------------------------------------------------------------------------
// Generic socket listener description
class ListenerDescription
{
public:
    ListenerDescription() :
        Transport(TransportType_None)
    {
    }

    TransportType Transport;
};


//-----------------------------------------------------------------------------
// Description for a Berkley socket listener
class BerkleyListenerDescription : public ListenerDescription
{
public:
    static const int DefaultMaxIncomingConnections =  64;
    static const int DefaultMaxConnections         = 128;

    BerkleyListenerDescription() :
        MaxIncomingConnections(DefaultMaxIncomingConnections),
        MaxConnections(DefaultMaxConnections)
    {
    }

    Ptr<BerkleySocket> BoundSocketToListenWith;
    int                MaxIncomingConnections;
    int                MaxConnections;
};


//-----------------------------------------------------------------------------
// Receive payload
struct ReceivePayload
{
    Connection* pConnection; // Source connection
    uint8_t*    pData;       // Pointer to data received
    int         Bytes;       // Number of bytes of data received
};

//-----------------------------------------------------------------------------
// Broadcast parameters
class BroadcastParameters
{
public:
    BroadcastParameters() :
        pData(NULL),
        Bytes(0)
    {
    }

    BroadcastParameters(const void* _pData, int _bytes) :
        pData(_pData),
        Bytes(_bytes)
    {
    }

public:
    const void*     pData;       // Pointer to data to send
    int             Bytes;       // Number of bytes of data received
};

//-----------------------------------------------------------------------------
// Send parameters
class SendParameters
{
public:
    SendParameters() :
        pData(NULL),
        Bytes(0)
    {
    }
    SendParameters(Ptr<Connection> _pConnection, const void* _pData, int _bytes) :
        pConnection(_pConnection),
        pData(_pData),
        Bytes(_bytes)
    {
    }

public:
    Ptr<Connection> pConnection; // Connection to use
    const void*     pData;       // Pointer to data to send
    int             Bytes;       // Number of bytes of data received
};


//-----------------------------------------------------------------------------
// Parameters to connect
struct ConnectParameters
{
public:
    ConnectParameters() :
        Transport(TransportType_None)
    {
    }

    TransportType Transport;
};

struct ConnectParametersBerkleySocket : public ConnectParameters
{
    SockAddr           RemoteAddress;
    Ptr<BerkleySocket> BoundSocketToConnectWith;
    bool               Blocking;

    ConnectParametersBerkleySocket(BerkleySocket* s, SockAddr* addr, bool blocking,
                                   TransportType transport) :
        RemoteAddress(*addr),
        BoundSocketToConnectWith(s),
        Blocking(blocking)
    {
        Transport = transport;
    }
};


//-----------------------------------------------------------------------------
// Listener receive result
enum ListenerReceiveResult
{
    /// The SessionListener used this message and it shouldn't be given to the user.
    LRR_RETURN = 0,

    /// The SessionListener is going to hold on to this message.  Do not deallocate it but do not pass it to other plugins either.
    LRR_BREAK,

    /// This message will be processed by other SessionListeners, and at last by the user.
    LRR_CONTINUE,
};


//-----------------------------------------------------------------------------
// SessionListener

// Callback interface for network events such as connecting, disconnecting, getting data, independent of the transport medium
class SessionListener
{
public:
    virtual ~SessionListener(){}

    // Data events
    virtual void OnReceive(ReceivePayload* pPayload, ListenerReceiveResult* lrrOut) { OVR_UNUSED2(pPayload, lrrOut);  }

    // Connection was closed
    virtual void OnDisconnected(Connection* conn) = 0;

    // Connection was created (some data was exchanged to verify protocol compatibility too)
    virtual void OnConnected(Connection* conn) = 0;

    // Server accepted client
    virtual void OnNewIncomingConnection(Connection* conn)     { OnConnected(conn); }
    // Client was accepted
    virtual void OnConnectionRequestAccepted(Connection* conn) { OnConnected(conn); }

    // Connection attempt failed for some reason
    virtual void OnConnectionAttemptFailed(Connection* conn)   { OnDisconnected(conn); }

    // Incompatible protocol
    virtual void OnIncompatibleProtocol(Connection* conn)      { OnConnectionAttemptFailed(conn); }
    // Disconnected during initial handshake
    virtual void OnHandshakeAttemptFailed(Connection* conn)    { OnConnectionAttemptFailed(conn); }

    // Other
    virtual void OnAddedToSession(Session* session)            { OVR_UNUSED(session); }
    virtual void OnRemovedFromSession(Session* session)        { OVR_UNUSED(session); }
};


//-----------------------------------------------------------------------------
// Session

//  Interface for network events such as listening on a socket, sending data, connecting, and disconnecting. Works independently of the transport medium and also implements loopback
class Session : public SocketEvent_TCP, public NewOverrideBase
{
public:
    Session() :
        HaveFullConnections(false)
    {
    }
    virtual ~Session()
    {
        // Ensure memory backing the sockets array is released
        AllBlockingTcpSockets.ClearAndRelease();
    }

    virtual SessionResult Listen(ListenerDescription* pListenerDescription);
    virtual SessionResult Connect(ConnectParameters* cp);
    virtual int           Send(SendParameters* payload);
    virtual void          Broadcast(BroadcastParameters* payload);
    // DO NOT CALL Poll() FROM MULTIPLE THREADS due to AllBlockingTcpSockets being a member
    virtual void          Poll(bool listeners = true);
    virtual void          AddSessionListener(SessionListener* se);
    virtual void          RemoveSessionListener(SessionListener* se);
    // GetActiveSocketsCount() is not thread-safe: Socket count may change at any time.
    virtual int           GetActiveSocketsCount();

    // Packetized TCP convenience functions
    virtual SessionResult ListenPTCP(BerkleyBindParameters* bbp);
    virtual SessionResult ConnectPTCP(BerkleyBindParameters* bbp, SockAddr* RemoteAddress, bool blocking);

    // Closes all the sockets; useful for interrupting the socket polling during shutdown
    void            Shutdown();

    // Returns true if there is at least one successful connection
    // WARNING: This function may not be in sync across threads, but it IS atomic
    bool            ConnectionSuccessful() const
    {
        return HaveFullConnections.load(std::memory_order_relaxed);
    }

    // Get count of successful connections (past handshake point)
    // WARNING: This function is not thread-safe
    int             GetConnectionCount() const
    {
        return FullConnections.GetSizeI();
    }
    Ptr<Connection> GetConnectionAtIndex(int index);

protected:
    virtual Ptr<Connection> AllocConnection(TransportType transportType);

    Lock SocketListenersLock, ConnectionsLock, SessionListenersLock;
    Array< Ptr<TCPSocket> >   SocketListeners;     // List of active sockets
    Array< Ptr<Connection> >  AllConnections;      // List of active connections stuck at the versioning handshake
    Array< Ptr<Connection> >  FullConnections;     // List of active connections past the versioning handshake
    Array< SessionListener* > SessionListeners;    // List of session listeners
    Array< Ptr< Net::TCPSocket > > AllBlockingTcpSockets; // Preallocated blocking sockets array

    std::atomic<bool> HaveFullConnections;

    // Tools
    Ptr<PacketizedTCPConnection> findConnectionBySocket(Array< Ptr<Connection> >& connectionArray, Socket* s, int *connectionIndex = NULL); // Call with ConnectionsLock held
    Ptr<PacketizedTCPConnection> findConnectionBySockAddr(SockAddr* address); // Call with ConnectionsLock held
    int                   invokeSessionListeners(ReceivePayload*);
    void                  invokeSessionEvent(void(SessionListener::*f)(Connection*), Connection* pConnection);

    // TCP
    virtual void          TCP_OnRecv(Socket* pSocket, uint8_t* pData, int bytesRead);
    virtual void          TCP_OnClosed(TCPSocket* pSocket);
    virtual void          TCP_OnAccept(TCPSocket* pListener, SockAddr* pSockAddr, SocketHandle newSock);
    virtual void          TCP_OnConnected(TCPSocket* pSocket);

public:
    static void SetSingleProcess(bool enable);
    static bool IsSingleProcess();

protected:
    Session* SingleTargetSession; // Target for SingleProcess mode
};


}} // OVR::Net

#endif
