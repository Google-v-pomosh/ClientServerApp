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
                     KeepAliveConfig keep_alive_config,
                     function_handler_typedef handler,
                     function_handler_typedef_connect connect_handle,
                     function_handler_typedef_connect disconnect_handle,
                     uint32_t thread_count
)
        : port_(port),
          handler_(handler),
          connect_handle_(connect_handle),
          disconnect_handle_(disconnect_handle),
          thread_pool_(thread_count),
          keep_alive_config_(keep_alive_config)
{}

Server::~Server() {
    if(server_status_ == Status::up) {
        stopServer();
    }
}

void Server::stopServer() {
    thread_pool_.restartJob();
    server_status_ = Status::close;
    WIN(closesocket)NIX(close)(socket_server);
    client_list_.clear();
}

void Server::setServerHandler(Server::function_handler_typedef handler) {
    this->handler_ = handler;
}

uint16_t Server::setServerPort(const uint16_t port) {
    this->port_ = port;
    startServer();
    return port;
}

Server::Status Server::startServer() {
    if(server_status_ == Status::up) {
        stopServer();
    }


    SocketAddr_in address;

#ifdef _WIN32
    address.sin_addr.S_un.S_addr = INADDR_ANY;
#else
    address.sin_addr.s_addr = INADDR_ANY;
#endif

    address.sin_port = htons(port_);
    address.sin_family = AF_INET;

#ifdef _WIN32
    if ((socket_server = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        return server_status_ = Status::err_socket_init;
    }
    unsigned long mode = 0;
    if (ioctlsocket(socket_server, FIONBIO, &mode) == SOCKET_ERROR) {
        return server_status_ = Status::err_socket_init;
    }
#else
    if ((socket_server = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1) {
        return server_status_ = Status::err_socket_init;
    }
#endif

    int flag = 1;
    if (setsockopt(socket_server, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(flag)) == -1) {
        return server_status_ = Status::err_socket_init;
    }

    if (bind(socket_server, (struct sockaddr*)&address, sizeof(address)) < 0) {
        return server_status_ = Status::err_socket_bind;
    }

    if (listen(socket_server, SOMAXCONN) < 0) {
        return server_status_ = Status::err_socket_listening;
    }

    server_status_ = Status::up;
    thread_pool_.addJob([this]{handlingAcceptLoop();});
    thread_pool_.addJob([this]{waitingDataLoop();});

    return server_status_;
}


bool Server::ServerConnectTo(uint32_t host, uint16_t port, Server::function_handler_typedef_connect connect_handle) {
    Socket client_socket;
    SocketAddr_in address;

#ifdef _WIN32
    if ((client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) == INVALID_SOCKET) {
        return false;
    }
#else
    if ((client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0) {
        return false;
    }
#endif

    new(&address) SocketAddr_in;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = host;

#ifdef _WIN32
    address.sin_addr.S_un.S_addr = host;
#else
    address.sin_addr.s_addr = host;
#endif

    address.sin_port = htons(port);

#ifdef _WIN32
    if(connect(client_socket, (sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        closesocket(client_socket);
        return false;
    }
    if(!enableKeepAlive(client_socket)) {
        shutdown(client_socket, 0);
        closesocket(client_socket);
    }
#else
    if(connect(client_socket, (sockaddr *)&address, sizeof(address)) != 0) {
        close(client_socket);
        return false;
    }
    if(!enableKeepAlive(client_socket)) {
        shutdown(client_socket, 0);
        close(client_socket);
    }
#endif

    std::unique_ptr<Client> client(new Client(client_socket, address));
    connect_handle(*client);

    client_mutex_.lock();
    client_list_.emplace_back(std::move(client));
    client_mutex_.unlock();

    return true;
}

void Server::ServerSendData(const void *buffer, const size_t size) {
    for (std::unique_ptr<Client>& client : client_list_) {
        client ->sendData(buffer, size);
    }
}

bool Server::ServerSendDataBy(uint32_t host, uint16_t port, const void *buffer, const size_t size) {
    bool dataIsSended = false;
    for(std::unique_ptr<Client>& client : client_list_) {
        if (client->getHost() == host && client->getPort() == port){
            client->sendData(buffer, size);
            dataIsSended = true;
        }
    }
    return dataIsSended;
}

bool Server::ServerDisconnectBy(uint32_t host, uint16_t port) {
    bool clientIsDisconnected = false;
    for(std::unique_ptr<Client>& client : client_list_) {
        if (client->getHost() == host && client->getPort() == port){
            client->disconnect();
            clientIsDisconnected = true;
        }
    }
    return clientIsDisconnected;
}

void Server::ServerDisconnectAll() {
    for (std::unique_ptr<Client>& client : client_list_) {
        client->disconnect();
    }
}

void Server::handlingAcceptLoop() {
    SockLen_t addrLen = sizeof(SocketAddr_in);
    SocketAddr_in clientAddr;
#ifdef _WIN32
    if(Socket clientSocket = accept(socket_server, (struct sockaddr*)&clientAddr, &addrLen);
              clientSocket != 0 && server_status_ == Status::up)
    {
        if (enableKeepAlive(clientSocket))
        {
            std::unique_ptr<Client> client(new Client(clientSocket, clientAddr));
            connect_handle_(*client);
            client_mutex_.lock();
            client_list_.emplace_back(std::move(client));
            client_mutex_.unlock();
        } else {
            shutdown(clientSocket, 0);
            closesocket(clientSocket);
        }
    }
#else
    if (Socket clientSocket = accept4(socket_server, (struct sockaddr*)&clientAddr, &addrLen, SOCK_NONBLOCK); clientSocket >= 0 && server_status_ == Status::up) {
        if(enableKeepAlive(clientSocket)) {
            std::unique_ptr<Client> client(new Client(clientSocket, clientAddr));
            connect_handle_(*client);
            client_mutex_.lock();
            client_list_.emplace_back(std::move(client));
            client_mutex_.unlock();
        } else {
            shutdown(clientSocket, 0);
            close(clientSocket);
        }
    }
#endif
    if(server_status_ == Status::up) {
        thread_pool_.addJob([this]() { handlingAcceptLoop(); });
    }
}

bool Server::enableKeepAlive(Socket socket) {
    int flag = 1;
#ifdef _WIN32
    tcp_keepalive keepAlive {1,
                             keep_alive_config_.ka_idle * 1000,
                             keep_alive_config_.ka_intvl * 1000};
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
                 0,
                 nullptr) != 0) {
        return false;
    }
#else
    if(setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)) == -1) {
        return false;
    }
    if(setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE, &keep_alive_config_.ka_idle, sizeof(keep_alive_config_.ka_idle)) == -1) {
        return false;
    }
    if(setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &keep_alive_config_.ka_intvl, sizeof(keep_alive_config_.ka_intvl)) == -1) {
        return false;
    }
    if(setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &keep_alive_config_.ka_cnt, sizeof(keep_alive_config_.ka_cnt)) == -1) {
        return false;
    }
