#ifndef ALL_HEADER_CLIENT_H
#define ALL_HEADER_CLIENT_H

#include <cstdint>
#include <cstddef>
#include <utility>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#endif
#include <iphlpapi.h>
#include "../../../TCP/inc/header.h"
#include <memory.h>

#pragma comment(lib, "IPHLPAPI.lib")

class Client : public TCPInterfaceBase {
private:
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

        std::string GetDomain() {return domain_;};
        std::string GetMachine() {return machine_;};
        std::string GetIp(){return ip_;};
        std::string GetUser(){return user_;};
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

public:
    using DataHandleFunctionClient = std::function<void(DataBuffer_t)>;
    Client() noexcept;
    explicit Client(NetworkThreadPool* client_thread_pool) noexcept;
    ~Client() override;

    SockStatusInfo_t ConnectTo(uint32_t host, uint16_t port) noexcept;
    SockStatusInfo_t Disconnect() noexcept override;

    [[nodiscard]] uint32_t GetHost() const override;
    [[nodiscard]] uint16_t GetPort() const override;
    [[nodiscard]] SockStatusInfo_t GetStatus() const override {return m_statusClient_;}

    DataBuffer_t LoadData() override;
    [[nodiscard]] DataBuffer_t LoadDataSync() const;
    void SetHandler(DataHandleFunctionClient handler);
    void JoinHandler() const;

    bool SendData(const void* buffer, size_t size) const override;
    [[nodiscard]] ConnectionType GetType() const override { return ConnectionType::Client;}

    PcDataReqest m_pcDataReqest_;

    bool SetDataPc();
    void GetDataPC();
};

#endif //ALL_HEADER_CLIENT_H
