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

class Client : public ClientBase {
private:
    enum class ThreadManagementType : bool {
        single_thread = false,
        thread_pool = true
    };

    union ThreadClient {
        std::thread* thread;
        ThreadPool* threadPool;
        ThreadClient(): thread(nullptr) {}
        ThreadClient(ThreadPool* threadPool) : threadPool(threadPool) {}
        ~ThreadClient(){}

    };

    SocketAddr_in address_;
    socket_t client_socket_;

    std::mutex handle_mutex_;
    std::function<void(DataBuffer)> function_handler = [](DataBuffer){};
    ThreadManagementType threadManagmentType;
    ThreadClient threadClient;
    status client_status_ = status::disconnected;

    void handleSingleThread();
    void handleThreadPool();

public:
    typedef std::function<void(DataBuffer)> function_handler_typedef;
    Client() noexcept;
    Client(ThreadPool* threadPool) noexcept;
    virtual ~Client() override;

    status connectTo(uint32_t host, uint16_t port) noexcept;
    virtual status disconnect() noexcept override;

    virtual uint32_t getHost() const override;
    virtual uint16_t getPort() const override;
    virtual status getStatus() const override {return client_status_;}

    virtual DataBuffer loadData() override;
    DataBuffer loadDataSync();
    void setHandler(function_handler_typedef handler);
    void joinHandler() const;

    virtual bool sendData(const void* buffer, const size_t size) const override;
    virtual SocketType getType() const override { return SocketType::client_socket;}

};

#endif //ALL_HEADER_CLIENT_H
