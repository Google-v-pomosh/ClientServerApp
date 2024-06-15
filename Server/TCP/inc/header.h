#ifndef ALL_HEADER_SERVER_H
#define ALL_HEADER_SERVER_H

#include <functional>
#include <cstring>

#include <list>
#include <map>
#include <set>
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
#include <optional>

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

        bool RegisteredUserInfo(const std::string& username, const std::string& password, Server &server, InterfaceClientSession& client);
        bool AuthorizeUserInfo(const std::string &username, const std::string &password, Server &server,
                               InterfaceClientSession &client);

        void HandleData(DataBuffer_t& data, Server& , InterfaceClientSession& client);

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

    struct UserLoginInfo {
        std::string username_;
        std::string password_;
        uint16_t sessionPort_{};
        std::string connectTime_;
        std::string disconnectTime_;
        std::string duration_;
        std::string timeToday_;

        explicit UserLoginInfo(std::string username,
                      std::string password,
                      uint16_t sessionPort,
                      std::string connectTime,
                      std::string disconnectTime,
                      std::string duration,
                      std::string timeToday)
                : username_(std::move(username)),
                  password_(std::move(password)),
                  sessionPort_(sessionPort),
                  connectTime_(std::move(connectTime)),
                  disconnectTime_(std::move(disconnectTime)),
                  duration_(std::move(duration)),
                  timeToday_(std::move(timeToday)) {}

        UserLoginInfo (
                std::string username,
                std::string password)
                : username_(std::move(username)),
                  password_(std::move(password))
        {}

        struct Comparator {
            using InfoComparator = std::true_type;

            bool operator()(const UserLoginInfo& lhs, const UserLoginInfo& rhs) const;
            bool operator()(const UserLoginInfo& lhs, const std::string& rhs) const;
            bool operator()(const std::string& lhs, const UserLoginInfo& rhs) const;
        };
    };

    struct Notification {
        std::time_t sendTime_;
        std::string content_;
    };

    struct UserPair {
        UserLoginInfo* firstUserLoginInfo;
        UserLoginInfo* secondUserLoginInfo;
    };

    struct ConnectionStatus {
        enum Status: unsigned char {
            NotAuthorized = 0x00,
            Authorized = 0x01,
            ErrorInvalid = 0xFF
        }status;
        TCPInterfaceBase *socket{};
        UserLoginInfo* userCredentials = nullptr;
    };

    static class UserManager {
        std::shared_mutex sharedMutex;
        std::set<UserLoginInfo, UserLoginInfo::Comparator> userLoginInfoSet;
    public:
        UserManager() = default;

        UserLoginInfo* Registred(std::string name, std::string pass);
        UserLoginInfo* Authorize(UserLoginInfo name, std::string pass);
        UserLoginInfo* Find(UserLoginInfo name);
    } userManager;

    struct ConnectionInfo : public Server::ConnectionStatus { uint32_t host{}; uint16_t port{}; };

    class SessionManager {
    private:
        struct SessionComparator {
            using InfoComparator = std::true_type;

            bool operator ()(const ConnectionStatus& lhs, const ConnectionStatus& rhs) const;
            bool operator ()(const ConnectionStatus& lhs, const ConnectionInfo& rhs) const;
            bool operator ()(const ConnectionInfo& lhs, const ConnectionStatus& rhs) const;
        };

        using SessionConnectionIterator = std::set<ConnectionStatus, SessionComparator>::iterator;

        struct UserComparator {
            using InfoComparator = std::true_type;

            inline bool operator ()(const SessionConnectionIterator& lhs, const SessionConnectionIterator& rhs) const;
            inline bool operator ()(const SessionConnectionIterator& lhs, UserLoginInfo* rhs) const;
            inline bool operator ()(UserLoginInfo* lhs, const SessionConnectionIterator& rhs) const;
        };

        mutable std::shared_mutex idSessionIdentifierMutex;
        mutable std::shared_mutex userIdentifierMutex;

        std::set<ConnectionStatus, SessionComparator> idSessionIdentifier;
        std::multiset<SessionConnectionIterator, UserComparator> userIdentifier;
    public:
        SessionManager() = default;
        
        bool AddSession(TCPInterfaceBase *socket);
        bool DeleteSession(TCPInterfaceBase* socket);
        bool RegistredUser(TCPInterfaceBase* socket, std::string username, std::string password);
        bool AuthorizeUser(TCPInterfaceBase* socket, std::string username, std::string password);
        bool Logout(TCPInterfaceBase* socket);

        std::list<TCPInterfaceBase*> findSocketListByUsername(std::string username);
        std::string GetUsername(TCPInterfaceBase* socket);
        ConnectionStatus::Status GetStatus (TCPInterfaceBase* socket) const;

    }sessionManager;

    struct ClientSessionComparator {
        using InfoComparator = std::true_type;

        bool operator()(const std::unique_ptr<InterfaceClientSession>& lhs, const std::unique_ptr<InterfaceClientSession>& rhs) const;
        bool operator()(const std::unique_ptr<InterfaceClientSession>& lhs, const ConnectionInfo& rhs) const;
        bool operator()(const ConnectionInfo& lhs, const std::unique_ptr<InterfaceClientSession>& rhs) const;
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
    const std::unordered_map<std::string, std::vector<UserLoginInfo>>& getUsersList() const {
        return usersList;
    }

    SocketStatusInfo StartServer();

    void StopServer();
    void JoinLoop() {m_threadPoolServer_.JoinThreads();};

    bool ServerConnectTo(uint32_t host, uint16_t port, const ConnectionHandlerFunction& connect_handle);
    void ServerSendData(const void* buffer, size_t size);
    bool ServerSendDataBy(uint32_t host, uint16_t port, const void* buffer, size_t size);
    bool ServerDisconnectBy(uint32_t host, uint16_t port);
    void ServerDisconnectAll();

    void HandleRegisterUser(Server::InterfaceClientSession &client, uint16_t codeSequence,
                            const std::string& username, const std::string& password);
    void HandleAuthorize(InterfaceClientSession& client, uint16_t act_sequence,
                         const std::string& nickname, const std::string& password_hash);
    void HandleSendTo(Server::InterfaceClientSession &client, uint16_t codeSequence,
                      const std::string& recipientUsername, const std::string& message);

    bool IsUserRegistered(const std::string& username, Server &server) const;

private:

    std::unordered_map<std::string, std::vector<UserInfo>> users;
    std::unordered_map<std::string, std::vector<UserLoginInfo>> usersList;
    std::mutex usersMutex;

    using ServerSessionIterator = std::list<std::unique_ptr<InterfaceClientSession>>::iterator;
    std::set<std::unique_ptr<InterfaceClientSession>, ClientSessionComparator> m_session_list_;

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
