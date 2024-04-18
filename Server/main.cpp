#include <iostream>
#include "../Server/TCP/inc/header.h"

std::string getHostStr(const Server::InterfaceServerSession& client)
{
    uint32_t ip = client.GetHost();
    return  std::string() +
            std::to_string(int(reinterpret_cast<char*>(&ip)[0])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[1])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[2])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[3])) + '.' +
            std::to_string(client.GetPort());
}

Server server(8081,
              {1, 1, 1},
              [](DataBuffer_t dataBuffer, Server::InterfaceServerSession& client){
                    using namespace std::chrono_literals;
                    std::cout << "Client " << getHostStr(client) << " send data [ " << dataBuffer.size() << "bytes ]: " << (char*)dataBuffer.data() << '\n';
                    std::this_thread::sleep_for(1s);
                    client.SendData("Hello, client\0", sizeof ("Hello, client\0"));
              },
              [](Server::InterfaceServerSession& client){
                    std::cout << "Client " << getHostStr(client) << " Connected\n";
              },
              [](Server::InterfaceServerSession& client){
                  std::cout << "Client " << getHostStr(client) << " disconnected\n";
              },
              std::thread::hardware_concurrency()
              );

int main() {
    std::cout << "Hello, World! fdsgdfg" << std::endl;
    try {
        if (server.StartServer() == SocketStatusInfo::Connected){
            std::cout   << "Server listen on port: " << server.GetServerPort() << std::endl
                        << "Server handling thread pool size: " << server.GetThreadPool().GetThreadCount() << std::endl;
            server.JoinLoop();
            return EXIT_SUCCESS;
        } else {
            std::cout << "Server start error! Error code:" << int(server.GetServerStatus()) << std::endl;
            return EXIT_FAILURE;
        }
    } catch (std::exception& exception) {
        std::cerr << exception.what();
        return EXIT_FAILURE;
    }
}