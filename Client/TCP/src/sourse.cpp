#include "../inc/header.h"

#include <cstring>
#include <iostream>
#include <utility>
#include <iomanip>

//#define DEBUGLOG

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


bool Client::SendMessageTo(const std::string &recipientId, const std::string &message) {
    int messageType = MessageTypes::SendMessageTo;

    std::string sender = m_pcDataReqest_.GetUser();

    std::string startPoint;
    std::string stopPoint;

    startPoint +='\x3C';
    startPoint +='\x2F';

    stopPoint +=('\x7C');
    stopPoint +='\x3E';

    std::string dataForHash = sender + "->" + recipientId + "=" + message;
    std::string framedMessage = startPoint +
                                std::to_string(messageType) + "{" + sender + "->" + recipientId + "=" + message + "}" +
                                stopPoint;

    std::string hash = calculateHash(dataForHash);
    framedMessage += ":" + hash;

    return SendData(framedMessage.c_str(), framedMessage.size());
}

bool Client::SendAuthData() {
    std::string username = m_pcDataReqest_.GetUser();
    int messageType = MessageTypes::SendAuthenticationUser;

    std::string password = GeneratePassword();

    std::string authData;
    std::string startPoint;
    std::string stopPoint;

    startPoint +='\x3C';
    startPoint +='\x2F';

    stopPoint +=('\x7C');
    stopPoint +='\x3E';

    authData = username + ":" + password;

    std::string hash = calculateHash(authData);

    authData = startPoint + std::to_string(messageType) + "{" + authData + "}" + stopPoint + ":" + hash;
#ifdef DEBUGLOG
    std::cout << "authData: " << authData << std::endl;
#endif
    if (!SendData(authData.c_str(), authData.size())) {
        std::cerr << "Failed to send authentication data to server\n";
        return false;
    }

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

void Client::sha256_init(Client::SHA256_CTX *ctx) {
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

void Client::sha256_update(Client::SHA256_CTX *ctx, const BYTE *data, size_t len) {
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

void Client::sha256_transform(Client::SHA256_CTX *ctx, const BYTE *data) {
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

void Client::sha256_final(Client::SHA256_CTX *ctx, BYTE *hash) {
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

std::string Client::calculateHash(const std::string &data) {
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, reinterpret_cast<const BYTE*>(data.c_str()), data.length());
    BYTE hash[32];
    sha256_final(&ctx, hash);

    std::string result;
    for (int i = 0; i < 32; ++i) {
        result += hash[i];
    }
    return result;
}

void Client::ReciveHandler(DataBuffer_t dataBuffer) {
    auto dataIterator = dataBuffer.cbegin();
    uint16_t extractedValue = NetworkThreadPool::Extract<uint16_t>(dataIterator);
    ResponseCode responseCode = NetworkThreadPool::Extract<ResponseCode>(dataIterator);
    switch (responseCode) {
        case ResponseCode::AuthenticationOk:
        case ResponseCode::AuthenticationFail:
        {
            std::unique_lock uniqueLock(m_authenticationMutex_);
            authenticationActExtracted_ = extractedValue;
            m_authenticationResponse = responseCode;
        }
            m_authenticationIO.notify_one();
            return;
        case ResponseCode::IncomingMessage: {
            std::string sendler = NetworkThreadPool::ExtractString(dataIterator);
            std::string message = NetworkThreadPool::ExtractString(dataIterator);

            m_messageList_.push_back(sendler + ":" + message);
            UpdateSpace();
            return;
        }
        case ResponseCode::SendingOk: {
            return;
        }
        case ResponseCode::SendingFail: {
            m_messageList_.push_back("Send message faild");
            UpdateSpace();
            return;
        }
        case ResponseCode::AccessDenied: {
            return;
        }
    }
}

//TODO
/*std::string Client::ExtractString(std::vector<uint8_t>::iterator &it) {
    uint64_t string_size = Extract<uint64_t>(it);
    std::string string(reinterpret_cast<std::string::value_type*>(&*it), string_size);
    it += string_size;
    return string;
}*/

void Client::UpdateSpace() {
    clearConsole();
    for(const auto &message: m_messageList_) {
        std::cout << message << std::endl;
    }
    std::cout << "Resiver" << std::endl;
    if(!recivername_.empty()){
        std::cout << "To whom: " << recivername_ << std::endl;
        std::cout << "Notification: " << std::endl;
    }
}

void Client::clearConsole() {
#ifdef _WIN32
    COORD topLeft  = { 0, 0 };
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO screen;
    DWORD written;

    GetConsoleScreenBufferInfo(console, &screen);
    FillConsoleOutputCharacterA(
            console, ' ', screen.dwSize.X * screen.dwSize.Y, topLeft, &written
    );
    FillConsoleOutputAttribute(
            console, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE,
            screen.dwSize.X * screen.dwSize.Y, topLeft, &written
    );
    SetConsoleCursorPosition(console, topLeft);
#else
    std::cout << "\x1B[2J\x1B[H";
#endif
}


