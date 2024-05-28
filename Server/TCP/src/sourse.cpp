#include <utility>
#include <future>

#include "../inc/header.h"

#define DEBUGLOG


#ifdef _WIN32
#include <mstcpip.h>
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
#define DEBUGLOG(exp)
#endif

#define ROTLEFT(a,b) (((a) << (b)) | ((a) >> (32-(b))))
#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32-(b))))

#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))

static const WORDL k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

sqlite3* dbConnection;

Server::Server(const uint16_t port,
                     ServerKeepAliveConfig keep_alive_config,
                     DataHandleFunctionServer handler,
                     ConnectionHandlerFunction connect_handle,
                     ConnectionHandlerFunction disconnect_handle,
                     unsigned int thread_count
)
        : port_(port),
          m_handler_(std::move(handler)),
          m_connectHandle_(std::move(connect_handle)),
          m_disconnectHandle_(std::move(disconnect_handle)),
          m_threadPoolServer_(thread_count),
          m_keepAliveConfig_(keep_alive_config)
{
    initializeDatabase();
}

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
    //std::cout << __FUNCTION__ << std::endl;
    int flag;
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

    if (unsigned long mode = 0; ioctlsocket(m_socketServer_, FIONBIO, &mode) == SOCKET_ERROR) {
        return m_serverStatus_ = SocketStatusInfo::InitError;
    }
