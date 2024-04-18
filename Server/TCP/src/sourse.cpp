#include <utility>

#include "../inc/header.h"


#ifdef _WIN32
#define WIN(exp) exp
#define NIX(exp)

inline int convertError() {
    switch (WSAGetLastError()) {
        case 0:
            return 0;
        case WSAEINTR:
            return EINTR;
        case WSAEINVAL:
            return EINVAL;
        case WSA_INVALID_HANDLE:
            return EBADF;
        case WSA_NOT_ENOUGH_MEMORY:
            return ENOMEM;
        case WSA_INVALID_PARAMETER:
            return EINVAL;
        case WSAENAMETOOLONG:
            return ENAMETOOLONG;
        case WSAENOTEMPTY:
            return ENOTEMPTY;
        case WSAEWOULDBLOCK:
            return EAGAIN;
        case WSAEINPROGRESS:
            return EINPROGRESS;
        case WSAEALREADY:
            return EALREADY;
        case WSAENOTSOCK:
            return ENOTSOCK;
        case WSAEDESTADDRREQ:
            return EDESTADDRREQ;
        case WSAEMSGSIZE:
            return EMSGSIZE;
        case WSAEPROTOTYPE:
            return EPROTOTYPE;
        case WSAENOPROTOOPT:
            return ENOPROTOOPT;
        case WSAEPROTONOSUPPORT:
            return EPROTONOSUPPORT;
        case WSAEOPNOTSUPP:
            return EOPNOTSUPP;
        case WSAEAFNOSUPPORT:
            return EAFNOSUPPORT;
        case WSAEADDRINUSE:
            return EADDRINUSE;
        case WSAEADDRNOTAVAIL:
            return EADDRNOTAVAIL;
        case WSAENETDOWN:
            return ENETDOWN;
        case WSAENETUNREACH:
            return ENETUNREACH;
        case WSAENETRESET:
            return ENETRESET;
        case WSAECONNABORTED:
            return ECONNABORTED;
        case WSAECONNRESET:
            return ECONNRESET;
        case WSAENOBUFS:
            return ENOBUFS;
        case WSAEISCONN:
            return EISCONN;
        case WSAENOTCONN:
            return ENOTCONN;
        case WSAETIMEDOUT:
            return ETIMEDOUT;
        case WSAECONNREFUSED:
            return ECONNREFUSED;
        case WSAELOOP:
            return ELOOP;
        case WSAEHOSTUNREACH:
            return EHOSTUNREACH;
        default:
            return EIO;
    }
}

#else
#define WIN(exp)
#define NIX(exp) exp
#endif

/*WIN(WSAData WinSocket::w_data;)*/

Server::Server(const uint16_t port,
                     ServerKeepAliveConfig keep_alive_config,
                     DataHandleFunctionServer handler,
                     ConnectionHandlerFunction connect_handle,
                     ConnectionHandlerFunction disconnect_handle,
                     uint32_t thread_count
)
        : port_(port),
          m_handler_(std::move(handler)),
          m_connectHandle_(std::move(connect_handle)),
          m_disconnectHandle_(std::move(disconnect_handle)),
          m_threadPoolServer_(thread_count),
          m_keepAliveConfig_(keep_alive_config)
{}

Server::~Server() {
    if(m_serverStatus_ == SocketStatusInfo::Connected) {
        StopServer();
    }
}

void Server::StopServer() {
    m_threadPoolServer_.ResetJob();
    m_serverStatus_ = SocketStatusInfo::Disconnected;
    WIN(closesocket)NIX(close)(m_socketServer_);
    m_session_list_.clear();
}

void Server::SetServerDataHandler(Server::DataHandleFunctionServer handler) {
    this->m_handler_ = std::move(handler);
}

uint16_t Server::SetServerPort(const uint16_t port) {
    this->port_ = port;
    StartServer();
    return port;
}

