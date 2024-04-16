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
#include <malloc.h>
#include <condition_variable>


#define HARDWARE_CONCURRENCY std::thread::hardware_concurrency()


#ifdef _WIN32 // Windows
typedef int SockLen_t;
typedef SOCKADDR_IN SocketAddr_in;
typedef SOCKET Socket;
typedef u_long ka_prop_t;
#else   //*nix
typedef socklen_t SockLen_t;
typedef struct sockaddr_in SocketAddr_in;
typedef int Socket;
typedef int ka_prop_t;
#endif

constexpr uint32_t LOCALHOST_IP = 0x0100007f;

enum class SocketStatus : uint8_t {
    connected = 0,
    err_socket_init = 1,
    err_socket_bind = 2,
    err_socket_connect = 3,
    disconnected = 4
};

typedef std::vector<uint8_t> DataBuffer;

enum class SocketType : uint8_t {
    client_socket = 0,
    server_socket = 1
};

class ThreadPool {
public:

    void start(uint32_t thread_count = HARDWARE_CONCURRENCY){
        if (!terminate_pool_) {
            return;
        }
        terminate_pool_ = false;
        setupThreadPool(thread_count);
    };
    void stop();

    template<typename A>
    void addJob(A work) {
        if(terminate_pool_) {
            return;
        }
        {
            std::unique_lock lock(queue_mutex_);
            queue_work_.push(std::function<void()>(work));
        }
        condition_variable_.notify_all();
    }

    template<typename A, typename ... Arg>
    void addJob(const A& work, const Arg&... args) {
        addJob([work, args...]{work(args...);});
    }

    void join();

    uint32_t getThreadCount() const;

    void restartJob();

    ThreadPool(uint32_t thread_count = HARDWARE_CONCURRENCY) { setupThreadPool(thread_count);};
    ~ThreadPool();

private:
    void setupThreadPool(uint32_t thread_count);
    void workerLoop();

    std::vector<std::thread> thread_pool_;
    std::queue<std::function<void()>> queue_work_;
    std::mutex queue_mutex_;
    std::condition_variable condition_variable_;
    std::atomic<bool> terminate_pool_ = false;

};

#ifdef _WIN32 //Windows
namespace {
    class WinSocket {
    public:
        WinSocket() {
            if(WSAStartup(MAKEWORD(2, 2), &w_data) != 0) {
                std::cerr << "WSAStartUp is faled\n";
            }
#ifndef DEBUG
            std::cout << "WinSocket init\n";
#endif
        }

        ~WinSocket() {
#ifndef DEBUG
            std::cout << "WinSocket close\n";
#endif
            WSACleanup();
        }

    private:
        static WSAData w_data;

    };

    WSAData WinSocket::w_data;
    static inline WinSocket winsock_initer;
}
#else
//TODO *nix
#endif

class ClientBase {
public:
    typedef SocketStatus status;
    virtual ~ClientBase() {};
    virtual status disconnect() = 0;
    virtual bool sendData(const void* buffer,const size_t size) const = 0;
    virtual DataBuffer loadData() = 0;
    virtual status getStatus() const = 0;
    virtual uint32_t getHost() const = 0;
    virtual uint16_t getPort() const = 0;
    virtual SocketType getType() const = 0;
};


#endif //ALL_HEADER_H
