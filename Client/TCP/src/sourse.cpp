#include "../inc/header.h"

#include <cstring>
#include <iostream>
#include <utility>
#include <iomanip>
#ifdef _WIN32
#else
#include <ifaddrs.h>
#endif
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

Client::Client(std::string  username) noexcept
        : m_statusClient_(SockStatusInfo_t::Disconnected),
          m_addressClient_(),
          m_socketClient_(SOCKET_INVALID),
          m_threadManagmentType_(ThreadManagementType::SingleThread),

          username_(std::move(username))
{}


Client::Client(NetworkThreadPool *client_thread_pool, std::string  username) noexcept
        : m_threadManagmentType_(ThreadManagementType::ThreadPool),
          m_threadClient(client_thread_pool),
          m_statusClient_(SockStatusInfo_t::Disconnected),
          m_addressClient_(),
          m_socketClient_(SOCKET_INVALID),
          username_(std::move(username))
{}


Client::~Client(){
    Disconnect();
    WIN(WSACleanup();)
    JoinThread();
}

void Client::JoinThread() {
    switch (m_threadManagmentType_) {
        case Client::ThreadManagementType::SingleThread:
            if (m_threadClient.m_threadClient_) {
                m_threadClient.m_threadClient_->join();
            }
            delete m_threadClient.m_threadClient_;
            break;
        case Client::ThreadManagementType::ThreadPool:
            break;
    }
}

Client::SockStatusInfo_t Client::Disconnect() noexcept {
    if (m_statusClient_ != SockStatusInfo_t::Connected){
        return m_statusClient_;
    }
    m_statusClient_ = SockStatusInfo_t::Disconnected;
    switch (m_threadManagmentType_) {
        case Client::ThreadManagementType::SingleThread:
            if(m_threadClient.m_threadClient_){
                m_threadClient.m_threadClient_->join();
            }
            delete m_threadClient.m_threadClient_;
        break;
        case Client::ThreadManagementType::ThreadPool:
        break;
    }
    shutdown(m_socketClient_, SD_BOTH);
    WINIX(closesocket(m_socketClient_), close(m_socketClient_));
    return m_statusClient_;
}

void Client::HandleSingleThread() {
    try {
        while (m_statusClient_ == SockStatusInfo_t::Connected) {
            if (DataBuffer_t dataBuffer = LoadData(); !dataBuffer.empty()) {
                std::lock_guard lockGuard(m_handleMutex_);
                m_dataHandlerFunction(std::move(dataBuffer));
            }
        }
    } catch (std::exception& exception) {
        std::cerr << exception.what() << std::endl;
        return;
    }
}


void Client::HandleThreadPool() {
    try {
        if (DataBuffer_t dataBuffer = LoadData(); !dataBuffer.empty()) {
            std::lock_guard lockGuard(m_handleMutex_);
            m_dataHandlerFunction(std::move(dataBuffer));
        }
        if (m_statusClient_ == SockStatusInfo_t::Connected) {
            m_threadClient.m_threadPoolClient_->AddTask([this]{HandleThreadPool();});
        }
    } catch (std::exception& exception) {
        std::cerr << exception.what() << std::endl;
        return;
    } catch (...) {
        std::cerr << "Unhandled exception!" << std::endl;
        return;
    }
}

