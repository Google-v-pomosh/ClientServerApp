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


#ifdef _WIN32 // Windows
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

constexpr uint32_t LOCALHOST_IP = 0x0100007f;

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

enum class SocketType : uint8_t {
    Client = 0,
    Server = 1
};

class ThreadPool {
public:

    void Start(uint32_t thread_count = HARDWARE_CONCURRENCY);
    void Stop();

    void Join();

    void RestartJob();

    uint32_t GetThreadCount() const;

    template<typename A>
    void AddJob(A work) {
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
    void AddJob(const A& work, const Arg&... args) {
        AddJob([work, args...]{work(args...);});
    }

    ThreadPool(uint32_t thread_count = HARDWARE_CONCURRENCY) { SetupThreadPool(thread_count);};
    ~ThreadPool();

private:
    void SetupThreadPool(uint32_t thread_count);
    void WorkerLoop();

    std::vector<std::thread> m_threadPool_;
    std::queue<std::function<void()>> m_queueWork_;
    std::mutex m_queueMutex_;
    std::condition_variable m_conditionVariable_;
    std::atomic<bool> m_terminatePool_ = false;

};

#ifdef _WIN32 //Windows
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
    static inline WindowsSocketInitializer winsockInitializer;
}
#endif

class TCPInterfaceBase {
public:
    typedef SocketStatusInfo SockStatusInfo_t;
    virtual ~TCPInterfaceBase() {};

    virtual SockStatusInfo_t Disconnect() = 0;
    virtual SockStatusInfo_t GetStatus() const = 0;

    virtual bool SendData(const void* buffer,const size_t size) const = 0;

    virtual DataBuffer_t LoadData() = 0;

    virtual uint32_t GetHost() const = 0;
    virtual uint16_t GetPort() const = 0;

    virtual SocketType GetType() const = 0;
};


#endif //ALL_HEADER_H
