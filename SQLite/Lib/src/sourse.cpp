#include "../inc/header.h"

void DatabaseHandler::readAllFromDatabase() {
    sqlite3* dbConnection;
    int rc = sqlite3_open(dbPath_, &dbConnection);
    if (rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(dbConnection) << std::endl;
        sqlite3_close(dbConnection);
        return;
    }

    sqlite3_stmt* stmt;
    const char* selectSQL = "SELECT * FROM UserInfo";

    rc = sqlite3_prepare_v2(dbConnection, selectSQL, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare SQL statement: " << sqlite3_errmsg(dbConnection) << std::endl;
        sqlite3_close(dbConnection);
        return;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        std::cout << "Time: " << sqlite3_column_text(stmt, 6) << std::endl;
        std::cout << "Username: " << sqlite3_column_text(stmt, 0) << std::endl;
        std::cout << "Port: " << sqlite3_column_text(stmt, 2) << std::endl;
        std::cout << "Connection Time: " << sqlite3_column_text(stmt, 3) << std::endl;
        std::cout << "Disconnection Time: " << sqlite3_column_text(stmt, 4) << std::endl;
        std::cout << "Duration: " << sqlite3_column_text(stmt, 5) << std::endl;
        std::cout << "Password: " << sqlite3_column_text(stmt, 1) << std::endl;
        std::cout << "---------------------------------------------------------" << std::endl;
    }

    if (rc != SQLITE_DONE) {
        std::cerr << "Error reading from database: " << sqlite3_errmsg(dbConnection) << std::endl;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(dbConnection);
}

std::string DatabaseHandler::formatTime(int hours, int minutes, int seconds) {
    char buffer[9];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hours, minutes, seconds);
    return std::string(buffer);
}

void DatabaseHandler::calculateConnectionTimeForUser(const std::string &username, const std::string &date) {
    sqlite3* dbConnection;
    int rc = sqlite3_open(dbPath_, &dbConnection);
    if (rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(dbConnection) << std::endl;
        sqlite3_close(dbConnection);
        return;
    }

    std::string selectSQL = "SELECT duration FROM UserInfo WHERE username = ? AND timeToday = ?";
    sqlite3_stmt* stmt;

    rc = sqlite3_prepare_v2(dbConnection, selectSQL.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare SQL statement: " << sqlite3_errmsg(dbConnection) << std::endl;
        sqlite3_close(dbConnection);
        return;
    }

    rc = sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_bind_text(stmt, 2, date.c_str(), -1, SQLITE_STATIC);

    int totalHours = 0;
    int totalMinutes = 0;
    int totalSeconds = 0;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char* durationStr = sqlite3_column_text(stmt, 0);
        std::string durationString(reinterpret_cast<const char*>(durationStr));

        int hours, minutes, seconds;
        sscanf(durationString.c_str(), "%d:%d:%d", &hours, &minutes, &seconds);
        totalHours += hours;
        totalMinutes += minutes;
        totalSeconds += seconds;
    }

    if (rc != SQLITE_DONE) {
        std::cerr << "Error reading from database: " << sqlite3_errmsg(dbConnection) << std::endl;
    }

    totalMinutes += totalSeconds / 60;
    totalSeconds %= 60;
    totalHours += totalMinutes / 60;
    totalMinutes %= 60;

    std::cout << "Total connection time for user " << username << " on " << date << ": "
              << formatTime(totalHours, totalMinutes, totalSeconds) << std::endl;

    sqlite3_finalize(stmt);
    sqlite3_close(dbConnection);
}

void DatabaseHandler::printUserData(const std::string &username) {
    sqlite3* dbConnection;
    int rc = sqlite3_open(dbPath_, &dbConnection);
    if (rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(dbConnection) << std::endl;
        sqlite3_close(dbConnection);
        return;
    }

    std::string selectSQL = "SELECT * FROM UserInfo WHERE username = ?";
    sqlite3_stmt* stmt;

    rc = sqlite3_prepare_v2(dbConnection, selectSQL.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare SQL statement: " << sqlite3_errmsg(dbConnection) << std::endl;
        sqlite3_close(dbConnection);
        return;
    }

    rc = sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        std::cout << "Username: " << sqlite3_column_text(stmt, 0) << std::endl;
        std::cout << "SessionPort: " << sqlite3_column_int(stmt, 2) << std::endl;
        std::cout << "ConnectTime: " << sqlite3_column_text(stmt, 3) << std::endl;
        std::cout << "DisconnectTime: " << sqlite3_column_text(stmt, 4) << std::endl;
        std::cout << "Duration: " << sqlite3_column_text(stmt, 5) << std::endl;
        std::cout << "TimeToday: " << sqlite3_column_text(stmt, 6) << std::endl;
        std::cout << "---------------------------------------------" << std::endl;
    }

    if (rc != SQLITE_DONE) {
        std::cerr << "Error reading from database: " << sqlite3_errmsg(dbConnection) << std::endl;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(dbConnection);
}