#endif
    return true;
}

void Server::waitingDataLoop() {
    {
        std::lock_guard lockGuard(client_mutex_);
        for (auto begin = client_list_.begin(), end = client_list_.end(); begin != end; ++begin) {
            auto &client = *begin;
            if (client) {
                if (DataBuffer dataBuffer = client->loadData(); !dataBuffer.empty()) {
                    thread_pool_.addJob([this, data = std::move(dataBuffer), &client] {
                        client->access_mutex_.lock();
                        handler_(std::move(data), *client);
                        client->access_mutex_.unlock();
                    });
                } else if (client->client_status_ == SocketStatus::disconnected) {
                    thread_pool_.addJob([this, &client, begin] {
                        client->access_mutex_.lock();
                        Client *pointer = client.release();
                        client = nullptr;
                        pointer->access_mutex_.unlock();
                        disconnect_handle_(*pointer);
                        client_list_.erase(begin);
                        delete pointer;

                    });
                }
            }
        }
    }
    if (server_status_ == Status::up) {
        thread_pool_.addJob([this](){waitingDataLoop();});
    }
}


Server::Client::Client(Socket socket, SocketAddr_in address)
        : address_(address), client_socket_(socket) {}


Server::Client::~Client(){
#ifdef _WIN32
    if(client_socket_ == INVALID_SOCKET) {
        return;
    }
    shutdown(client_socket_, SD_BOTH);
    closesocket(client_socket_);
#else
    if(client_socket_ == -1) {
        return;
    }
    shutdown(client_socket_, SD_BOTH);
    close(client_socket_);
#endif
}

