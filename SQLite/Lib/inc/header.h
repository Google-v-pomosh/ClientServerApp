#ifndef ALL_HEADER_H
#define ALL_HEADER_H

#include "sqlite3.h"
#include <iostream>
#include <string>

#ifdef _WIN32
#define DB_PATH "C:/CLionProjects/ClientServerApp/Server/example.db"
#else
#define DB_PATH "/home/alex/CLionProjects/ClientServerApp/Server/example.db"
#endif


class DatabaseHandler {
public:
    explicit DatabaseHandler(char *dbPath)
        :   dbPath_(const_cast<char*>(DB_PATH)),
            dbConnection_(nullptr)
    {
        int rc = sqlite3_open(dbPath_, &dbConnection_);
        if (rc) {
            std::cerr << "Can't open database: " << sqlite3_errmsg(dbConnection_) << std::endl;
            sqlite3_close(dbConnection_);
        }
    }

    ~DatabaseHandler() {
        Exit();
    }

    void readAllFromDatabase();
    std::string formatTime(int hours, int minutes, int seconds);
    void calculateConnectionTimeForUser(const std::string& username, const std::string& date);
    void printUserData(const std::string& username);

    void Exit() {
        if (dbConnection_) {
            sqlite3_close(dbConnection_);
            std::cout << "Database connection closed." << std::endl;
            dbConnection_ = nullptr;
        }
    }

private:
    char *dbPath_;
    sqlite3* dbConnection_;
};


#endif //ALL_HEADER_H
