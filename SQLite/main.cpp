#ifdef _WIN32
#include "Lib/inc/sqlite3.h"
#include "Lib/inc/header.h"
#else
#include "Lib/inc/sqlite3.h"
#include "Lib/inc/header.h"
#endif

#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <sstream>
#include <iterator>


std::atomic<bool> exitRequested(false);

void DataBaseIOThread(DatabaseHandler &db) {
    std::cout << "Available commands:" << std::endl;
    std::cout << "exit - Exit the program" << std::endl;
    std::cout << "printAll - Print all data from the database" << std::endl;
    std::cout << "calculateTime <username> <date> - Calculate connection time for a user on a specific date" << std::endl;
    std::cout << "printUserData <username> - Print user data" << std::endl;
    std::cout << "help - Show available commands" << std::endl;

    while (!exitRequested) {
        std::string command;
        std::getline(std::cin, command);
        if (command == "exit") {
            exitRequested = true;
            db.Exit();
        } else if (command == "printAll") {
            db.readAllFromDatabase();
        } else if (command == "help") {
            std::cout << "Available commands:" << std::endl;
            std::cout << "exit - Exit the program" << std::endl;
            std::cout << "printAll - Print all data from the database" << std::endl;
            std::cout << "calculateTime <username> <date> - Calculate connection time for a user on a specific date" << std::endl;
            std::cout << "printUserData <username> - Print user data" << std::endl;
            std::cout << "help - Show available commands" << std::endl;
        } else if (command.find("calculateTime") == 0) {
            std::istringstream iss(command);
            std::vector<std::string> tokens(std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>());

            if (tokens.size() != 3) {
                std::cout << "Invalid command syntax. Usage: calculateTime <username> <date>" << std::endl;
            } else {
                db.calculateConnectionTimeForUser(tokens[1], tokens[2]);
            }
        } else if (command.find("printUserData") == 0) {
            std::istringstream iss(command);
            std::vector<std::string> tokens(std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>());

            if (tokens.size() != 2) {
                std::cout << "Invalid command syntax. Usage: printUserData <username>" << std::endl;
            } else {
                db.printUserData(tokens[1]);
            }
        } else {
            std::cout << "Unknown command. Type 'help' to see available commands." << std::endl;
        }
    }
}

int main() {
    std::cout << "Hello, World, Iam DataBase" << std::endl;
    DatabaseHandler db(DB_PATH);

    std::thread ThreadDataBase(DataBaseIOThread, std::ref(db));


    ThreadDataBase.join();
    return 0;
}