#else
    if ((m_socketServer_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1) {
        return m_serverStatus_ = SocketStatusInfo::InitError;
    }
#endif

    if(flag = true;
        (setsockopt(m_socketServer_, SOL_SOCKET, SO_REUSEADDR, WIN((char*))&flag, sizeof(flag)) == -1) ||
        (bind(m_socketServer_, (struct sockaddr*)&address, sizeof(address)) WIN(== SOCKET_ERROR)NIX(< 0))){
        return m_serverStatus_ = SocketStatusInfo::BindError;
    }


    if (listen(m_socketServer_, SOMAXCONN) WIN(== SOCKET_ERROR)NIX(< 0)) {
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

    std::unique_ptr<InterfaceClientSession> client(new InterfaceClientSession(clientSocket, address));
    connect_handle(*client);

    m_clientMutex_.lock();
    m_session_list_.emplace(std::move(client));
    m_clientMutex_.unlock();

    return true;
}

void Server::ServerSendData(const void *buffer, const size_t size) {
    for (const std::unique_ptr<InterfaceClientSession>& client : m_session_list_) {
        client ->SendData(buffer, size);
    }
}

bool Server::ServerSendDataBy(uint32_t host, uint16_t port, const void *buffer, const size_t size) {
    bool dataIsSended = false;
    for(const std::unique_ptr<InterfaceClientSession>& client : m_session_list_) {
        if (client->GetHost() == host && client->GetPort() == port){
            client->SendData(buffer, size);
            dataIsSended = true;
        }
    }
    return dataIsSended;
}

bool Server::ServerDisconnectBy(uint32_t host, uint16_t port) {
    bool clientIsDisconnected = false;
    for(const std::unique_ptr<InterfaceClientSession>& client : m_session_list_) {
        if (client->GetHost() == host && client->GetPort() == port){
            client->Disconnect();
            clientIsDisconnected = true;
        }
    }
    return clientIsDisconnected;
}

void Server::ServerDisconnectAll() {
    for (const std::unique_ptr<InterfaceClientSession>& client : m_session_list_) {
        client->Disconnect();
    }
}

void Server::HandlingAcceptLoop() {
    SocketLength_t addrLen = sizeof(SocketAddressIn_t);
    SocketAddressIn_t clientAddr;
#ifdef _WIN32
    if(SocketHandle_t clientSocket =
            accept(m_socketServer_, (struct sockaddr*)&clientAddr, &addrLen);
            clientSocket != 0 && m_serverStatus_ == SocketStatusInfo::Connected) {
        if (EnableKeepAlive(clientSocket))
        {
            std::unique_ptr<InterfaceClientSession> client(new InterfaceClientSession(clientSocket, clientAddr));
            m_connectHandle_(*client);
            m_clientMutex_.lock();
            m_session_list_.emplace(std::move(client));
            m_clientMutex_.unlock();
        } else {
            shutdown(clientSocket, 0);
            closesocket(clientSocket);
        }
    }
#else
    if (SocketHandle_t clientSocket = accept4(m_socketServer_, (struct sockaddr*)&clientAddr, &addrLen, SOCK_NONBLOCK); clientSocket >= 0 && m_serverStatus_ == SocketStatusInfo::Connected) {
        if(EnableKeepAlive(clientSocket)) {
            std::unique_ptr<InterfaceClientSession> client(new InterfaceClientSession(clientSocket, clientAddr));
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
    KeepAliveProperty_t idle = m_keepAliveConfig_.GetIdle();
    KeepAliveProperty_t interval = m_keepAliveConfig_.GetInterval();
    KeepAliveProperty_t count = m_keepAliveConfig_.GetCount();

    if(setsockopt(socket,
                  IPPROTO_TCP,
                  TCP_KEEPIDLE,
                  &idle,
                  sizeof(KeepAliveProperty_t)) == -1)
    {
        return false;
    }

    if(setsockopt(socket,
                  IPPROTO_TCP,
                  TCP_KEEPINTVL,
                  &interval,
                  sizeof(KeepAliveProperty_t)) == -1)
    {
        return false;
    }

    if(setsockopt(socket,
                  IPPROTO_TCP,
                  TCP_KEEPCNT,
                  &count,
                  sizeof(KeepAliveProperty_t)) == -1)
    {
        return false;
    }
#endif
    return true;
}

void Server::WaitingDataLoop() {
    [this]{
        std::lock_guard lock(m_clientMutex_);
        for(auto it = m_session_list_.begin(), end = m_session_list_.end(); it != end; ++it) {

            do {
                auto& client = *it;

                if(DataBuffer_t data = client->LoadData(); !data.empty()) {

                    m_threadPoolServer_.AddTask([this, _data = std::move(data), &client = *client]{
                        client.m_accessMutex_.lock();
                        m_handler_(_data, client);
                        client.m_accessMutex_.unlock();
                    });

                } else if(client->m_connectionStatus_ == SocketStatusInfo::Disconnected) {

                    m_threadPoolServer_.AddTask([this, it = it++]{
                        m_clientMutex_.lock();
                        auto client_node = m_session_list_.extract(it);
                        m_clientMutex_.unlock();
                        client_node.value()->m_accessMutex_.lock();
                        m_disconnectHandle_(*client_node.value());
                        client_node.value()->m_accessMutex_.unlock();
                    });

                    if(it == m_session_list_.cend()) return;
                    else continue;
                }

                break;
            } while(true);

        }
    }();

    if(m_serverStatus_ == SocketStatusInfo::Connected) m_threadPoolServer_.AddTask([this](){WaitingDataLoop();});
}

void Server::printUserInfo(const Server::UserInfo &userInfo) {
    std::cout << "Password: " << userInfo.password_ << std::endl;
    std::cout << "Date Today: " << userInfo.timeToday_ << std::endl;
    std::cout << "Session Port: " << userInfo.sessionPort_ << std::endl;
    std::cout << "Time connect: " << userInfo.connectTime_ << std::endl;
    std::cout << "Time disconnect: " << userInfo.disconnectTime_ << std::endl;
    std::cout << "Duration Time: " << userInfo.duration_ << std::endl;
}

void Server::printAllUsersInfo() {
    std::lock_guard<std::mutex> lock(usersMutex);
    for (const auto& pair : users) {
        std::cout << "Username: " << pair.first << std::endl;
        std::cout << "User Info:" << std::endl;
        for (const auto& userInfo : pair.second) {
            printUserInfo(userInfo);
            std::cout << "-----------------------------" << std::endl;
        }
        std::cout << std::endl;
    }
}

void Server::initializeDatabase() {
    int rc;
    char* errorMsg = nullptr;

    rc = sqlite3_open("C:/CLionProjects/ClientServerApp/Server/example.db", &dbConnection);
    if (rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(dbConnection) << std::endl;
        sqlite3_close(dbConnection);
        return;
    }

    rc = sqlite3_exec(dbConnection, "BEGIN TRANSACTION", nullptr, nullptr, &errorMsg);
    rc = sqlite3_exec(dbConnection, "PRAGMA foreign_keys = OFF", nullptr, nullptr, &errorMsg);
    rc = sqlite3_exec(dbConnection, "CREATE TABLE IF NOT EXISTS UserInfo ("
                                    "username TEXT,"
                                    "password TEXT,"
                                    "sessionPort INTEGER,"
                                    "connectTime TEXT,"
                                    "disconnectTime TEXT,"
                                    "duration TEXT,"
                                    "timeToday TEXT"
                                    ")", nullptr, nullptr, &errorMsg);
    rc = sqlite3_exec(dbConnection, "COMMIT", nullptr, nullptr, &errorMsg);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errorMsg << std::endl;
        sqlite3_free(errorMsg);
    } else {
        std::cout << "Database initialized successfully." << std::endl;
    }
}

void Server::writeToDatabase(const Server::UserInfo &userInfo) {
    int rc;
    char* errorMsg = nullptr;

    std::string insertSQL = "INSERT INTO UserInfo (username, password, sessionPort, connectTime, disconnectTime, duration, timeToday) VALUES (?, ?, ?, ?, ?, ?, ?)";

    sqlite3_stmt* stmt;

    rc = sqlite3_prepare_v2(dbConnection, insertSQL.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare SQL statement: " << sqlite3_errmsg(dbConnection) << std::endl;
        return;
    }

    rc = sqlite3_bind_text(stmt, 1, userInfo.username_.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_bind_text(stmt, 2, userInfo.password_.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_bind_int(stmt, 3, userInfo.sessionPort_);
    rc = sqlite3_bind_text(stmt, 4, userInfo.connectTime_.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_bind_text(stmt, 5, userInfo.disconnectTime_.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_bind_text(stmt, 6, userInfo.duration_.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_bind_text(stmt, 7, userInfo.timeToday_.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Error executing SQL statement: " << sqlite3_errmsg(dbConnection) << std::endl;
    } else {
        std::cout << "Data written to database successfully." << std::endl;
    }

    sqlite3_finalize(stmt);
}

void Server::clearUser(const std::string &username) {
    std::lock_guard<std::mutex> lock(usersMutex);
    auto it = users.find(username);
    if (it != users.end()) {
        it->second.clear();
        users.erase(it);
    }
}

void Server::HandleRegisterUser(Server::InterfaceClientSession &client, uint16_t codeSequence,
                                const std::string &username, const std::string &password) {
#pragma pack(push, 1)
    struct {
        uint16_t codeSequence_;
        ResponseCode responseCode_;
    } response {
            codeSequence,
            sessionManager.RegistredUser(&client, username, password) ? ResponseCode::AuthenticationOk : ResponseCode::AuthenticationFail
    };
#pragma pack(pop)

    client.SendData(&response, sizeof(response));
}

void Server::HandleAuthorize(Server::InterfaceClientSession &client, uint16_t codeSequence,
                             const std::string &username, const std::string &password) {
#pragma pack(push, 1)
    struct {
        uint16_t codeSequence_;
        ResponseCode responseCode_;
    } response {
            codeSequence,
            sessionManager.AuthorizeUser(&client, username, password) ? ResponseCode::AuthenticationOk : ResponseCode::AuthenticationFail
    };
#pragma pack(pop)

    client.SendData(&response, sizeof(response));
}

void Server::HandleSendTo(Server::InterfaceClientSession &client, uint16_t codeSequence,
                          const std::string &recipientUsername, const std::string &message) {
    if (sessionManager.GetStatus(&client) != ConnectionStatus::Authorized) {
#pragma pack(push, 1)
        struct {
            uint16_t codeSequence_;
            ResponseCode responseCode_;
        } response {
                codeSequence,
                ResponseCode::AccessDenied
        };
#pragma pack(pop)
        client.SendData(&response, sizeof(response));
        return;
    }

    std::string sender_username = sessionManager.GetUsername(&client);
    auto recipient_sockets = sessionManager.findSocketListByUsername(sender_username);

    if (recipient_sockets.empty()) {
#pragma pack(push, 1)
        struct {
            uint16_t codeSequence_;
            ResponseCode responseCode_;
        } response {
                codeSequence,
                ResponseCode::SendingFail
        };
#pragma pack(pop)
        client.SendData(&response, sizeof(response));
        return;
    }

    DataBuffer_t message_buffer;
    message_buffer.reserve(
            sizeof(uint16_t) +
            sizeof(ResponseCode) +
            sizeof(uint64_t) +
            sender_username.size() +
            sizeof(uint64_t) +
            message.size()
    );

    NetworkThreadPool::Append(message_buffer, uint16_t(-1));
    NetworkThreadPool::Append(message_buffer, ResponseCode::IncomingMessage);
    NetworkThreadPool::AppendString(message_buffer, sender_username);
    NetworkThreadPool::AppendString(message_buffer, message);

    for (auto& recipient_socket : recipient_sockets)
        recipient_socket->SendData(message_buffer.data(), message_buffer.size());

#pragma pack(push, 1)
    struct {
        uint16_t act_sequence;
        ResponseCode response_code;
    } response {
            codeSequence,
            ResponseCode::SendingOk
    };
#pragma pack(pop)
    client.SendData(&response, sizeof(response));
}

bool Server::IsUserRegistered(const std::string &username) const {
    UserLoginInfo userLoginInfo;
    userLoginInfo.username_ = username;
    return userManager.Find(userLoginInfo) != nullptr;
}

Server::InterfaceClientSession::InterfaceClientSession(SocketHandle_t socket, SocketAddressIn_t address)
        : m_address_(address), m_socketDescriptor_(socket){
    SetFirstConnectionTime();
}


Server::InterfaceClientSession::~InterfaceClientSession(){
    //std::cout << __FUNCTION__ << std::endl;
    InterfaceClientSession::Disconnect();
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

TCPInterfaceBase::SockStatusInfo_t Server::InterfaceClientSession::Disconnect() {
    //std::cout << __FUNCTION__ << std::endl;
    SetLastDisconnectionTime();
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
    m_lastDisconnectionTime_ = std::chrono::system_clock::now();
    return m_connectionStatus_;
}

bool Server::InterfaceClientSession::SendData(const void *buffer, const size_t size) const {
    if(m_connectionStatus_ != SocketStatusInfo::Connected) {
        return false;
    }
    void* sendBuffer = malloc(size + sizeof (uint32_t));
    memcpy(reinterpret_cast<char*>(sendBuffer) + sizeof (uint32_t ), buffer, size);
    *reinterpret_cast<uint32_t*>(sendBuffer) = size;
    if(send(m_socketDescriptor_, reinterpret_cast<char*>(sendBuffer), static_cast<int>(size + sizeof(uint32_t)), 0) < 0){
        free(sendBuffer);
        return false;
    }
    free(sendBuffer);
    return true;
}

DataBuffer_t Server::InterfaceClientSession::LoadData() {
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
    //TODO std::cout << "before recv" << std::endl;
    int answer = recv(m_socketDescriptor_, (char *)&size, sizeof(size), 0);
    // TODO std::cout << "after recv" << std::endl;
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
                SocketLength_t length = sizeof(error);
                getsockopt(m_socketDescriptor_, SOL_SOCKET, SO_ERROR, WIN((char*))&error, &length);
                if (!error) {
                    error = errno;
                }
        )

        switch (error) {
            case 0:
                return DataBuffer_t();
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


uint32_t Server::InterfaceClientSession::GetHost() const {
    return
            WIN (
                    m_address_.sin_addr.S_un.S_addr
            )
            NIX (
                    m_address_.sin_addr.s_addr
            );
}

uint16_t Server::InterfaceClientSession::GetPort() const {
    return m_address_.sin_port;
}

std::string Server::InterfaceClientSession::ConnectionTimes(const InterfaceClientSession& client, Server& server) {
    std::chrono::system_clock::time_point firstConnectionTime = client.GetFirstConnectionTime();
    std::chrono::system_clock::time_point lastDisconnectionTime = client.GetLastDisconnectionTime();

    std::chrono::seconds duration = std::chrono::duration_cast<std::chrono::seconds>(lastDisconnectionTime - firstConnectionTime);

    auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
    duration -= hours;
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
    duration -= minutes;
    auto seconds = duration;

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << hours.count() << ":"
        << std::setfill('0') << std::setw(2) << minutes.count() << ":"
        << std::setfill('0') << std::setw(2) << seconds.count();

    return oss.str();
}

bool Server::InterfaceClientSession::AutentficateUserInfo(const DataBuffer_t& data,
                                                          Server::InterfaceClientSession& client,
                                                          Server& server) {
    std::string receivedMessageAll(data.begin(), data.end());
    receivedMessageAll.erase(std::remove(receivedMessageAll.begin(), receivedMessageAll.end(), '\0'),
                             receivedMessageAll.end());

    std::string startPoint;
    startPoint +='\x3C';
    startPoint +='\x2F';

    std::string::size_type start = receivedMessageAll.find(startPoint);

    std::string stopPoint;
    stopPoint +=('\x7C');
    stopPoint +='\x3E';

    std::string::size_type end = receivedMessageAll.rfind(stopPoint);

    if(start == std::string::npos || end == std::string::npos || start >= end){
#ifdef DEBUGLOG
        //std::cerr << "Notification format error\n";
#endif
        return false;
    }

    std::string content = receivedMessageAll.substr(start + 2, end - (start + 2));
    end += 2;

    std::string startDataContent;
    startDataContent += '\x7B';
    std::string::size_type startData = content.find(startDataContent);

    std::string stopDataContent;
    stopDataContent += '\x7D';
    std::string::size_type endData = content.rfind(stopDataContent);

    if(startData == std::string::npos || endData == std::string::npos || startData >= endData){
#ifdef DEBUGLOG
        std::cerr << "Data format error\n";
#endif
        return false;
    }

    std::string dataContent = content.substr(startData + 1, endData - (startData + 1));

    int messageType = std::stoi(content.substr(0, 1));

    switch (messageType) {
        case MessageTypes::SendAuthenticationUser: {
            std::string userData = dataContent.substr(0);
            size_t colonPos = userData.find(':');
            if (colonPos == std::string::npos) {
#ifdef DEBUGLOG
                std::cerr << "User data format error\n";
#endif
                return false;
            }

            std::string hash = receivedMessageAll.substr(end + 1); // Extract hash
            if (!checkHash(userData, hash)) {
#ifdef DEBUGLOG
                std::cerr << "Hash mismatch\n";
#endif
                return false;
            }

            std::string username = userData.substr(0, colonPos);
            client.username_ = username;
            std::string password = userData.substr(colonPos + 1);

            std::lock_guard<std::mutex> lock(server.usersMutex);
            try {
                auto it = server.users.find(username);
                if (it == server.users.end()) {
                    server.users.emplace(username, std::vector<UserInfo>{UserInfo{username,
                                                                                  password,
                                                                                  client.GetPort(),
                                                                                  client.GetConnectionTime(),
                                                                                  "",
                                                                                  "",
                                                                                  client.GetDayNow()}});
#ifdef DEBUGLOG
                    std::cout << "User '" << username << "' has been assigned port: " << client.GetPort() << std::endl;
#endif
                } else {
                    it->second.emplace_back(username,
                                            password,
                                            client.GetPort(),
                                            client.GetConnectionTime(),
                                            "",
                                            "",
                                            client.GetDayNow());
#ifdef DEBUGLOG
                    std::cout << "User '" << username << "' authenticated successfully\n";
#endif
                }
            } catch (...) {
                throw;
            }

            break;
        }
        case MessageTypes::SendMessageTo: {
            std::string sender;
            std::string recipientName;
            std::string message;

            size_t arrowPos = dataContent.find("->");
            if (arrowPos == std::string::npos) {
#ifdef DEBUGLOG
                std::cerr << "Invalid message format: no arrow found\n";
#endif
                return false;
            }

            std::string hash = receivedMessageAll.substr(end + 1);
            if (!checkHash(dataContent, hash)) {
#ifdef DEBUGLOG
                std::cerr << "Hash mismatch\n";
#endif
                return false;
            }

            sender = dataContent.substr(0, arrowPos);

            size_t equalPos = dataContent.find("=");
            if (equalPos == std::string::npos) {
#ifdef DEBUGLOG
                std::cerr << "Invalid message format: no equal sign found\n";
#endif
                return false;
            }
            recipientName = dataContent.substr(arrowPos + 2, equalPos - (arrowPos + 2));
            message = dataContent.substr(equalPos + 1);

            std::lock_guard<std::mutex> lock(server.usersMutex);

            auto senderSession = std::find_if(server.m_session_list_.begin(), server.m_session_list_.end(),
                                              [&](const auto& session) { return session->GetUserNameIn() == sender; });
            if (senderSession == server.m_session_list_.end()) {
#ifdef DEBUGLOG
                std::cerr << "Sender session not found\n";
#endif
                return false;
            }

            auto recipientSession = std::find_if(server.m_session_list_.begin(), server.m_session_list_.end(),
                                                 [&](const auto& session) { return session->GetUserNameIn() == recipientName; });
            if (recipientSession == server.m_session_list_.end()) {
#ifdef DEBUGLOG
                std::cerr << "Recipient session not found\n";
#endif
                return false;
            }

            std::string fullMessage = sender + "->" + recipientName + "=" + message;
            bool sendSuccess = (*recipientSession)->SendData(fullMessage.c_str(), fullMessage.size());

            return sendSuccess;
        }
        default: {
#ifdef DEBUGLOG
            std::cerr << "Unsupported message type\n";
#endif
            return false;
        }
    }

    return true;
}



void Server::InterfaceClientSession::OnDisconnect(const InterfaceClientSession& client, Server& server) {
    std::string username = client.GetUserNameIn();
    uint16_t port = client.GetPort();
    std::string sumDuration = Server::InterfaceClientSession::ConnectionTimes(client, server);
    std::string lastConnection = Server::InterfaceClientSession::GetDisconnectionTime();

    std::lock_guard<std::mutex> lock(server.usersMutex);
    auto it = server.users.find(username);
    if (it != server.users.end()) {
        for (auto& userInfo : it->second) {
            if (userInfo.sessionPort_ == port){
                userInfo.duration_ = sumDuration;
                userInfo.disconnectTime_ = lastConnection;
                break;
            }
        }
    }
}

void Server::InterfaceClientSession::WriteToDB(const InterfaceClientSession& client, Server& server) {
    std::string username = client.GetUserNameIn();
    auto it = server.users.find(username);

    if (it != server.users.end()) {
        for (const auto& userInfo : it->second) {
            server.writeToDatabase(userInfo);
        }
        server.clearUser(username);
    }
}

std::string Server::InterfaceClientSession::GetDayNow() {
    std::chrono::system_clock::time_point lastRequestTime = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(lastRequestTime);
    char buft[80];
    std::strftime(buft, sizeof(buft), "%Y-%m-%d", std::localtime(&t));
    return std::string{buft};
}

std::string Server::InterfaceClientSession::GetConnectionTime() {
    std::chrono::system_clock::time_point timeConnection = GetFirstConnectionTime();
    std::time_t t = std::chrono::system_clock::to_time_t(timeConnection);
    char buft[80];
    std::strftime(buft, sizeof(buft), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return std::string{buft};
}

std::string Server::InterfaceClientSession::GetDisconnectionTime() {
    std::chrono::system_clock::time_point timeConnection = GetLastDisconnectionTime();
    std::time_t t = std::chrono::system_clock::to_time_t(timeConnection);
    char buft[80];
    std::strftime(buft, sizeof(buft), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return std::string{buft};
}


void Server::InterfaceClientSession::sha256_transform(SHA256_CTX *ctx, const BYTE *data) {
    WORDL a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];

    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
    for ( ; i < 64; ++i)
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e,f,g) + k[i] + m[i];
        t2 = EP0(a) + MAJ(a,b,c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

void Server::InterfaceClientSession::sha256_init(Server::InterfaceClientSession::SHA256_CTX *ctx) {
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

void Server::InterfaceClientSession::sha256_update(Server::InterfaceClientSession::SHA256_CTX *ctx, const BYTE *data,
                                                   size_t len) {
    WORDL i;

    for (i = 0; i < len; ++i) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

void Server::InterfaceClientSession::sha256_final(Server::InterfaceClientSession::SHA256_CTX *ctx, BYTE *hash) {
    WORDL i;

    i = ctx->datalen;

    // Pad whatever data is left in the buffer.
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56)
            ctx->data[i++] = 0x00;
    }
    else {
        ctx->data[i++] = 0x80;
        while (i < 64)
            ctx->data[i++] = 0x00;
        sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }

    // Append to the padding the total message's length in bits and transform.
    ctx->bitlen += ctx->datalen * 8;
    ctx->data[63] = ctx->bitlen;
    ctx->data[62] = ctx->bitlen >> 8;
    ctx->data[61] = ctx->bitlen >> 16;
    ctx->data[60] = ctx->bitlen >> 24;
    ctx->data[59] = ctx->bitlen >> 32;
    ctx->data[58] = ctx->bitlen >> 40;
    ctx->data[57] = ctx->bitlen >> 48;
    ctx->data[56] = ctx->bitlen >> 56;
    sha256_transform(ctx, ctx->data);

    // Since this implementation uses little endian byte ordering and SHA uses big endian,
    // reverse all the bytes when copying the final state to the output hash.
    for (i = 0; i < 4; ++i) {
        hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0x000000ff;
    }
}

bool Server::InterfaceClientSession::checkHash(const std::string &content, const std::string &receivedHash) {
    const int SHA256_DIGEST_LENGTH = 32;

    SHA256_CTX sha256;
    sha256_init(&sha256);

    sha256_update(&sha256, reinterpret_cast<const BYTE*>(content.c_str()), content.length());

    BYTE computedHash[SHA256_DIGEST_LENGTH];
    sha256_final(&sha256, computedHash);


    std::string computedHashStr;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        computedHashStr += static_cast<char>(computedHash[i]);
    }
#ifdef DEBUGLOG
    std::cout   << "computedHashStr :" << computedHashStr << "\n"
                << "receivedHash: " << receivedHash <<std::endl;
#endif

    return (computedHashStr == receivedHash);
}

void Server::InterfaceClientSession::HandleData(DataBuffer_t &data, Server &server) {
    auto it = data.begin();
    uint16_t code_sequence = NetworkThreadPool::Extract<uint16_t>(it);
    MessageType act = NetworkThreadPool::Extract<MessageType>(it);

    std::string user_name;
    std::string pass_word;

    if (act == MessageType::Registered) {
        user_name = NetworkThreadPool::ExtractString(it);
        pass_word = NetworkThreadPool::ExtractString(it);
        if (server.IsUserRegistered(user_name)) {
            act = MessageType::Authorize;
        }
    }

    switch (act) {
        case MessageType::Registered: {
            if (user_name.empty() && pass_word.empty()) {
                user_name = NetworkThreadPool::ExtractString(it);
                pass_word = NetworkThreadPool::ExtractString(it);
            }
            std::string hash = NetworkThreadPool::ExtractString(it);
            if (!checkHash(user_name + pass_word, hash)) {
#ifdef DEBUGLOG
                std::cerr << "Hash mismatch\n";
#endif
                return;
            }
            server.HandleRegisterUser(*this, code_sequence, user_name, pass_word);
            return;
        }
        case MessageType::Authorize: {
            if (user_name.empty() && pass_word.empty()) {
                user_name = NetworkThreadPool::ExtractString(it);
                pass_word = NetworkThreadPool::ExtractString(it);
            }
            server.HandleAuthorize(*this, code_sequence, user_name, pass_word);
            return;
        }
        case MessageType::SendingTo: {
            std::string recipient_username = NetworkThreadPool::ExtractString(it);
            std::string message = NetworkThreadPool::ExtractString(it);
            server.HandleSendTo(*this, code_sequence, recipient_username, message);
            return;
        }
        default:
            std::cerr << "Unknown MessageType received: " << static_cast<int>(act) << '\n';
            return;
    }
}



ServerKeepAliveConfig::ServerKeepAliveConfig(KeepAliveProperty_t idle,
                                               KeepAliveProperty_t interval,
                                               KeepAliveProperty_t count
                                               )
                                               :    idle_(idle),
                                                    interval_(interval),
                                                    count_(count)
                                               {}

Server::UserInfo::UserInfo(std::string username,
                           std::string pass,
                           uint16_t port,
                           std::string connectTime,
                           std::string disconnectTime,
                           std::string duration,
                           std::string timeToday)
        : username_(std::move(username)),
          password_(std::move(pass)),
          sessionPort_(port),
          connectTime_(std::move(connectTime)),
          disconnectTime_(std::move(disconnectTime)),
          duration_(std::move(duration)),
          timeToday_(std::move(timeToday))
{}

bool Server::UserLoginInfo::Comparator::operator()(const Server::UserLoginInfo &lhs, const Server::UserLoginInfo &rhs) const {
    return std::less<std::string>{}(lhs.username_, rhs.username_);
}

bool Server::UserLoginInfo::Comparator::operator()(const Server::UserLoginInfo &lhs, const std::string &rhs) const {
    return std::less<std::string>{}(lhs.username_, rhs);
}

bool Server::UserLoginInfo::Comparator::operator()(const std::string &lhs, const Server::UserLoginInfo &rhs) const {
    return std::less<std::string>{}(lhs, rhs.username_);
}

bool Server::ClientSessionComparator::operator()(const std::unique_ptr<InterfaceClientSession> &lhs,
                                                 const std::unique_ptr<InterfaceClientSession> &rhs) const {
    return  (uint64_t(lhs->GetHost()) | uint64_t(lhs->GetPort()) << 32) <
            (uint64_t(rhs->GetHost()) | uint64_t(rhs->GetPort()) << 32);
}

bool Server::ClientSessionComparator::operator()(const std::unique_ptr<InterfaceClientSession> &lhs,
                                                 const Server::ConnectionInfo &rhs) const {
    return  (uint64_t(lhs->GetHost()) | uint64_t(lhs->GetPort()) << 32) <
            (uint64_t(rhs.host) | uint64_t(rhs.port) << 32);
}

bool Server::ClientSessionComparator::operator()(const Server::ConnectionInfo &lhs,
                                                 const std::unique_ptr<InterfaceClientSession> &rhs) const {
    return  (uint64_t(lhs.host) | uint64_t(lhs.port) << 32) <
            (uint64_t(rhs->GetHost()) | uint64_t(rhs->GetPort()) << 32);
}

Server::UserLoginInfo* Server::UserManager::Registred(std::string name, std::string pass) {
    std::lock_guard lockGuard(sharedMutex);
    auto info = userLoginInfoSet.emplace(UserLoginInfo{std::move(name), std::move(pass)});
    if(info.second) {
        return const_cast<UserLoginInfo*>(&*info.first);
    } else {
        return nullptr;
    }
}

Server::UserLoginInfo* Server::UserManager::Authorize(UserLoginInfo name, std::string pass) {
    std::lock_guard lockGuard(sharedMutex);
    auto iterator = userLoginInfoSet.find(name);
    if(iterator != userLoginInfoSet.cend()){
        if(iterator -> password_ == pass) {
            return const_cast<UserLoginInfo*>(&*iterator);
        } else {
            return nullptr;
        }
    }
    return nullptr;
}

Server::UserLoginInfo *Server::UserManager::Find(UserLoginInfo name) {
    std::lock_guard lockGuard(sharedMutex);
    auto iterator = userLoginInfoSet.find(name);
    if (iterator != userLoginInfoSet.cend()) {
        return const_cast<UserLoginInfo *>(&*iterator);
    } else {
        return nullptr;
    }
}

bool Server::SessionManager::SessionComparator::operator()(const Server::ConnectionStatus &lhs,
                                                           const Server::ConnectionStatus &rhs) const {
    return (uint64_t(lhs.socket->GetHost()) | uint64_t(lhs.socket->GetPort()) << 32) <
           (uint64_t(rhs.socket->GetHost()) | uint64_t(rhs.socket->GetPort()) << 32);
}

bool Server::SessionManager::SessionComparator::operator()(const Server::ConnectionStatus &lhs,
                                                           const Server::ConnectionInfo &rhs) const {
    return (uint64_t(lhs.socket->GetHost()) | uint64_t(lhs.socket->GetPort()) << 32) <
            (uint64_t(rhs.host) | uint64_t(rhs.port) << 32);
}

bool Server::SessionManager::SessionComparator::operator()(const Server::ConnectionInfo &lhs,
                                                           const Server::ConnectionStatus &rhs) const {
    return  (uint64_t(lhs.host) | uint64_t(lhs.port) << 32) <
            (uint64_t(rhs.socket->GetHost()) | uint64_t(rhs.socket->GetPort()) << 32);
}

bool Server::SessionManager::UserComparator::operator()(
        const Server::SessionManager::SessionConnectionIterator &lhs,
        const Server::SessionManager::SessionConnectionIterator &rhs) const {
    return lhs->userCredentials < rhs->userCredentials;
}

bool Server::SessionManager::UserComparator::operator()(
        const Server::SessionManager::SessionConnectionIterator &lhs, Server::UserLoginInfo *rhs) const {
    return lhs->userCredentials < rhs;
}

bool Server::SessionManager::UserComparator::operator()(Server::UserLoginInfo *lhs,
                                                        const Server::SessionManager::SessionConnectionIterator &rhs) const {
    return lhs < rhs->userCredentials;
}

bool Server::SessionManager::AddSession(TCPInterfaceBase *socket) {
    std::lock_guard lockGuard(idSessionIdentifierMutex);
    return idSessionIdentifier.emplace(ConnectionStatus{ConnectionStatus::Status::NotAuthorized, socket}).second;
}

bool Server::SessionManager::DeleteSession(TCPInterfaceBase *socket) {
    std::lock_guard lock(idSessionIdentifierMutex);
    auto sessionIterator = idSessionIdentifier.find(ConnectionInfo{.host = socket->GetHost(), .port = socket->GetPort()});
    if(sessionIterator == idSessionIdentifier.end()) return false;
    idSessionIdentifier.erase(sessionIterator);
    return true;
}

std::string Server::SessionManager::GetUsername(TCPInterfaceBase *socket) {
    idSessionIdentifierMutex.lock_shared();
    auto sessionIterator = idSessionIdentifier.find(ConnectionInfo{.host = socket->GetHost(), .port = socket->GetPort()});
    idSessionIdentifierMutex.unlock_shared();
    if(sessionIterator == idSessionIdentifier.end()) return "";
    if(!sessionIterator->userCredentials) return "";
    return sessionIterator->userCredentials->username_;
}

Server::ConnectionStatus::Status Server::SessionManager::GetStatus(TCPInterfaceBase *socket) const {
    idSessionIdentifierMutex.lock_shared();
    auto session_it = idSessionIdentifier.find(ConnectionInfo{.host = socket->GetHost(), .port = socket->GetPort()});
    idSessionIdentifierMutex.unlock_shared();
    if(session_it == idSessionIdentifier.end()) return ConnectionStatus::Status::ErrorInvalid;
    return session_it->status;
}

Server::UserManager Server::userManager;

bool Server::SessionManager::RegistredUser(TCPInterfaceBase *socket, std::string username, std::string password) {
    std::shared_lock lock(idSessionIdentifierMutex);
    auto sessionIterator = idSessionIdentifier.find(ConnectionInfo{.host = socket->GetHost(), .port = socket->GetPort()});
    if (sessionIterator == idSessionIdentifier.end()) {
        return false;
    }
    lock.unlock();

    UserLoginInfo* newUserList = userManager.Registred(std::move(username), std::move(password));
    if (!newUserList) {
        return false;
    }

    {
        std::unique_lock lock(userIdentifierMutex);
        if (sessionIterator->userCredentials) {
            auto range = userIdentifier.equal_range(sessionIterator);
            for (auto it = range.first; it != range.second; ++it) {
                if (*it == sessionIterator) {
                    userIdentifier.erase(it);
                    break;
                }
            }
        }
        const_cast<UserLoginInfo*&>(sessionIterator->userCredentials) = newUserList;
        const_cast<ConnectionStatus::Status&>(sessionIterator->status) = ConnectionStatus::Status::Authorized;
        userIdentifier.emplace(sessionIterator);
    }
    return true;
}

bool Server::SessionManager::AuthorizeUser(TCPInterfaceBase *socket, std::string username, std::string password) {
    std::shared_lock idLock(idSessionIdentifierMutex);
    auto sessionIterator = idSessionIdentifier.find(ConnectionInfo{.host = socket->GetHost(), .port = socket->GetPort()});
    if (sessionIterator == idSessionIdentifier.end()) {
        return false;
    }
    idLock.unlock();

    UserLoginInfo userInfo{username, password};
    UserLoginInfo* authorizedUser = userManager.Authorize(userInfo, password);
    if (!authorizedUser) {
        return false;
    }

    {
        std::unique_lock userLock(userIdentifierMutex);
        if (sessionIterator->userCredentials) {
            auto range = userIdentifier.equal_range(sessionIterator);
            for (auto it = range.first; it != range.second; ++it) {
                if (*it == sessionIterator) {
                    userIdentifier.erase(it);
                    break;
                }
            }
        }

        const_cast<UserLoginInfo*&>(sessionIterator->userCredentials) = authorizedUser;
        const_cast<ConnectionStatus::Status&>(sessionIterator->status) = ConnectionStatus::Status::Authorized;
        userIdentifier.emplace(sessionIterator);
    }

    return true;
}

bool Server::SessionManager::Logout(TCPInterfaceBase *socket) {
    idSessionIdentifierMutex.lock_shared();
    auto sessionIterator = idSessionIdentifier.find(ConnectionInfo{.host = socket->GetHost(), .port = socket->GetPort()});
    idSessionIdentifierMutex.unlock_shared();

    if (sessionIterator == idSessionIdentifier.end()) {
        return false;
    }

    {
        std::unique_lock userLock(userIdentifierMutex);
        if (sessionIterator->userCredentials) {
            auto range = userIdentifier.equal_range(sessionIterator);
            for (auto it = range.first; it != range.second; ++it) {
                if (*it == sessionIterator) {
                    userIdentifier.erase(it);
                    break;
                }
            }
        } else {
            return false;
        }
    }

    const_cast<UserLoginInfo*&>(sessionIterator->userCredentials) = nullptr;
    const_cast<ConnectionStatus::Status&>(sessionIterator->status) = ConnectionStatus::Status::NotAuthorized;
    return true;
}

std::list<TCPInterfaceBase *> Server::SessionManager::findSocketListByUsername(std::string username) {
    std::list<TCPInterfaceBase*> socket_list;
    std::shared_lock lock(userIdentifierMutex);
    UserLoginInfo userLoginInfo = {std::move(username), ""};
    UserLoginInfo* user = userManager.Find(userLoginInfo);
    if(!user) {
        return {};
    }
    for (const auto& sessionIt : userIdentifier) {
        if (sessionIt->userCredentials == user) {
            socket_list.emplace_back(sessionIt->socket);
        }
    }
    return socket_list;
}