SocketStatusInfo Server::StartServer() {
    if(m_serverStatus_ == SocketStatusInfo::Connected) {
        StopServer();
    }


    SocketAddressIn_t address;

#ifdef _WIN32
    address.sin_addr.S_un.S_addr = INADDR_ANY;
#else
    address.sin_addr.s_addr = INADDR_ANY;
#endif

    address.sin_port = htons(port_);
    address.sin_family = AF_INET;

#ifdef _WIN32
    if ((m_socketServer_ = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        return m_serverStatus_ = SocketStatusInfo::InitError;
    }
    unsigned long mode = 0;
    if (ioctlsocket(m_socketServer_, FIONBIO, &mode) == SOCKET_ERROR) {
        return m_serverStatus_ = SocketStatusInfo::InitError;
    }
#else
    if ((m_socketServer_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1) {
        return m_serverStatus_ = SocketStatusInfo::InitError;
    }
#endif

    int flag = 1;
    if (setsockopt(m_socketServer_, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(flag)) == -1) {
        return m_serverStatus_ = SocketStatusInfo::InitError;
    }

    if (bind(m_socketServer_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        return m_serverStatus_ = SocketStatusInfo::BindError;
    }

    if (listen(m_socketServer_, SOMAXCONN) < 0) {
        return m_serverStatus_ = SocketStatusInfo::ListeningError;
    }

    m_serverStatus_ = SocketStatusInfo::Connected;
    m_threadPoolServer_.AddTask([this]{HandlingAcceptLoop();});
    m_threadPoolServer_.AddTask([this]{WaitingDataLoop();});

    return m_serverStatus_;
}


bool Server::ServerConnectTo(uint32_t host, uint16_t port, const Server::ConnectionHandlerFunction& connect_handle) {
    SocketHandle_t clientSocket;
    SocketAddressIn_t address;

#ifdef _WIN32
    if ((clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) == INVALID_SOCKET) {
        return false;
    }
#else
    if ((clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0) {
        return false;
    }
#endif

    new(&address) SocketAddressIn_t;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = host;

#ifdef _WIN32
    address.sin_addr.S_un.S_addr = host;
#else
    address.sin_addr.s_addr = host;
#endif

    address.sin_port = htons(port);

#ifdef _WIN32
    if(connect(clientSocket, (sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        closesocket(clientSocket);
        return false;
    }
    if(!EnableKeepAlive(clientSocket)) {
        shutdown(clientSocket, 0);
        closesocket(clientSocket);
    }
#else
    if(connect(clientSocket, (sockaddr *)&address, sizeof(address)) != 0) {
        close(clientSocket);
        return false;
    }
    if(!EnableKeepAlive(clientSocket)) {
        shutdown(clientSocket, 0);
        close(clientSocket);
    }
#endif

    std::unique_ptr<InterfaceServerSession> client(new InterfaceServerSession(clientSocket, address));
    connect_handle(*client);

    m_clientMutex_.lock();
    m_session_list_.emplace_back(std::move(client));
    m_clientMutex_.unlock();

    return true;
}

void Server::ServerSendData(const void *buffer, const size_t size) {
    for (std::unique_ptr<InterfaceServerSession>& client : m_session_list_) {
        client ->SendData(buffer, size);
    }
}

bool Server::ServerSendDataBy(uint32_t host, uint16_t port, const void *buffer, const size_t size) {
    bool dataIsSended = false;
    for(std::unique_ptr<InterfaceServerSession>& client : m_session_list_) {
        if (client->GetHost() == host && client->GetPort() == port){
            client->SendData(buffer, size);
            dataIsSended = true;
        }
    }
    return dataIsSended;
}

bool Server::ServerDisconnectBy(uint32_t host, uint16_t port) {
    bool clientIsDisconnected = false;
    for(std::unique_ptr<InterfaceServerSession>& client : m_session_list_) {
        if (client->GetHost() == host && client->GetPort() == port){
            client->Disconnect();
            clientIsDisconnected = true;
        }
    }
    return clientIsDisconnected;
}

void Server::ServerDisconnectAll() {
    for (std::unique_ptr<InterfaceServerSession>& client : m_session_list_) {
        client->Disconnect();
    }
}

void Server::HandlingAcceptLoop() {
    SocketLength_t addrLen = sizeof(SocketAddressIn_t);
    SocketAddressIn_t clientAddr;
#ifdef _WIN32
    if(SocketHandle_t clientSocket = accept(m_socketServer_, (struct sockaddr*)&clientAddr, &addrLen);
              clientSocket != 0 && m_serverStatus_ == SocketStatusInfo::Connected)
    {
        if (EnableKeepAlive(clientSocket))
        {
            std::unique_ptr<InterfaceServerSession> client(new InterfaceServerSession(clientSocket, clientAddr));
            m_connectHandle_(*client);
            m_clientMutex_.lock();
            m_session_list_.emplace_back(std::move(client));
            m_clientMutex_.unlock();
        } else {
            shutdown(clientSocket, 0);
            closesocket(clientSocket);
        }
    }
#else
    if (SocketHandle_t clientSocket = accept4(m_socketServer_, (struct sockaddr*)&clientAddr, &addrLen, SOCK_NONBLOCK); clientSocket >= 0 && m_serverStatus_ == SocketStatusInfo::Connected) {
        if(EnableKeepAlive(clientSocket)) {
            std::unique_ptr<Client> client(new Client(clientSocket, clientAddr));
            m_connectHandle_(*client);
            m_clientMutex_.lock();
            m_session_list_.emplace_back(std::move(client));
            m_clientMutex_.unlock();
        } else {
            shutdown(clientSocket, 0);
            close(clientSocket);
        }
    }
#endif
    if(m_serverStatus_ == SocketStatusInfo::Connected) {
        m_threadPoolServer_.AddTask([this]() { HandlingAcceptLoop(); });
    }
}

bool Server::EnableKeepAlive(SocketHandle_t socket) {
    int flag = 1;
#ifdef _WIN32
    tcp_keepalive keepAlive {1,
                             m_keepAliveConfig_.GetIdle() * 1000,
                             m_keepAliveConfig_.GetInterval() * 1000};
    if (setsockopt(socket,
                   SOL_SOCKET,
                   SO_KEEPALIVE,
                   (const char*)&flag,
                   sizeof(flag)) != 0) {
        return false;
    }
    unsigned long numberBytesReturned = 0;
    if (WSAIoctl(socket,
                 SIO_KEEPALIVE_VALS,
                 &keepAlive,
                 sizeof (keepAlive),
                 nullptr,
                 0,
                 &numberBytesReturned,
                 nullptr,
                 nullptr) != 0) {
        return false;
    }
#else
    if(setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)) == -1) {
        return false;
    }
    if(setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE, &m_keepAliveConfig_.ka_idle, sizeof(m_keepAliveConfig_.ka_idle)) == -1) {
        return false;
    }
    if(setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &m_keepAliveConfig_.ka_intvl, sizeof(m_keepAliveConfig_.ka_intvl)) == -1) {
        return false;
    }
    if(setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &m_keepAliveConfig_.ka_cnt, sizeof(m_keepAliveConfig_.ka_cnt)) == -1) {
        return false;
    }
#endif
    return true;
}

void Server::WaitingDataLoop() {
    {
        std::lock_guard lockGuard(m_clientMutex_);
        for (auto begin = m_session_list_.begin(), end = m_session_list_.end(); begin != end; ++begin) {
            auto &client = *begin;
            if (client) {
                if (DataBuffer_t dataBuffer = client->LoadData(); !dataBuffer.empty()) {
                    m_threadPoolServer_.AddTask([this, data = std::move(dataBuffer), &client] {
                        client->m_accessMutex_.lock();
                        m_handler_(data, *client);
                        client->m_accessMutex_.unlock();
                    });
                } else if (client->m_connectionStatus_ == SocketStatusInfo::Disconnected) {
                    m_threadPoolServer_.AddTask([this, &client, begin] {
                        client->m_accessMutex_.lock();
                        InterfaceServerSession *pointer = client.release();
                        client = nullptr;
                        pointer->m_accessMutex_.unlock();
                        m_disconnectHandle_(*pointer);
                        m_session_list_.erase(begin);
                        delete pointer;

                    });
                }
            }
        }
    }
    if (m_serverStatus_ == SocketStatusInfo::Connected) {
        m_threadPoolServer_.AddTask([this](){WaitingDataLoop();});
    }
}


Server::InterfaceServerSession::InterfaceServerSession(SocketHandle_t socket, SocketAddressIn_t address)
        : m_address_(address), m_socketDescriptor_(socket) {}


Server::InterfaceServerSession::~InterfaceServerSession(){
#ifdef _WIN32
    if(m_socketDescriptor_ == INVALID_SOCKET) {
        return;
    }
    shutdown(m_socketDescriptor_, SD_BOTH);
    closesocket(m_socketDescriptor_);
#else
    if(m_socketDescriptor_ == -1) {
        return;
    }
    shutdown(m_socketDescriptor_, SD_BOTH);
    close(m_socketDescriptor_);
#endif
}

TCPInterfaceBase::SockStatusInfo_t Server::InterfaceServerSession::Disconnect() {
    m_connectionStatus_ = SockStatusInfo_t::Disconnected;
#ifdef _WIN32
    if (m_socketDescriptor_ == INVALID_SOCKET) {
        return m_connectionStatus_;
    }
    shutdown(m_socketDescriptor_, SD_BOTH);
    closesocket(m_socketDescriptor_);
    m_socketDescriptor_ = INVALID_SOCKET;
#else
    if (m_socketDescriptor_ == -1) {
        return m_connectionStatus_;
    }
    shutdown(m_socketDescriptor_, SD_BOTH);
    close(m_socketDescriptor_);
    m_socketDescriptor_ = -1;
#endif
    return m_connectionStatus_;
}

bool Server::InterfaceServerSession::SendData(const void *buffer, const size_t size) const {
    if(m_connectionStatus_ != SocketStatusInfo::Connected) {
        return false;
    }
    void* sendBuffer = malloc(size + sizeof (uint32_t));
    memcpy(reinterpret_cast<char*>(sendBuffer) + sizeof (uint32_t ), buffer, size);
    *reinterpret_cast<uint32_t*>(sendBuffer) = size;
    if(send(m_socketDescriptor_, reinterpret_cast<char*>(sendBuffer), static_cast<int>(size + sizeof(uint32_t)), 0) < 0){
        return false;
    }
    free(sendBuffer);
    return true;
}

DataBuffer_t Server::InterfaceServerSession::LoadData() {
    if (m_connectionStatus_ != SocketStatusInfo::Connected) {
        return DataBuffer_t();
    }
    DataBuffer_t dataBuffer;
    uint32_t size;
    int error = 0;
#ifdef _WIN32
    if (u_long t = true; SOCKET_ERROR == ioctlsocket(m_socketDescriptor_, FIONBIO, &t)) {
        return DataBuffer_t();
    }
    int answer = recv(m_socketDescriptor_, (char *)&size, sizeof(size), 0);
    if (u_long t = false; SOCKET_ERROR == ioctlsocket(m_socketDescriptor_, FIONBIO, &t)){
        return DataBuffer_t();
    }
#else
    int answer = recv(m_socketDescriptor_, (char *)&size, sizeof(size), MSG_DONTWAIT);
#endif
    if (!answer) {
        Disconnect();
        return DataBuffer_t();
    } else if (answer == -1) {
        WIN (
                error = convertError();
                if (!error) {
                    SocketLength_t length = sizeof (error);
                    getsockopt(m_socketDescriptor_, SOL_SOCKET, SO_ERROR, WIN((char*))&error, &length);
                }
        )
        NIX (
                SocketLength_t length = sizeof (error);
                getsockopt(m_socketDescriptor_, SOL_SOCKET, SO_ERROR, WIN((char*))&error, &length);
                if (!error) {
                    error = errno;
                }
        )
    }

    switch (error) {
        case 0:
            break;
        case ETIMEDOUT:
        case ECONNRESET:
        case EPIPE:
            Disconnect();
            [[fallthrough]];
        case EAGAIN:
            return DataBuffer_t();
        default:
            Disconnect();
            std::cerr << "Unhandled error!\n"
                      << "Code: " << error << " Error: " << std::strerror(error) << '\n';
            return DataBuffer_t();
    }

    if (!size) {
        return DataBuffer_t();
    }

    dataBuffer.resize(static_cast<std::vector<DataBuffer_t>::size_type>(size));
    int recvResult = recv(m_socketDescriptor_, reinterpret_cast<char*>(dataBuffer.data()), static_cast<int>(dataBuffer.size()), 0);
    if (recvResult < 0) {
        int err = errno;
        std::cerr << "Error receiving data: " << std::strerror(err) << '\n';
        Disconnect();
        return DataBuffer_t();
    }

    return dataBuffer;
}

uint32_t Server::InterfaceServerSession::GetHost() const {
    return
            WIN (
                    m_address_.sin_addr.S_un.S_addr
            )
            NIX (
                    m_address_.sin_addr.s_addr
            );
}

uint16_t Server::InterfaceServerSession::GetPort() const {
    return m_address_.sin_port;
}

ServerKeepAliveConfig::ServerKeepAliveConfig(KeepAliveProperty_t idle,
                                               KeepAliveProperty_t interval,
                                               KeepAliveProperty_t count
                                               )
                                               :    idle_(idle),
                                                    interval_(interval),
                                                    count_(count)
                                               {}
