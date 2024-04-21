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

#include "../../../SQLite/Windows/inc/sqlite3.h"
/*#include <sqlite3.h>*/
#include "../../../TCP/inc/header.h"

class ServerKeepAliveConfig {
public:
    ServerKeepAliveConfig(KeepAliveProperty_t idle = 120,
                           KeepAliveProperty_t interval = 3,
                           KeepAliveProperty_t count = 5
                           );
    [[nodiscard]] KeepAliveProperty_t GetIdle() const {return idle_;};
    [[nodiscard]] KeepAliveProperty_t GetInterval() const {return interval_;};
    [[nodiscard]] KeepAliveProperty_t GetCount() const {return count_;};

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

    class InterfaceServerSession : public TCPInterfaceBase {
        public:
            InterfaceServerSession(SocketHandle_t socket, SocketAddressIn_t address);
            ~InterfaceServerSession() override;
            [[nodiscard]] uint32_t GetHost() const override;
            [[nodiscard]] uint16_t GetPort() const override;
            [[nodiscard]] SockStatusInfo_t GetStatus() const override {return m_connectionStatus_;};
            SockStatusInfo_t Disconnect() override;

            DataBuffer_t LoadData() override;
            bool SendData(const void* buffer, size_t size) const override;
            [[nodiscard]] ConnectionType GetType() const override {return ConnectionType::Server;}
        private:
            friend class Server;

            std::mutex m_accessMutex_;
            SocketAddressIn_t m_address_;
            SocketHandle_t m_socketDescriptor_;
            SockStatusInfo_t m_connectionStatus_ = SockStatusInfo_t::Connected;

    };

    class DataBase {
        public:

        private:

    };

    using DataHandleFunctionServer = std::function<void(DataBuffer_t, Server::InterfaceServerSession&)>;
    using ConnectionHandlerFunction = std::function<void(Server::InterfaceServerSession&)>;

    static constexpr auto kDefaultDataHandlerServer
        = [](const DataBuffer_t&, Server::InterfaceServerSession&){};
    static constexpr auto kDefaultConnectionHandlerServer
        = [](Server::InterfaceServerSession&){};

    explicit Server(uint16_t port,
           ServerKeepAliveConfig keep_alive_config      = {},
           DataHandleFunctionServer handler             = kDefaultDataHandlerServer,
           ConnectionHandlerFunction connect_handle     = kDefaultConnectionHandlerServer,
           ConnectionHandlerFunction disconnect_handle  = kDefaultConnectionHandlerServer,
           uint32_t thread_count                        = HARDWARE_CONCURRENCY
    );

    ~Server();

    //setter
    void SetServerDataHandler(DataHandleFunctionServer handler);
    uint16_t SetServerPort(uint16_t port);


    //getter
    NetworkThreadPool& GetThreadExecutor() {return m_threadPoolServer_;};
    [[nodiscard]] SocketStatusInfo GetServerStatus() const {return m_serverStatus_;}
    [[nodiscard]] uint16_t GetServerPort() const {return port_;};

    SocketStatusInfo StartServer();

    void StopServer();
    void JoinLoop() {m_threadPoolServer_.JoinThreads();};

    bool ServerConnectTo(uint32_t host, uint16_t port, const ConnectionHandlerFunction& connect_handle);
    void ServerSendData(const void* buffer, size_t size);
    bool ServerSendDataBy(uint32_t host, uint16_t port, const void* buffer, size_t size);
    bool ServerDisconnectBy(uint32_t host, uint16_t port);
    void ServerDisconnectAll();

    /*void HandleConnectionWithTimer(InterfaceServerSession& client);
    bool WriteToDataBase(const std::string &data);*/

private:
    using ServerSessionIterator = std::list<std::unique_ptr<InterfaceServerSession>>::iterator;
    std::list<std::unique_ptr<InterfaceServerSession>> m_session_list_;

    DataHandleFunctionServer m_handler_ = kDefaultDataHandlerServer;
    ConnectionHandlerFunction m_connectHandle_ = kDefaultConnectionHandlerServer;
    ConnectionHandlerFunction m_disconnectHandle_ = kDefaultConnectionHandlerServer;

    SocketHandle_t m_socketServer_{};
    SocketStatusInfo m_serverStatus_ = SocketStatusInfo::Disconnected;
    ServerKeepAliveConfig m_keepAliveConfig_;

    NetworkThreadPool m_threadPoolServer_;
    std::mutex m_clientMutex_;

    std::uint16_t port_;

    bool EnableKeepAlive(SocketHandle_t socket);
    void HandlingAcceptLoop();
    void WaitingDataLoop();

};

#endif //ALL_HEADER_SERVER_H
