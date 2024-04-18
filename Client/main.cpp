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
    using namespace std::chrono_literals;
    if (!client.SetDataPc()) {
        std::cerr << "Failed to set PC data\n";
        return;
    }
    if (client.ConnectTo(kLocalhostIP, 8081) == SocketStatusInfo::Connected) {
        std::clog << "Client Connected\n";
        client.SetHandler([&client](DataBuffer_t dataBuffer){
            std::this_thread::sleep_for(10s);
            std::clog << "Recived " << dataBuffer.size() << " bytes: " << (char *)dataBuffer.data() << '\n';
            client.GetDataPC();
            std::this_thread::sleep_for(10s);
            client.SendData("Hello, server\0", sizeof("Hello, server\0"));
        });
        client.SendData("Hello, server\0", sizeof("Hello, server\0"));
    } else {
        std::cerr << "Client is not Connected\n";
        std::exit(EXIT_FAILURE);
    }
}

int main() {
    std::cout << "Hello, World!" << std::endl;

    NetworkThreadPool m_clientThreadPool;


    Client firstClient(&m_clientThreadPool);
    Client secondClient(&m_clientThreadPool);
    Client thrirdClient(&m_clientThreadPool);
    Client fourthClient(&m_clientThreadPool);


    runClient(firstClient);
    runClient(secondClient);
    runClient(thrirdClient);
    runClient(fourthClient);


    firstClient.JoinHandler();
    std::cout << "firstClient" << std::endl;



    secondClient.JoinHandler();
    std::cout << "secondClient" << std::endl;



    thrirdClient.JoinHandler();
    std::cout << "thrirdClient" << std::endl;


    fourthClient.JoinHandler();
    std::cout << "fourthClient" << std::endl;

    return EXIT_SUCCESS;
}
