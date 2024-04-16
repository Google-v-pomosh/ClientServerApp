#include "../inc/header.h"

#include <cstdio>
#include <cstring>
#include <iostream>

#ifdef _WIN32
#define WIN(exp) exp
#define NIX(exp)
#define WINIX(win_exp, nix_exp) win_exp

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
#define WINIX(win_exp, nix_exp) nix_exp
#endif

Client::Client() noexcept
    :   client_status_(status::disconnected)
    {}

Client::Client(ThreadPool *threadPool) noexcept
    :   threadManagmentType(ThreadManagementType::thread_pool),
        threadClient(threadPool),
        client_status_(status::disconnected)
    {}

Client::~Client(){
    disconnect();
    WIN(WSACleanup();)
    switch (threadManagmentType) {
        case Client::ThreadManagementType::single_thread:
            if (threadClient.thread) {
                threadClient.thread->join();
            }
            delete threadClient.thread;
        break;
        case Client::ThreadManagementType::thread_pool:
        break;
    }
}

Client::status Client::disconnect() noexcept {
    if (client_status_ != status::connected){
        return client_status_;
    }
    client_status_ = status::disconnected;
    switch (threadManagmentType) {
        case Client::ThreadManagementType::single_thread:
            if(threadClient.thread){
                threadClient.thread->join();
            }
            delete threadClient.thread;
        break;
        case Client::ThreadManagementType::thread_pool:
        break;
    }
    shutdown(client_socket_, SD_BOTH);
    WINIX(closesocket(client_socket_), close(client_socket_));
    return client_status_;
}

void Client::handleSingleThread() {
    try {
        while (client_status_ == status::connected) {
            if (DataBuffer dataBuffer = loadData(); !dataBuffer.empty()) {
                std::lock_guard lockGuard(handle_mutex_);
                function_handler(std::move(dataBuffer));
            } else if (client_status_ != status::connected) {
                return;
            }
        }
    } catch (std::exception& exception) {
        std::cerr << exception.what() << std::endl;
        return;
    }
}

void Client::handleThreadPool() {
    try {
        if (DataBuffer dataBuffer = loadData(); !dataBuffer.empty()) {
            std::lock_guard lockGuard(handle_mutex_);
            function_handler(std::move(dataBuffer));
        }
        if (client_status_ == status::connected) {
            threadClient.threadPool->addJob([this]{handleThreadPool();});
        }
    } catch (std::exception& exception) {
        std::cerr << exception.what() << std::endl;
        return;
    } catch (...) {
        std::cerr << "Unhandled exception!" << std::endl;
        return;
    }
}

