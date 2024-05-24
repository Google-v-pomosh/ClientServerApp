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

#ifdef _WIN32 // Lib NT
#include <WinSock2.h>
#include <mstcpip.h>
#include <set>

#else // *nix
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#endif

#ifdef _WIN32
#define DB_PATH "C:/CLionProjects/ClientServerApp/Server/example.db"
#else
#define DB_PATH "/home/alex/CLionProjects/ClientServerApp/Server/example.db"
#endif


#include "../../../SQLite/Lib/inc/sqlite3.h"
#include "../../../TCP/inc/header.h"
/*#include "../../../Lib/sha256/inc/sha256.h"*/

#define SHA256_DIGEST_SIZE 32

typedef unsigned char BYTE;
typedef unsigned int  WORDL;

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

class Database;

class Server {
public:
    class InterfaceClientSession : public TCPInterfaceBase {
    private:
        typedef struct {
            BYTE data[64];
            WORDL datalen;
            unsigned long long bitlen;
            WORDL state[8];
        } SHA256_CTX;

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
        bool AutentficateUserInfo(const DataBuffer_t& data,Server::InterfaceClientSession& client, Server& server);
        [[nodiscard]] ConnectionType GetType() const override {return ConnectionType::Server;}

        [[nodiscard]] std::chrono::system_clock::time_point GetFirstConnectionTime() const { return m_firstConnectionTime_; }
        [[nodiscard]] std::chrono::system_clock::time_point GetLastDisconnectionTime() const { return m_lastDisconnectionTime_; }

        static std::string ConnectionTimes(const InterfaceClientSession& client, Server& server);
        void OnDisconnect(const InterfaceClientSession& client, Server& server);

        std::string GetDayNow();
        std::string GetConnectionTime();
        std::string GetDisconnectionTime();
        static void WriteToDB(const InterfaceClientSession& client, Server& server);

        void sha256_transform(SHA256_CTX *ctx, const BYTE data[]);
        void sha256_init(SHA256_CTX *ctx);
        void sha256_update(SHA256_CTX *ctx, const BYTE data[], size_t len);
        void sha256_final(SHA256_CTX *ctx, BYTE hash[]);

        bool checkHash(const std::string& content, const std::string& receivedHash);

    };

    struct UserInfo {
        std::string username_;
        std::string password_;
        uint16_t sessionPort_;
        std::string connectTime_;
        std::string disconnectTime_;
        std::string duration_;
        std::string timeToday_;

        explicit UserInfo(std::string username,
                          std::string pass,
                          uint16_t port,
                          std::string connectTime,
                          std::string disconnectTime,
                          std::string duration,
                          std::string timeToday);
    };

    struct ClientKey{ uint32_t host; uint16_t port; };

    struct ClientComparator {
        using is_transparent = std::true_type;

        bool operator()(const std::unique_ptr<InterfaceClientSession>& lhs, const std::unique_ptr<InterfaceClientSession>& rhs) const {
            return (uint64_t(lhs->GetHost()) | uint64_t(lhs->GetPort()) << 32) < (uint64_t(rhs->GetHost()) | uint64_t(rhs->GetPort()) << 32);
        }

        bool operator()(const std::unique_ptr<InterfaceClientSession>& lhs, const ClientKey& rhs) const {
            return (uint64_t(lhs->GetHost()) | uint64_t(lhs->GetPort()) << 32) < (uint64_t(rhs.host) | uint64_t(rhs.port) << 32);
        }

        bool operator()(const ClientKey& lhs, const std::unique_ptr<InterfaceClientSession>& rhs) const {
            return (uint64_t(lhs.host) | uint64_t(lhs.port) << 32) < (uint64_t(rhs->GetHost()) | uint64_t(rhs->GetPort()) << 32);
        }
    };

    void printUserInfo(const UserInfo& userInfo);
    void printAllUsersInfo();
    void initializeDatabase();
    void writeToDatabase(const UserInfo& userInfo);
    void clearUser(const std::string& username);

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
           unsigned int thread_count                    = HARDWARE_CONCURRENCY
    );

    ~Server();

    //setter
    void SetServerDataHandler(DataHandleFunctionServer handler);
    uint16_t SetServerPort(uint16_t port);


    //getter
    NetworkThreadPool& GetThreadExecutor() {return m_threadPoolServer_;};
    [[nodiscard]] SocketStatusInfo GetServerStatus() const {return m_serverStatus_;}
    [[nodiscard]] uint16_t GetServerPort() const {return port_;};
    std::mutex& getUsersMutex() {return usersMutex;}
    const std::unordered_map<std::string, std::vector<UserInfo>>& getUsers() const {
        return users;
    }

    SocketStatusInfo StartServer();

    void StopServer();
    void JoinLoop() {m_threadPoolServer_.JoinThreads();};

    bool ServerConnectTo(uint32_t host, uint16_t port, const ConnectionHandlerFunction& connect_handle);
    void ServerSendData(const void* buffer, size_t size);
    bool ServerSendDataBy(uint32_t host, uint16_t port, const void* buffer, size_t size);
    bool ServerDisconnectBy(uint32_t host, uint16_t port);
    void ServerDisconnectAll();

private:
    std::unordered_map<std::string, std::vector<UserInfo>> users;
    std::mutex usersMutex;

    using ServerSessionIterator = std::list<std::unique_ptr<InterfaceClientSession>>::iterator;
    std::set<std::unique_ptr<InterfaceClientSession>, ClientComparator> m_session_list_;

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
