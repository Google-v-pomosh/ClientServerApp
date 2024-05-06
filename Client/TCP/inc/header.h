#ifndef ALL_HEADER_CLIENT_H
#define ALL_HEADER_CLIENT_H

#include <cstdint>
#include <cstddef>
#include <utility>


#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <iphlpapi.h>
#define SOCKET_INVALID INVALID_SOCKET
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <sys/utsname.h>
#define SOCKET_INVALID (-1)
#endif

#include "../../../TCP/inc/header.h"
/*#include "../../../Lib/sha256/inc/sha256.h"*/
#include <memory.h>

#pragma comment(lib, "IPHLPAPI.lib")
#define SHA256_DIGEST_SIZE 32

typedef unsigned char BYTE;
typedef unsigned int  WORDL;


class Client : public TCPInterfaceBase {
private:
    typedef struct {
        BYTE data[64];
        WORDL datalen;
        unsigned long long bitlen;
        WORDL state[8];
    } SHA256_CTX;

    enum class ThreadManagementType : bool {
        SingleThread = false,
        ThreadPool = true
    };

    union ClientThread {
        std::thread* m_threadClient_;
        NetworkThreadPool* m_threadPoolClient_;
        ClientThread(): m_threadClient_(nullptr) {}
        explicit ClientThread(NetworkThreadPool* client_thread_pool) : m_threadPoolClient_(client_thread_pool) {}
        ~ClientThread(){}

    };

    class PcDataReqest {
    public:
        void SetDomain(std::string domain) {domain_ = std::move(domain);};
        void SetMachine(std::string machine) {machine_ = std::move(machine);};
        void SetIp(std::string ip) {ip_ = std::move(ip);};
        void SetUser(std::string user) {user_ = std::move(user);};

        [[nodiscard]] std::string GetDomain() const {return domain_;};
        [[nodiscard]] std::string GetMachine() const {return machine_;};
        [[nodiscard]] std::string GetIp() const {return ip_;};
        [[nodiscard]] std::string GetUser() const {return user_;};
    private:
        std::string domain_;
        std::string machine_;
        std::string ip_;
        std::string user_;
    };

    SocketAddressIn_t m_addressClient_;
    SocketHandle_t m_socketClient_;

    std::mutex m_handleMutex_;
    std::function<void(DataBuffer_t)> m_dataHandlerFunction = [](const DataBuffer_t&){};
    ThreadManagementType m_threadManagmentType_;
    ClientThread m_threadClient;
    SockStatusInfo_t m_statusClient_ = SockStatusInfo_t::Disconnected;

    void HandleSingleThread();
    void HandleThreadPool();
    void JoinThread();


    std::string username_;

public:
    void sha256_transform(SHA256_CTX *ctx, const BYTE data[]);
    void sha256_init(SHA256_CTX *ctx);
    void sha256_update(SHA256_CTX *ctx, const BYTE data[], size_t len);
    void sha256_final(SHA256_CTX *ctx, BYTE hash[]);

    using DataHandleFunctionClient = std::function<void(DataBuffer_t)>;
    explicit Client(std::string  username) noexcept;
    explicit Client(NetworkThreadPool* client_thread_pool, std::string  username) noexcept;
    ~Client() override;

    SockStatusInfo_t ConnectTo(uint32_t host, uint16_t port) noexcept;
    SockStatusInfo_t Disconnect() noexcept override;

    [[nodiscard]] uint32_t GetHost() const override;
    [[nodiscard]] uint16_t GetPort() const override;
    [[nodiscard]] SockStatusInfo_t GetStatus() const override {return m_statusClient_;}
    [[nodiscard]] std::string GetUser() const {return m_pcDataReqest_.GetUser();};

    DataBuffer_t LoadData() override;
    [[nodiscard]] DataBuffer_t LoadDataSync() const;
    void SetHandler(DataHandleFunctionClient handler);
    void JoinHandler() const;

    bool SendData(const void* buffer, size_t size) const override;
    bool SendMessageTo(const std::string& recipientId, const std::string& message);
    [[nodiscard]] bool SendAuthData();
    [[nodiscard]] std::string GeneratePassword() const ;
    [[nodiscard]] ConnectionType GetType() const override { return ConnectionType::Client;}
    std::string calculateHash(const std::string& data);

    PcDataReqest m_pcDataReqest_;

    bool SetDataPc();
    void GetDataPC();
    bool SetUserName(std::string user);
};

#endif //ALL_HEADER_CLIENT_H