ClientBase::status Client::connectTo(uint32_t host, uint16_t port) noexcept {

#ifdef _WIN32
    WinSocket winsock_initer;
    /*if (WSAStartup(MAKEWORD(2, 2), &w_data) != 0)
    {};*/
    if ((client_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) == INVALID_SOCKET) {
        return  client_status_ = status::err_socket_init;
    }
#else
    if ((client_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0) {
        return  client_status_ = status::err_socket_init;
    }
#endif
    new(&address_) SocketAddr_in;
    address_.sin_family = AF_INET;
    address_.sin_addr.s_addr = host;

#ifdef _WIN32
    address_.sin_addr.S_un.S_addr = host;
#else
    address.sin_addr.s_addr = host;
#endif

    address_.sin_port = htons(port);

    if (connect(client_socket_, (sockaddr*)&address_, sizeof (address_)) WINIX(== SOCKET_ERROR,!= 0)) {
        WIN(closesocket)NIX(close)(client_socket_);
        return client_status_ = status::err_socket_connect;
    }
    return client_status_ = status::connected;
}

DataBuffer Client::loadData() {
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

DataBuffer Client::loadDataSync() {
    DataBuffer dataBuffer;
    uint32_t size = 0;
    int answer = recv(client_socket_, reinterpret_cast<char*>(&size), sizeof(size), 0);
    if(size && answer == sizeof(size)){
        dataBuffer.resize(size);
        recv(client_socket_, reinterpret_cast<char *>(dataBuffer.data()), dataBuffer.size(), 0);
    }
    return dataBuffer;
}

void Client::setHandler(Client::function_handler_typedef handler) {
    {
        std::lock_guard lockGuard(handle_mutex_);
        function_handler = handler;
    }
    switch (threadManagmentType) {
        case Client::ThreadManagementType::single_thread:
            if(threadClient.thread){
                return;
            }
            threadClient.thread = new std::thread(&Client::handleSingleThread, this);
        break;
        case Client::ThreadManagementType::thread_pool:
            threadClient.threadPool->addJob([this]{handleThreadPool();});
        break;
    }
}

/*void Client::joinHandler() {
    switch (threadManagmentType) {
        case Client::ThreadManagementType::single_thread:
            if (threadClient.thread){
                threadClient.thread->join();
            }
        break;
        case Client::ThreadManagementType::thread_pool:
            threadClient.threadPool->join();
        break;
    }
}*/

void Client::joinHandler() const {
    switch (threadManagmentType) {
        case Client::ThreadManagementType::single_thread:
            if (threadClient.thread) {
                threadClient.thread->join();
            }
            break;
        case Client::ThreadManagementType::thread_pool:
            if (threadClient.threadPool) {
                threadClient.threadPool->join();
            }
            break;
        default:
            std::cerr << "Invalid ThreadManagementType\n";
            break;
    }
}


/*bool Client::sendData(const void *buffer, const size_t size) const {
    void* sendBuffer = malloc(size + sizeof(int));
    memcpy(reinterpret_cast<int*>(sendBuffer)+ sizeof(int), buffer, size);
    *reinterpret_cast<int*>(sendBuffer) = size;
    if (send(client_socket_, reinterpret_cast<char*>(sendBuffer), size + sizeof (int), 0) < 0 ) {
        return false;
    }
    free(sendBuffer);
    return true;
}*/
/*bool Client::sendData(const void *buffer, const size_t size) const {
    // Выделяем буфер для отправки данных, включая место для хранения размера данных
    void* sendBuffer = malloc(size + sizeof(int));
    if (sendBuffer == nullptr) {
        return false; // Ошибка при выделении памяти
    }

    // Записываем размер данных в начало буфера
    int* sizePtr = reinterpret_cast<int*>(sendBuffer);
    *sizePtr = static_cast<int>(size);

    // Копируем данные в буфер отправки, начиная сразу после размера
    memcpy(static_cast<char*>(sendBuffer) + sizeof(int), buffer, size);

    // Отправляем данные
    int bytesSent = send(client_socket_, reinterpret_cast<char*>(sendBuffer), size + sizeof(int), 0);
    free(sendBuffer); // Освобождаем выделенную память

    // Проверяем успешность отправки данных
    if (bytesSent == SOCKET_ERROR || static_cast<size_t>(bytesSent) != size + sizeof(int)) {
        return false; // Ошибка при отправке данных или не все данные были отправлены
    }

    return true; // Данные успешно отправлены
}*/
bool Client::sendData(const void *buffer, const size_t size) const {
    std::vector<char> sendBuffer(size + sizeof(int));
    *reinterpret_cast<int*>(sendBuffer.data()) = static_cast<int>(size);
    memcpy(sendBuffer.data() + sizeof(int), buffer, size);
#ifdef _WIN32
    int bytesSent = send(client_socket_, sendBuffer.data(), size + sizeof(int), 0);
    if (bytesSent == SOCKET_ERROR || static_cast<size_t>(bytesSent) != size + sizeof(int)) {
        return false;
    }
#else
    ssize_t bytesSent = send(client_socket_, sendBuffer.data(), size + sizeof(int), 0);
    if (bytesSent == -1 || static_cast<size_t>(bytesSent) != size + sizeof(int)) {
        return false;
    }
#endif
    return true;
}



uint32_t Client::getHost() const {
    return
            NIX(
                    address_.sin_addr.s_addr
                    )
            WIN(
                    address_.sin_addr.S_un.S_addr
                    );
}

uint16_t Client::getPort() const {
    return address_.sin_port;
}