ClientBase::status Server::Client::disconnect() {
    client_status_ = status::disconnected;
#ifdef _WIN32
    if (client_socket_ == INVALID_SOCKET) {
        return client_status_;
    }
    shutdown(client_socket_, SD_BOTH);
    closesocket(client_socket_);
    client_socket_ = INVALID_SOCKET;
#else
    if (client_socket_ == -1) {
        return client_status_;
    }
    shutdown(client_socket_, SD_BOTH);
    close(client_socket_);
    client_socket_ = -1;
#endif
    return client_status_;
}

bool Server::Client::sendData(const void *buffer, const size_t size) const {
    if(client_status_ != SocketStatus::connected) {
        return false;
    }
    void* sendBuffer = malloc(size + sizeof (uint32_t));
    memcpy(reinterpret_cast<char*>(sendBuffer) + sizeof (uint32_t ), buffer, size);
    *reinterpret_cast<uint32_t*>(sendBuffer) = size;
    if(send(client_socket_, reinterpret_cast<char*>(sendBuffer), size + sizeof (int), 0) < 0){
        return false;
    }
    free(sendBuffer);
    return true;
}

DataBuffer Server::Client::loadData() {
    if (client_status_ != SocketStatus::connected) {
        return DataBuffer();
    }
    DataBuffer dataBuffer;
    uint32_t size;
    int error = 0;
#ifdef _WIN32
    if (u_long t = true; SOCKET_ERROR == ioctlsocket(client_socket_, FIONBIO, &t)) {
        return DataBuffer();
    }
    int answer = recv(client_socket_, (char *)&size, sizeof(size), 0);
    if (u_long t = false; SOCKET_ERROR == ioctlsocket(client_socket_, FIONBIO, &t)){
        return DataBuffer();
    }
#else
    int answer = recv(client_socket_, (char *)&size, sizeof(size), MSG_DONTWAIT);
#endif
    if (!answer) {
        disconnect();
        return DataBuffer();
    } else if (answer == -1) {
        WIN (
                error = convertError();
                if (!error) {
                    SockLen_t length = sizeof (error);
                    getsockopt(client_socket_, SOL_SOCKET, SO_ERROR, WIN((char*))&error, &length);
                }
        )
        NIX (
                SockLen_t length = sizeof (error);
                getsockopt(client_socket_, SOL_SOCKET, SO_ERROR, WIN((char*))&error, &length);
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
            disconnect();
            [[fallthrough]];
        case EAGAIN:
            return DataBuffer ();
        default:
            disconnect();
            std::cerr << "Unhandled error!\n"
                      << "Code: " << error << " Error: " << std::strerror(error) << '\n';
            return DataBuffer();
    }

    if (!size) {
        return DataBuffer();
    }

    dataBuffer.resize(size);
    int recvResult = recv(client_socket_, reinterpret_cast<char*>(dataBuffer.data()), dataBuffer.size(), 0);
    if (recvResult < 0) {
        int err = errno;
        std::cerr << "Error receiving data: " << std::strerror(err) << '\n';
        disconnect();
        return DataBuffer();
    }

    return dataBuffer;
}

uint32_t Server::Client::getHost() const {
    return
            WIN (
                    address_.sin_addr.S_un.S_addr
            )
            NIX (
                    address_.sin_addr.s_addr
            );
}

uint16_t Server::Client::getPort() const {
    return address_.sin_port;
}
