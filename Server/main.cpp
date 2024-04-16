#include <iostream>
#include "../Server/TCP/inc/header.h"

std::string getHostStr(const Server::Client& client)
{
    uint32_t ip = client.getHost();
    return  std::string() +
            std::to_string(int(reinterpret_cast<char*>(&ip)[0])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[1])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[2])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[3])) + '.' +
            std::to_string(client.getPort());
}

Server server(8081,
              {1, 1, 1},
              [](DataBuffer dataBuffer, Server::Client& client){
                    using namespace std::chrono_literals;
                    std::cout << "Client " << getHostStr(client) << " send data [ " << dataBuffer.size() << "bytes ]: " << (char*)dataBuffer.data() << '\n';
                    std::this_thread::sleep_for(1s);
                    client.sendData("Hello, client\0", sizeof ("Hello, client\0"));
              },
              [](Server::Client& client){
                    std::cout << "Client " << getHostStr(client) << " connected\n";
              },
              [](Server::Client& client){
                  std::cout << "Client " << getHostStr(client) << " disconnected\n";
              },
              std::thread::hardware_concurrency()
              );

int main() {
    std::cout << "Hello, World! fdsgdfg" << std::endl;
    try {
        if (server.startServer() == Server::Status::up){
            std::cout   << "Server listen on port: " << server.getServerPort() << std::endl
                        << "Server handling thread pool size: " << server.getThreadPool().getThreadCount() << std::endl;
            server.joinLoop();
            return EXIT_SUCCESS;
        } else {
            std::cout << "Server start error! Error code:" << int(server.getServerStatus()) << std::endl;
            return EXIT_FAILURE;
        }
    } catch (std::exception& exception) {
        std::cerr << exception.what();
        return EXIT_FAILURE;
    }
}