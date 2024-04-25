#ifndef ALL_HEADER_SERVER_H
#define ALL_HEADER_SERVER_H

#include <functional>
#include <cstring>

#include <list>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>
#include <string>

#include <chrono>

#include <thread>
#include <mutex>
#include <shared_mutex>
#include <utility>

#include <iomanip>
#include <iostream>

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
    class InterfaceClientSession : public TCPInterfaceBase {
    public:

        InterfaceClientSession(SocketHandle_t socket, SocketAddressIn_t address);
        ~InterfaceClientSession() override;

        [[nodiscard]] uint32_t GetHost() const override;
        [[nodiscard]] uint16_t GetPort() const override;
        [[nodiscard]] SockStatusInfo_t GetStatus() const override {return m_connectionStatus_;};
        [[nodiscard]] std::string GetUserNameIn() const {return username_;};

        SockStatusInfo_t Disconnect() override;

        DataBuffer_t LoadData() override;
        bool SendData(const void* buffer, size_t size) const override;
        bool FindNamePass(const DataBuffer_t& data, Server::InterfaceClientSession& client, Server& server);
        [[nodiscard]] ConnectionType GetType() const override {return ConnectionType::Server;}

        [[nodiscard]] std::chrono::system_clock::time_point GetFirstConnectionTime() const { return m_firstConnectionTime_; }
        [[nodiscard]] std::chrono::system_clock::time_point GetLastDisconnectionTime() const { return m_lastDisconnectionTime_; }

        static std::string ConnectionTimes(const InterfaceClientSession& client, Server& server);

    private:
        friend class Server;
        std::mutex m_accessMutex_;
        SocketAddressIn_t m_address_;
        SocketHandle_t m_socketDescriptor_;
        SockStatusInfo_t m_connectionStatus_ = SockStatusInfo_t::Connected;

        std::chrono::system_clock::time_point m_firstConnectionTime_;
        std::chrono::system_clock::time_point m_lastDisconnectionTime_;


        void SetFirstConnectionTime() { m_firstConnectionTime_ = std::chrono::system_clock::now(); }
        void SetLastDisconnectionTime() { m_lastDisconnectionTime_ = std::chrono::system_clock::now(); }

        std::string username_;

    };

    struct UserInfo {
        std::string password_;
        uint16_t sessionPort_;
        std::string timeConnection_;
        std::string timeToday_;

        explicit UserInfo(std::string pass,
                          uint16_t port,
                          std::string timeConnection,
                          std::string timeToday)
                : password_(std::move(pass)),
                  sessionPort_(port),
                  timeConnection_(std::move(timeConnection)),
                  timeToday_(std::move(timeToday)) {}
    };

    void printUserInfo(const UserInfo& userInfo) {
        std::cout << "Password: " << userInfo.password_ << std::endl;
        std::cout << "Session Port: " << userInfo.sessionPort_ << std::endl;
        std::cout << userInfo.timeConnection_ << std::endl;
        std::cout << "Time Today: " << userInfo.timeToday_ << std::endl;
    }

    void printAllUsersInfo() {
        std::lock_guard<std::mutex> lock(usersMutex);
        for (const auto& pair : users) {
            std::cout << "Username: " << pair.first << std::endl;
            std::cout << "User Info:" << std::endl;
            printUserInfo(pair.second);
            std::cout << std::endl;
        }
    }

    /*static void AddUser(const std::string& username, const UserInfo& user) {
        std::lock_guard<std::mutex> lock(usersMutex);
        users[username] = user;
    }

    static void RemoveUser(const std::string& username){
        std::lock_guard<std::mutex> lock(usersMutex);
        users.erase(username);
    }

    static bool FindUser(const std::string& username, UserInfo& user) {
        std::lock_guard<std::mutex> lock(usersMutex);
        auto it = users.find(username);
        if (it != users.end()) {
            user = it->second;
            return true;
        }
        return false;
    }*/


    using DataHandleFunctionServer = std::function<void(DataBuffer_t , InterfaceClientSession&)>;
    using ConnectionHandlerFunction = std::function<void(InterfaceClientSession&)>;

    static constexpr auto kDefaultDataHandlerServer
        = [](const DataBuffer_t&, InterfaceClientSession&){};
    static constexpr auto kDefaultConnectionHandlerServer
        = [](InterfaceClientSession&){};

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

    /*void HandleConnectionWithTimer(InterfaceClientSession& client);
    bool WriteToDataBase(const std::string &data);*/

private:
    std::unordered_map<std::string, UserInfo> users;
    std::mutex usersMutex;

    std::string getHostStr(const Server::InterfaceClientSession& client)
    {
        uint32_t ip = client.GetHost();
        return  std::string() +
                std::to_string(int(reinterpret_cast<char*>(&ip)[0])) + '.' +
                std::to_string(int(reinterpret_cast<char*>(&ip)[1])) + '.' +
                std::to_string(int(reinterpret_cast<char*>(&ip)[2])) + '.' +
                std::to_string(int(reinterpret_cast<char*>(&ip)[3])) + '.' +
                std::to_string(client.GetPort());
    }

    using ServerSessionIterator = std::list<std::unique_ptr<InterfaceClientSession>>::iterator;
    std::list<std::unique_ptr<InterfaceClientSession>> m_session_list_;

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
