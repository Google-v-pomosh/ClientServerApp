#include <iostream>
#include "../Client/TCP/inc/header.h"


#define DEBUGLOG

std::atomic<bool> exitRequested(false);

void runClient(Client& client) {
    if (!client.SetDataPc()) {
        std::cerr << "Failed to set PC data\n";
        return;
    }
    if (client.ConnectTo(kLocalhostIP, 8081) == SocketStatusInfo::Connected) {
        std::cout << "Client Connected\n";
        if (!client.SendAuthData()) {
            std::cerr << "Failed to send auth data\n";
            return;
        }
        //client.GetDataPC();
        client.SetHandler([&client](DataBuffer_t dataBuffer){
#ifdef DEBUGLOG
            std::clog << "Recived " << dataBuffer.size() << " bytes: " << (char *)dataBuffer.data() << '\n';
#endif
            //client.SendData("Hello, server\0", sizeof("Hello, server\0"));
        });
        client.SendData("Hello, server\0", sizeof("Hello, server\0"));
    } else {
        std::cerr << "Client is not Connected\n";
        std::exit(EXIT_FAILURE);
    }
}

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
    /*NetworkThreadPool m_thread_pool;
    Client client(&m_thread_pool, "Anton");

    if(client.ConnectTo(kLocalhostIP, 8021) == SocketStatusInfo::Connected) {
        std::cout << "Client connected\n";
        m_thread_pool.AddTask(client.SendAuthData());
    }*/

    std::cout << "Hello, World! Client" << std::endl;

    NetworkThreadPool m_clientThreadPool;

    Client firstClient(&m_clientThreadPool, "Anton");

    std::thread clientInputThread(clientIOThread, std::ref(firstClient));
    std::thread runThread1(runClient, std::ref(firstClient));

    runThread1.join();
    clientInputThread.join();

    return EXIT_SUCCESS;
}