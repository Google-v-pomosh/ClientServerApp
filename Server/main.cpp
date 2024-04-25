#include <iostream>
#include "../Server/TCP/inc/header.h"

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

Server server(8081,
              {1, 1, 1},
              [](DataBuffer_t dataBuffer, Server::InterfaceClientSession& client){
                    std::cout << "Client " << getHostStr(client) << " send data [ " << dataBuffer.size() << "bytes ]: " << (char*)dataBuffer.data() << '\n';
                    client.FindNamePass(dataBuffer, client);
                    client.SendData("Hello, client\0", sizeof ("Hello, client\0"));
              },
              [](Server::InterfaceClientSession& client){
                    std::cout << "Client " << getHostStr(client) << " Connected\n";
              },
              [](Server::InterfaceClientSession& client){
                    Server::InterfaceClientSession::ConnectionTimes(client);
                    std::cout << "Client " << getHostStr(client) << " disconnected\n";
              },
              std::thread::hardware_concurrency()
              );

void serverIOThread(Server &server) {
    while (true) {
        std::string command;
        std::getline(std::cin, command);
        if (command == "exit") {
            server.ServerDisconnectAll();
            break;

        }
    }
}

int main() {

    std::cout << "Hello, World! Server" << std::endl;

    /*server.SetServerDataHandler(DataHandlerToDataBase);*/

    if (server.StartServer() == SocketStatusInfo::Connected) {
        std::cout << "Server listening on port: " << server.GetServerPort() << '\n'
                  << "Server handling thread pool size: " << server.GetThreadExecutor().GetThreadCount() << std::endl;

        std::thread serverThread(serverIOThread, std::ref(server));

        try {
            server.JoinLoop();
        } catch (const std::exception& e) {
            std::cerr << "Exception: " << e.what() << std::endl;
            server.StopServer();
        }

        serverThread.join();
        return EXIT_SUCCESS;
    } else {
        std::cerr << "Failed to start server! Error code: " << static_cast<int>(server.GetServerStatus()) << std::endl;
        return EXIT_FAILURE;
    }
}



/*
int main() {
    std::cout << "Hello, World! Server" << std::endl;
    try {
        if (server.StartServer() == SocketStatusInfo::Connected){
            std::cout   << "Server listen on port: " << server.GetServerPort() << std::endl
                        << "Server handling thread pool size: " << server.GetThreadExecutor().GetThreadCount() << std::endl;
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
}*/
