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

sqlite3* dbConnection;

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

    std::unique_ptr<InterfaceClientSession> client(new InterfaceClientSession(clientSocket, address));
    connect_handle(*client);

    m_clientMutex_.lock();
    m_session_list_.emplace_back(std::move(client));
    m_clientMutex_.unlock();

    return true;
}

void Server::ServerSendData(const void *buffer, const size_t size) {
    for (std::unique_ptr<InterfaceClientSession>& client : m_session_list_) {
        client ->SendData(buffer, size);
    }
}

bool Server::ServerSendDataBy(uint32_t host, uint16_t port, const void *buffer, const size_t size) {
    bool dataIsSended = false;
    for(std::unique_ptr<InterfaceClientSession>& client : m_session_list_) {
        if (client->GetHost() == host && client->GetPort() == port){
            client->SendData(buffer, size);
            dataIsSended = true;
        }
    }
    return dataIsSended;
}

bool Server::ServerDisconnectBy(uint32_t host, uint16_t port) {
    bool clientIsDisconnected = false;
    for(std::unique_ptr<InterfaceClientSession>& client : m_session_list_) {
        if (client->GetHost() == host && client->GetPort() == port){
            client->Disconnect();
            clientIsDisconnected = true;
        }
    }
    return clientIsDisconnected;
}

void Server::ServerDisconnectAll() {
    for (std::unique_ptr<InterfaceClientSession>& client : m_session_list_) {
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
            std::unique_ptr<InterfaceClientSession> client(new InterfaceClientSession(clientSocket, clientAddr));
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
                        InterfaceClientSession *pointer = client.release();
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

    rc = sqlite3_open("example.db", &dbConnection);
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


Server::InterfaceClientSession::InterfaceClientSession(SocketHandle_t socket, SocketAddressIn_t address)
        : m_address_(address), m_socketDescriptor_(socket){
    SetFirstConnectionTime();
}


Server::InterfaceClientSession::~InterfaceClientSession(){
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



bool Server::InterfaceClientSession::AutentficateUserInfo(const DataBuffer_t& data,Server::InterfaceClientSession& client, Server& server) {
    std::string timeStr = client.GetDayNow();
    uint16_t port = client.GetPort();
    std::string connectionTime = client.GetConnectionTime();

    std::string receivedMessageAll(data.begin(), data.end());
    receivedMessageAll.erase(std::remove(receivedMessageAll.begin(), receivedMessageAll.end(), '\0'), receivedMessageAll.end());

    std::string receivedMessage = receivedMessageAll.substr(0);

    size_t colonPos = receivedMessage.find(':');
    if (colonPos == std::string::npos){
        /*std::cerr << "User don`t detected" << std::endl;*/
        return false;
    }
    std::string username = receivedMessage.substr(0, colonPos);
    client.username_ = username;
    std::string password = receivedMessage.substr(colonPos + 1);

    std::lock_guard<std::mutex> lock(server.usersMutex);

    auto it = server.users.find(username);
    if(it == server.users.end()){
        server.users.emplace(username, std::vector<UserInfo>{UserInfo(username,password,port,connectionTime,"", "", timeStr)});
        std::cout << "User '" << username << "' registered" << std::endl;
    }
    else {
        it->second.emplace_back(username,password,port,connectionTime,"", "", timeStr);
        std::cout << "User '" << username << "' authenticated successfully" << std::endl;
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

ServerKeepAliveConfig::ServerKeepAliveConfig(KeepAliveProperty_t idle,
                                               KeepAliveProperty_t interval,
                                               KeepAliveProperty_t count
                                               )
                                               :    idle_(idle),
                                                    interval_(interval),
                                                    count_(count)
                                               {}

Server::UserInfo::UserInfo(std::string username, std::string pass, uint16_t port, std::string connectTime,
                           std::string disconnectTime, std::string duration, std::string timeToday)
        : username_(std::move(username)),
          password_(std::move(pass)),
          sessionPort_(port),
          connectTime_(std::move(connectTime)),
          disconnectTime_(std::move(disconnectTime)),
          duration_(std::move(duration)),
          timeToday_(std::move(timeToday))
{}
