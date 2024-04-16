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

struct KeepAliveConfig{
    ka_prop_t ka_idle = 120;
    ka_prop_t ka_intvl = 3;
    ka_prop_t ka_cnt = 5;
};

class Server {
public:
    enum class Status : uint8_t {
        up = 0,
        err_socket_init = 1,
        err_socket_bind = 2,
        err_scoket_keep_alive = 3,
        err_socket_listening = 4,
        close = 5
    };

    class Client;
    class ClientList;

    typedef std::function<void(DataBuffer , Client&)> function_handler_typedef;
    typedef std::function<void(Client&)> function_handler_typedef_connect;

    static constexpr auto default_data_handler = [](DataBuffer, Client&){};
    static constexpr auto default_connsection_handler = [](Client&){};

    Server(const uint16_t port,
              KeepAliveConfig keep_alive_config = {},
              function_handler_typedef handler = default_data_handler,
              function_handler_typedef_connect connect_handle = default_connsection_handler,
              function_handler_typedef_connect disconnect_handle = default_connsection_handler,
              uint32_t thread_count = HARDWARE_CONCURRENCY
    );

    ~Server();

    //setter
    /** dxxxfjuhxtgfj */
    void setServerHandler(function_handler_typedef handler);
    uint16_t setServerPort(const uint16_t port);


    //getter
    ThreadPool& getThreadPool() {return thread_pool_;};
    Status getServerStatus() const {return server_status_;}
    uint16_t getServerPort() const {return port_;};

    Status startServer();

    void stopServer();
    void joinLoop() {thread_pool_.join();};

    bool ServerConnectTo(uint32_t host, uint16_t port, function_handler_typedef_connect connect_handle);
    void ServerSendData(const void* buffer, const size_t size);
    bool ServerSendDataBy(uint32_t host, uint16_t port, const void* buffer, const size_t size);
    bool ServerDisconnectBy(uint32_t host, uint16_t port);
    void ServerDisconnectAll();


private:
    typedef std::list<std::unique_ptr<Client>>::iterator iterator_client_;
    std::list<std::unique_ptr<Client>> client_list_;

    function_handler_typedef handler_ = default_data_handler;
    function_handler_typedef_connect connect_handle_ = default_connsection_handler;
    function_handler_typedef_connect disconnect_handle_ = default_connsection_handler;

    Socket socket_server;
    Status server_status_ = Status::close;
    KeepAliveConfig keep_alive_config_;

    ThreadPool thread_pool_;
    std::mutex client_mutex_;

    std::uint16_t port_;

    bool enableKeepAlive(Socket socket);
    void handlingAcceptLoop();
    void waitingDataLoop();

};

class Server::Client : public ClientBase {
public:
    Client(Socket socket, SocketAddr_in address);
    virtual ~Client() override;
    virtual uint32_t getHost() const override;
    virtual uint16_t getPort() const override;
    virtual status getStatus() const override {return client_status_;};
    virtual status disconnect() override;

    virtual DataBuffer loadData() override;
    virtual bool sendData(const void* buffer, const size_t size) const override;
    virtual SocketType getType() const override {return SocketType::server_socket;}



private:
    friend class Server;

    std::mutex access_mutex_;
    SocketAddr_in address_;
    Socket client_socket_;
    status client_status_ = status::connected;



};



#endif //ALL_HEADER_SERVER_H
