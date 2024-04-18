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
    if (client.connectTo(LOCALHOST_IP, 8081) == SocketStatusInfo::Connected) {
#ifdef DEBUG
        std::clog << "Client Connected\n";
#endif
        client.setHandler([&client](DataBuffer_t dataBuffer){
            std::this_thread::sleep_for(3s);
#ifdef DEBUG
            std::clog << "Recived " << dataBuffer.size() << " bytes: " << (char *)dataBuffer.data() << '\n';
#endif
            std::this_thread::sleep_for(1s);
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

    ThreadPool threadPool;


    Client firstClient(&threadPool);
    Client secondClient(&threadPool);
    Client thrirdClient(&threadPool);
    Client fourthClient(&threadPool);


    runClient(firstClient);
    runClient(secondClient);
    runClient(thrirdClient);
    runClient(fourthClient);


    firstClient.joinHandler();
    std::cout << "firstClient" << std::endl;



    secondClient.joinHandler();
    std::cout << "secondClient" << std::endl;



    thrirdClient.joinHandler();
    std::cout << "thrirdClient" << std::endl;


    fourthClient.joinHandler();
    std::cout << "fourthClient" << std::endl;

    return EXIT_SUCCESS;
}