TCPInterfaceBase::SockStatusInfo_t Client::ConnectTo(uint32_t host, uint16_t port) noexcept {

#ifdef _WIN32
    WindowsSocketInitializer winsockInitializer;
    if ((m_socketClient_ = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) == INVALID_SOCKET) {
        return  m_statusClient_ = SockStatusInfo_t::InitError;
    }
#else
    if ((m_socketClient_ = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0) {
        return  m_statusClient_ = SockStatusInfo_t::InitError;
    }
#endif
    new(&m_addressClient_) SocketAddressIn_t;
    m_addressClient_.sin_family = AF_INET;
    m_addressClient_.sin_addr.s_addr = host;

#ifdef _WIN32
    m_addressClient_.sin_addr.S_un.S_addr = host;
#else
    m_addressClient_.sin_addr.s_addr = host;
#endif

    m_addressClient_.sin_port = htons(port);

    if (connect(m_socketClient_, (sockaddr*)&m_addressClient_, sizeof (m_addressClient_)) WINIX(== SOCKET_ERROR,!= 0)) {
        WIN(closesocket)NIX(close)(m_socketClient_);
        return m_statusClient_ = SockStatusInfo_t::ConnectError;
    }
    return m_statusClient_ = SockStatusInfo_t::Connected;
}

DataBuffer_t Client::LoadData() {
    if (m_statusClient_ != SocketStatusInfo::Connected) {
        return DataBuffer_t();
    }
    DataBuffer_t dataBuffer;
    uint32_t size;
    int error = 0;
#ifdef _WIN32
    if (u_long t = true; SOCKET_ERROR == ioctlsocket(m_socketClient_, FIONBIO, &t)) {
        return DataBuffer_t();
    }
    int answer = recv(m_socketClient_, (char *)&size, sizeof(size), 0);
    if (u_long t = false; SOCKET_ERROR == ioctlsocket(m_socketClient_, FIONBIO, &t)){
        return DataBuffer_t();
    }
#else
    int answer = recv(m_socketClient_, (char *)&size, sizeof(size), MSG_DONTWAIT);
#endif
    if (!answer) {
        Disconnect();
        return DataBuffer_t();
    } else if (answer == -1) {
        WIN (
                error = convertError();
                if (!error) {
                    SocketLength_t length = sizeof (error);
                    getsockopt(m_socketClient_, SOL_SOCKET, SO_ERROR, WIN((char*))&error, &length);
                }
        )
        NIX (
                SocketLength_t length = sizeof (error);
                getsockopt(m_socketClient_, SOL_SOCKET, SO_ERROR, WIN((char*))&error, &length);
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

    if (size > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
        std::cerr << "DataBuffer size exceeds maximum int value\n";
        Disconnect();
        return DataBuffer_t();
    }

    dataBuffer.resize(static_cast<size_t>(size));
    int recvResult = recv(m_socketClient_, reinterpret_cast<char*>(dataBuffer.data()), static_cast<int>(dataBuffer.size()), 0);
    if (recvResult < 0) {
        int err = errno;
        std::cerr << "Error receiving data: " << std::strerror(err) << '\n';
        Disconnect();
        return DataBuffer_t();
    }

    return dataBuffer;
}


DataBuffer_t Client::LoadDataSync() const {
    DataBuffer_t dataBuffer;
    uint32_t size = 0;
    int answer = recv(m_socketClient_, reinterpret_cast<char*>(&size), sizeof(size), 0);
    if(size && answer == sizeof(size)){
        if (size > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
            std::cerr << "DataBuffer size exceeds maximum int value\n";
            return DataBuffer_t();
        }
        dataBuffer.resize(static_cast<size_t>(size));
        recv(m_socketClient_, reinterpret_cast<char *>(dataBuffer.data()), static_cast<int>(dataBuffer.size()), 0);
    }
    return dataBuffer;
}


void Client::SetHandler(Client::DataHandleFunctionClient handler) {
    {
        std::lock_guard lockGuard(m_handleMutex_);
        m_dataHandlerFunction = std::move(handler);
    }
    switch (m_threadManagmentType_) {
        case Client::ThreadManagementType::SingleThread:
            if(m_threadClient.m_threadClient_){
                return;
            }
            m_threadClient.m_threadClient_ = new std::thread(&Client::HandleSingleThread, this);
        break;
        case Client::ThreadManagementType::ThreadPool:
            m_threadClient.m_threadPoolClient_->AddTask([this]{HandleThreadPool();});
        break;
    }
}

void Client::JoinHandler() const {
    switch (m_threadManagmentType_) {
        case Client::ThreadManagementType::SingleThread:
            if (m_threadClient.m_threadClient_) {
                m_threadClient.m_threadClient_->join();
            }
            break;
        case Client::ThreadManagementType::ThreadPool:
            if (m_threadClient.m_threadPoolClient_) {
                m_threadClient.m_threadPoolClient_->JoinThreads();
            }
            break;
        default:
            std::cerr << "Invalid ThreadManagementType\n";
            break;
    }
}

bool Client::SendData(const void *buffer, const size_t size) const {
    size_t bufferSize = size + sizeof(int);
    std::vector<char> sendBuffer(bufferSize);
    *reinterpret_cast<int*>(sendBuffer.data()) = static_cast<int>(size);
    memcpy(sendBuffer.data() + sizeof(int), buffer, size);

#ifdef _WIN32
    int bytesSent = send(m_socketClient_, sendBuffer.data(), static_cast<int>(bufferSize), 0);
    if (bytesSent == SOCKET_ERROR || static_cast<size_t>(bytesSent) != bufferSize) {
        return false;
    }
#else
    ssize_t bytesSent = send(m_socketClient_, sendBuffer.data(), bufferSize, 0);
    if (bytesSent == -1 || static_cast<size_t>(bytesSent) != bufferSize) {
        return false;
    }
#endif

    return true;
}





uint32_t Client::GetHost() const {
    return
            NIX(
                    m_addressClient_.sin_addr.s_addr
                    )
            WIN(
                    m_addressClient_.sin_addr.S_un.S_addr
                    );
}

uint16_t Client::GetPort() const {
    return m_addressClient_.sin_port;
}

/*bool Client::SetDataPc() {
    constexpr DWORD maxAdapterInfo = 16;
    constexpr DWORD maxInfoBufSize = 256;

    // --- Domain ---
    IP_ADAPTER_INFO AdapterInfo[maxAdapterInfo];
    DWORD dwBufLen = sizeof(AdapterInfo);
    DWORD dwStatus = GetAdaptersInfo(AdapterInfo, &dwBufLen);
    if (dwStatus != ERROR_SUCCESS) {
        return false;
    }
    for (PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo; true; pAdapterInfo = pAdapterInfo->Next) {
        std::string domain = pAdapterInfo->IpAddressList.IpAddress.String;
        m_pcDataReqest_.SetDomain(domain);
        break;
    }

    // --- Machine ---
    TCHAR info_buf[maxInfoBufSize];
    DWORD buf_char_count = maxInfoBufSize;
    if (!GetComputerName(info_buf, &buf_char_count)) {
        return false;
    }
    std::string machine = info_buf;
    m_pcDataReqest_.SetMachine(machine);

    // --- IP ---
    for (PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo; true; pAdapterInfo = pAdapterInfo->Next) {
        std::string ip = pAdapterInfo->IpAddressList.IpAddress.String;
        m_pcDataReqest_.SetIp(ip);
        break;
    }

    // --- User ---
    *//*buf_char_count = maxInfoBufSize;
    if (!GetUserName(info_buf, &buf_char_count)) {
        return false;
    }
    std::string user = info_buf;*//*
    std::string user = username_;
    m_pcDataReqest_.SetUser(user);

    return true;
}*/

bool Client::SetDataPc() {
    // --- Domain ---
    std::string domain;
#ifdef _WIN32 // Для Windows
    constexpr DWORD maxAdapterInfo = 16;
    IP_ADAPTER_INFO AdapterInfo[maxAdapterInfo];
    DWORD dwBufLen = sizeof(AdapterInfo);
    DWORD dwStatus = GetAdaptersInfo(AdapterInfo, &dwBufLen);
    if (dwStatus == ERROR_SUCCESS) {
        for (PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo; pAdapterInfo != nullptr; pAdapterInfo = pAdapterInfo->Next) {
            domain = pAdapterInfo->IpAddressList.IpAddress.String;
            break;
        }
    }
#else // Для Ubuntu и других Unix-подобных систем
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        return false;
    }
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        char addr_buf[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr, addr_buf, sizeof(addr_buf)) != nullptr) {
            domain = addr_buf;
            break;
        }
    }
    freeifaddrs(ifaddr);
#endif
    m_pcDataReqest_.SetDomain(domain);

    // --- Machine ---
    std::string machine;
#ifdef _WIN32 // Для Windows
    TCHAR info_buf[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD buf_char_count = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerName(info_buf, &buf_char_count)) {
        machine = info_buf;
    }
#else // Для Ubuntu и других Unix-подобных систем
    struct utsname buf;
    if (uname(&buf) != -1) {
        machine = buf.nodename;
    }
#endif
    m_pcDataReqest_.SetMachine(machine);

    // --- IP ---
    std::string ip;
#ifdef _WIN32 // Для Windows
    if (dwStatus == ERROR_SUCCESS) {
        for (PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo; pAdapterInfo != nullptr; pAdapterInfo = pAdapterInfo->Next) {
            ip = pAdapterInfo->IpAddressList.IpAddress.String;
            break;
        }
    }
#else // Для Ubuntu и других Unix-подобных систем
    if (getifaddrs(&ifaddr) == -1) {
        return false;
    }
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        char addr_buf[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr, addr_buf, sizeof(addr_buf)) != nullptr) {
            ip = addr_buf;
            break;
        }
    }
    freeifaddrs(ifaddr);
