#include <iostream>
#include "../Client/TCP/inc/header.h"


std::string getHostStr(uint32_t ip, uint16_t port) {
    return  std::string() +
            std::to_string(int(reinterpret_cast<char*>(&ip)[0])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[1])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[2])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[3])) + '.' +
            std::to_string(port);
}
void runClient(Client& client) {
    /*using namespace std::chrono_literals;*/
    if (!client.SetDataPc()) {
        std::cerr << "Failed to set PC data\n";
        return;
    }
    if (client.ConnectTo(kLocalhostIP, 8081) == SocketStatusInfo::Connected) {
        std::clog << "Client Connected\n";
        client.SetHandler([&client](DataBuffer_t dataBuffer){
            /*std::this_thread::sleep_for(1s);*/
            std::clog << "Recived " << dataBuffer.size() << " bytes: " << (char *)dataBuffer.data() << '\n';
            client.GetDataPC();
            /*std::this_thread::sleep_for(1s);*/
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
        std::cout << "Received: " << receivedMessage << std::endl;
    });

    while (running) {
        std::cout << "> ";
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

    Client firstClient(&m_clientThreadPool);
    /*Client secondClient(&m_clientThreadPool);
    Client thirdClient(&m_clientThreadPool);
    Client fourthClient(&m_clientThreadPool);*/


    std::thread clientThread(clientIOThread, std::ref(firstClient));

    std::thread runThread1(runClient, std::ref(firstClient));
    /*std::thread runThread2(runClient, std::ref(secondClient));
    std::thread runThread3(runClient, std::ref(thirdClient));
    std::thread runThread4(runClient, std::ref(fourthClient));*/

    runThread1.join();
    /*runThread2.join();
    runThread3.join();
    runThread4.join();*/

    clientThread.join();

    /*Client firstClient(&m_clientThreadPool);
    Client secondClient(&m_clientThreadPool);
    Client thrirdClient(&m_clientThreadPool);
    Client fourthClient(&m_clientThreadPool);

    runClient(firstClient);
    runClient(secondClient);
    runClient(thrirdClient);
    runClient(fourthClient);

    firstClient.JoinHandler();
    secondClient.JoinHandler();
    thrirdClient.JoinHandler();
    fourthClient.JoinHandler();*/

    return EXIT_SUCCESS;
}
