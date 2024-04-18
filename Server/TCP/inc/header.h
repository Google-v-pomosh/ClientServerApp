#ifndef ALL_HEADER_SERVER_H
#define ALL_HEADER_SERVER_H

#include <functional>
#include <cstring>

#include <list>

#include <chrono>

#include <thread>
#include <mutex>
#include <shared_mutex>

#ifdef _WIN32 // Windows NT
#include <WinSock2.h>
#include <mstcpip.h>
#else // *nix
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#endif

#include "../../../TCP/inc/header.h"

class KeepAliveConfiguration {
public:
    KeepAliveConfiguration(KeepAliveProperty_t idle = 120,
                           KeepAliveProperty_t interval = 3,
                           KeepAliveProperty_t count = 5
                           );
    KeepAliveProperty_t GetIdle() const {return idle_;};
    KeepAliveProperty_t GetInterval() const {return interval_;};
    KeepAliveProperty_t GetCount() const {return count_;};

    void SetIdle(KeepAliveProperty_t idle) {idle_ = idle;};
    void SetInterval(KeepAliveProperty_t interval) {interval_ = interval;};
    void SetCount(KeepAliveProperty_t count) {count_ = count;};
private:
    KeepAliveProperty_t idle_;
    KeepAliveProperty_t interval_;
    KeepAliveProperty_t count_;
};

class Server {
public:

    class InterfaceServerSession;

    using DataHandleFunction = std::function<void(DataBuffer_t , Server::InterfaceServerSession&)>;
    using ConnectionHandlerFunction = std::function<void(Server::InterfaceServerSession&)>;

    static constexpr auto kDefaultDataHandler
        = [](DataBuffer_t, Server::InterfaceServerSession&){};
    static constexpr auto kDefaultConnectionHandler
        = [](Server::InterfaceServerSession&){};

    Server(const uint16_t port,
           KeepAliveConfiguration keep_alive_config     = {},
           DataHandleFunction handler                   = kDefaultDataHandler,
           ConnectionHandlerFunction connect_handle     = kDefaultConnectionHandler,
           ConnectionHandlerFunction disconnect_handle  = kDefaultConnectionHandler,
           uint32_t thread_count                        = HARDWARE_CONCURRENCY
    );

    ~Server();

    //setter
    void SetServerHandler(DataHandleFunction handler);
    uint16_t SetServerPort(const uint16_t port);


    //getter
    ThreadPool& GetThreadPool() {return m_threadPool_;};
    SocketStatusInfo GetServerStatus() const {return m_serverStatus_;}
    uint16_t GetServerPort() const {return port_;};

    SocketStatusInfo StartServer();

    void StopServer();
    void JoinLoop() {m_threadPool_.Join();};

    bool ServerConnectTo(uint32_t host, uint16_t port, ConnectionHandlerFunction connect_handle);
    void ServerSendData(const void* buffer, const size_t size);
    bool ServerSendDataBy(uint32_t host, uint16_t port, const void* buffer, const size_t size);
    bool ServerDisconnectBy(uint32_t host, uint16_t port);
    void ServerDisconnectAll();


private:
    using ServerSessionIterator = std::list<std::unique_ptr<InterfaceServerSession>>::iterator;
    std::list<std::unique_ptr<InterfaceServerSession>> m_session_list_;

    DataHandleFunction m_handler_ = kDefaultDataHandler;
    ConnectionHandlerFunction m_connectHandle_ = kDefaultConnectionHandler;
    ConnectionHandlerFunction m_disconnectHandle_ = kDefaultConnectionHandler;

    SocketHandle_t m_socketServer_;
    SocketStatusInfo m_serverStatus_ = SocketStatusInfo::Disconnected;
    KeepAliveConfiguration m_keepAliveConfig_;

    ThreadPool m_threadPool_;
    std::mutex m_clientMutex_;

    std::uint16_t port_;

    bool EnableKeepAlive(SocketHandle_t socket);
    void HandlingAcceptLoop();
    void WaitingDataLoop();

};

class Server::InterfaceServerSession : public TCPInterfaceBase {
public:
    InterfaceServerSession(SocketHandle_t socket, SocketAddressIn_t address);
    virtual ~InterfaceServerSession() override;
    virtual uint32_t GetHost() const override;
    virtual uint16_t GetPort() const override;
    virtual SockStatusInfo_t GetStatus() const override {return m_connectionStatus_;};
    virtual SockStatusInfo_t Disconnect() override;

    virtual DataBuffer_t LoadData() override;
    virtual bool SendData(const void* buffer, const size_t size) const override;
    virtual SocketType GetType() const override {return SocketType::Server;}

private:
    std::mutex m_accessMutex_;
    SocketAddressIn_t m_address_;
    SocketHandle_t m_socketDescriptor_;
    SockStatusInfo_t m_connectionStatus_ = SockStatusInfo_t::Connected;

};

#endif //ALL_HEADER_SERVER_H