#endif
    m_pcDataReqest_.SetIp(ip);

    // --- User ---
    /*buf_char_count = maxInfoBufSize;
    if (!GetUserName(info_buf, &buf_char_count)) {
        return false;
    }
    std::string user = info_buf;*/
    std::string user = username_;
    m_pcDataReqest_.SetUser(user);

    return true;
}



void Client::GetDataPC() {
    if (!Client::SetDataPc())
        return;

    // Time
    std::chrono::system_clock::time_point lastRequestTime = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(lastRequestTime);
    char buft[80];
    std::strftime(buft, sizeof(buft), "%Y-%m-%d %H:%M:%S", std::localtime(&t));

    std::string timeStr(buft);

    // WSend
    std::string message =   "    (User: "    + m_pcDataReqest_.GetUser()     + " " +
                            " domain: "   + m_pcDataReqest_.GetDomain()  + " " +
                            " machine: " + m_pcDataReqest_.GetMachine()  + " " +
                            " Ip: "      + m_pcDataReqest_.GetIp()       + " " +
                            " Time: "    + timeStr                       + ")";
    int messageSize = static_cast<int>(message.size());
    send(m_socketClient_, message.c_str(), messageSize + 1, 0);
}

bool Client::SetUserName(std::string user) {
    m_pcDataReqest_.SetUser(std::move(user));
    return true;
}

/*std::string Client::GeneratePassword() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S");
    std::string timestamp = ss.str();

    auto sha256 = [](const std::string& input) {
        std::hash<std::string> hasher;
        return hasher(input);
    };

    std::string password = std::to_string(sha256(timestamp));

    return password;
}*/

std::string Client::GeneratePassword() const {
    std::string username = m_pcDataReqest_.GetUser();
    auto sha256 = [](const std::string& input) {
        std::hash<std::string> hasher;
        return hasher(input);
    };

    std::string password = std::to_string(sha256(username));

    return password;
}

bool Client::SendAuthData() const {
    std::string username = m_pcDataReqest_.GetUser();

    std::string password = GeneratePassword();
    std::string authData = username + ":" + password;

    if (!SendData(authData.c_str(), authData.size())) {
        std::cerr << "Failed to send authentication data to server\n";
        return false;
    }

    return true;
}
