#include <iostream>
#include <utility>
#include "../Client/TCP/inc/header.h"


#define DEBUGLOG

std::atomic<bool> exitRequested(false);

void clientIOThread(Client& client) {
    std::string input;
    client.SetHandler([&](const DataBuffer_t& data) {
        std::string receivedMessage(data.begin(), data.end());
#ifdef DEBUGLOG
        std::cout << "Received: " << receivedMessage << std::endl;
#endif
    });
    while (!exitRequested) {
        std::getline(std::cin, input);
        if (input == "exit") {
            exitRequested = true;
            client.Disconnect();
        } else if (input == "connect") {
            auto status = client.ConnectTo(kLocalhostIP, 8081); // 127.0.0.1:8081
            if (status == Client::SockStatusInfo_t::Connected) {
                std::cout << "Connected to server." << std::endl;
            } else {
                std::cout << "Failed to connect to server." << std::endl;
            }
        } else if (input == "status") {
            auto status = client.GetStatus();
            std::cout << "Client status: " << static_cast<int>(status) << std::endl;
        } else if (input == "SendTo") {
            std::cout << "To whom should I send your message?" << std::endl;
            std::string recipient;
            std::getline(std::cin, recipient);

            std::cout << "What message do you want to send?" << std::endl;
            std::string message;
            std::getline(std::cin, message);

            client.SendMessageTo(recipient, message);
        } else if (!input.empty()) {
            client.SendData(input.data(), input.size());
        }
    }
}

int main() {
    std::cout << "Hello, World!Iam Client" << std::endl;

    NetworkThreadPool m_thread_pool;
    Client client(&m_thread_pool, "Alex");
    if(client.ConnectTo(kLocalhostIP, 8081) == SocketStatusInfo::Connected) {
        if (!client.SetDataPc()) {
            std::cerr << "Failed to set PC data\n";
            return EXIT_FAILURE;
        }
        std::cout << "Client connected\n";
        m_thread_pool.AddTask([&client]{client.SendAuthData();});
        client.SetHandler([&client](DataBuffer_t dataBuffer) {return client.ReciveHandler(std::move(dataBuffer)); });

        std::thread clientIOThreadVar(clientIOThread, std::ref(client));

        clientIOThreadVar.detach();

        m_thread_pool.JoinThreads();
    } else {
        std::cerr << "Client isn`t connected\n";
        return EXIT_FAILURE;
    }
    return EXIT_FAILURE;
}