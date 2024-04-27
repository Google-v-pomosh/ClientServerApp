#include <iostream>
#include "../Client/TCP/inc/header.h"


//#define DEBUGLOG


std::string getHostStr(uint32_t ip, uint16_t port) {
    return  std::string() +
            std::to_string(int(reinterpret_cast<char*>(&ip)[0])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[1])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[2])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[3])) + '.' +
            std::to_string(port);
}
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
            client.SendData("Hello, server\0", sizeof("Hello, server\0"));
        });
        client.SendData("Hello, server\0", sizeof("Hello, server\0"));
    } else {
        std::cerr << "Client is not Connected\n";
        std::exit(EXIT_FAILURE);
    }
}

void clientIOThread(Client& client) {
    std::string input;
    bool running = true;
    client.SetHandler([&](const DataBuffer_t& data) {
        std::string receivedMessage(data.begin(), data.end());
#ifdef DEBUGLOG
        std::cout << "Received: " << receivedMessage << std::endl;
#endif
    });

    while (running) {
        std::getline(std::cin, input);
        if (input == "exit") {
            running = false;
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
        } else if (!input.empty()) {
            client.SendData(input.data(), input.size());
        }
    }
}

int main() {
    std::cout << "Hello, World! Client" << std::endl;

    NetworkThreadPool m_clientThreadPool;

    Client firstClient(&m_clientThreadPool, "Alex");
    std::thread clientThread1(clientIOThread, std::ref(firstClient));
    std::thread runThread1(runClient, std::ref(firstClient));

    /*std::this_thread::sleep_for(std::chrono::seconds(1));*/

    Client secondClient(&m_clientThreadPool, "Olga");
    std::thread clientThread2(clientIOThread, std::ref(secondClient));
    std::thread runThread2(runClient, std::ref(secondClient));

    /*std::this_thread::sleep_for(std::chrono::seconds(1));*/

    Client thirdClient(&m_clientThreadPool, "Kirill");
    std::thread clientThread3(clientIOThread, std::ref(thirdClient));
    std::thread runThread3(runClient, std::ref(thirdClient));

    /*std::this_thread::sleep_for(std::chrono::seconds(1));*/

    Client fourthClient(&m_clientThreadPool, "Evgeniy");
    std::thread clientThread4(clientIOThread, std::ref(fourthClient));
    std::thread runThread4(runClient, std::ref(fourthClient));

    runThread1.join();
    runThread2.join();
    runThread3.join();
    runThread4.join();

    clientThread1.join();
    clientThread2.join();
    clientThread3.join();
    clientThread4.join();

    return EXIT_SUCCESS;
}
