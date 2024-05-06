#ifndef ALL_HEADER_H
#define ALL_HEADER_H

#ifdef _WIN32
#include <winsock2.h>
#include <winsock.h>
#else
#define SD_BOTH 0
#include <sys/socket.h>
#endif

#include <functional>
#include <cstdint>
#include <cstring>
#include <cinttypes>

#include <iostream>

#include <queue>
#include <vector>

#include <thread>
#include <mutex>
#include <atomic>
#include <cstdlib>
#include <condition_variable>


#define HARDWARE_CONCURRENCY std::thread::hardware_concurrency()


#ifdef _WIN32 // Lib
typedef int SocketLength_t;
typedef SOCKADDR_IN SocketAddressIn_t;
typedef SOCKET SocketHandle_t;
typedef u_long KeepAliveProperty_t;
#else   //*nix
typedef socklen_t SocketLength_t;
typedef struct sockaddr_in SocketAddressIn_t;
typedef int SocketHandle_t;
typedef int KeepAliveProperty_t;
#endif

constexpr uint32_t kLocalhostIP = 0x0100007f;

enum class SocketStatusInfo : uint8_t {
    Connected       = 0,
    InitError       = 1,
    BindError       = 2,
    ConnectError    = 3,
    KeepAliveError  = 4,
    ListeningError  = 5,
    Disconnected    = 6
};

typedef std::vector<uint8_t> DataBuffer_t;

enum class ConnectionType : uint8_t {
    Client = 0,
    Server = 1
};

struct MessageTypes {
    [[maybe_unused]] static const int SendAuthenticationUser = 0;
    static const int SendMessageTo = 1;
};

class NetworkThreadPool {
public:

    void StartThreads(uint32_t thread_count = HARDWARE_CONCURRENCY);
    void StopThreads();

    void JoinThreads();

    void ResetJob();

    [[nodiscard]] uint32_t GetThreadCount() const;

    template<typename A>
    void AddTask(A work) {
        if(m_terminatePool_) {
            return;
        }
        {
            std::unique_lock lock(m_queueMutex_);
            m_queueWork_.push(std::function<void()>(work));
        }
        m_conditionVariable_.notify_all();
    }

    template<typename A, typename ... Arg>
    void AddTask(const A& work, const Arg&... args) {
        AddTask([work, args...]{work(args...);});
    }

    explicit NetworkThreadPool(uint32_t thread_count = HARDWARE_CONCURRENCY) { ConfigureThreadPool(thread_count);};
    ~NetworkThreadPool();

private:
    void ConfigureThreadPool(uint32_t thread_count);
    void ThreadWorkerLoop();

    std::vector<std::thread> m_threadPool_;
    std::queue<std::function<void()>> m_queueWork_;
    std::mutex m_queueMutex_;
    std::condition_variable m_conditionVariable_;
    std::atomic<bool> m_terminatePool_ = false;

};

#ifdef _WIN32 //Lib
namespace {
    class WindowsSocketInitializer {
    public:
        WindowsSocketInitializer() {
            if (WSAStartup(MAKEWORD(2, 2), &m_wsaData_) != 0) {
                std::cerr << "WSAStartUp is faled\n";
            }
        }
        ~WindowsSocketInitializer(){
            WSACleanup();
        }

    private:
        static WSAData m_wsaData_;
    };

    WSAData WindowsSocketInitializer::m_wsaData_;
    inline WindowsSocketInitializer winsockInitializer;
}
#endif

class TCPInterfaceBase {
public:
    typedef SocketStatusInfo SockStatusInfo_t;
    virtual ~TCPInterfaceBase() = default;

    virtual SockStatusInfo_t Disconnect() = 0;
    [[nodiscard]] virtual SockStatusInfo_t GetStatus() const = 0;

    virtual bool SendData(const void* buffer,size_t size) const = 0;

    virtual DataBuffer_t LoadData() = 0;

    [[nodiscard]] virtual uint32_t GetHost() const = 0;
    [[nodiscard]] virtual uint16_t GetPort() const = 0;

    [[nodiscard]] virtual ConnectionType GetType() const = 0;
};


#endif //ALL_HEADER_H
