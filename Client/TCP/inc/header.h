#ifndef ALL_HEADER_CLIENT_H
#define ALL_HEADER_CLIENT_H

#include <cstdint>
#include <cstddef>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
typedef SOCKET socket_t;
#else
typedef int socket_t;
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#endif

#include "../../../TCP/inc/header.h"
#include <memory.h>

class Client : public TCPInterfaceBase {
private:
    enum class ThreadManagementType : bool {
        SingleThread = false,
        ThreadPool = true
    };

    union ThreadClient {
        std::thread* thread;
        ThreadPool* threadPool;
        ThreadClient(): thread(nullptr) {}
        ThreadClient(ThreadPool* threadPool) : threadPool(threadPool) {}
        ~ThreadClient(){}

    };

    SocketAddressIn_t address_;
    socket_t client_socket_;

    std::mutex handle_mutex_;
    std::function<void(DataBuffer_t)> function_handler = [](DataBuffer_t){};
    ThreadManagementType threadManagmentType;
    ThreadClient threadClient;
    SockStatusInfo_t client_status_ = SockStatusInfo_t::Disconnected;

    void handleSingleThread();
    void handleThreadPool();

public:
    typedef std::function<void(DataBuffer_t)> function_handler_typedef;
    Client() noexcept;
    Client(ThreadPool* threadPool) noexcept;
    virtual ~Client() override;

    SockStatusInfo_t connectTo(uint32_t host, uint16_t port) noexcept;
    virtual SockStatusInfo_t Disconnect() noexcept override;

    virtual uint32_t GetHost() const override;
    virtual uint16_t GetPort() const override;
    virtual SockStatusInfo_t GetStatus() const override {return client_status_;}

    virtual DataBuffer_t LoadData() override;
    DataBuffer_t loadDataSync();
    void setHandler(function_handler_typedef handler);
    void joinHandler() const;

    virtual bool SendData(const void* buffer, const size_t size) const override;
    virtual SocketType GetType() const override { return SocketType::Client;}

};

#endif //ALL_HEADER_CLIENT_H
