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

std::atomic<bool> exitRequested(false);

Server server(8081,
              {1, 1, 1},
              [](DataBuffer_t dataBuffer, Server::InterfaceClientSession& client){
                  std::cout << "Client " << getHostStr(client) << " send data [ " << dataBuffer.size() << "bytes ]: " << (char*)dataBuffer.data() << '\n';
                  client.AutentficateUserInfo(dataBuffer,client, server);
                  client.SendData("Hello, client\0", sizeof ("Hello, client\0"));
              },
              [](Server::InterfaceClientSession& client){
                  std::cout << "Client " << getHostStr(client) << " Connected\n";
              },
              [](Server::InterfaceClientSession& client){
                  std::cout << "Client " << getHostStr(client) << " disconnected\n";
                  client.OnDisconnect(client, server);
                  Server::InterfaceClientSession::WriteToDB(client, server);
              },
              std::thread::hardware_concurrency()
);

void serverIOThread(Server &server) {
    while (!exitRequested) {
        std::string command;
        std::getline(std::cin, command);
        if (command == "exit") {
            exitRequested = true;
            server.ServerDisconnectAll();
        } else if (command == "print") {
            server.printAllUsersInfo();
        }
    }
}


int main() {

    std::cout << "Hello, World, Iam Server" << std::endl;

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
