#include "Windows/inc/sqlite3.h"
#include <iostream>

static int callback(void *data, int argc, char **argv, char **azColName) {
    std::cout << (const char *)data << ": ";

    for (int i = 0; i < argc; i++) {
        std::cout << azColName[i] << " = " << (argv[i] ? argv[i] : "NULL") << std::endl;
    }
    std::cout << std::endl;
    return 0;
}

int main() {

    sqlite3 *db;
    char *zErrMsg = nullptr;
    int rc;



    if (rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return 0;
    } else {
        std::cout << "Opened database successfully" << std::endl;
    }

    const char *sql_create = "CREATE TABLE IF NOT EXISTS COMPANY("
                             "ID INT PRIMARY KEY     NOT NULL,"
                             "NAME           TEXT    NOT NULL,"
                             "AGE            INT     NOT NULL,"
                             "ADDRESS        CHAR(50),"
                             "SALARY         REAL );";

    rc = sqlite3_exec(db, sql_create, nullptr, nullptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
    } else {
        std::cout << "Table created successfully" << std::endl;
    }

    const char *sql_insert = "INSERT INTO COMPANY (ID,NAME,AGE,ADDRESS,SALARY) "
                             "VALUES (1, 'Paul', 32, 'California', 20000.00 ); "
                             "INSERT INTO COMPANY (ID,NAME,AGE,ADDRESS,SALARY) "
                             "VALUES (2, 'Allen', 25, 'Texas', 15000.00 );";

    rc = sqlite3_exec(db, sql_insert, nullptr, nullptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
    } else {
        std::cout << "Records inserted successfully" << std::endl;
    }

    const char *sql_select = "SELECT * FROM COMPANY";

    rc = sqlite3_exec(db, sql_select, callback, (void *)"SELECT result", &zErrMsg);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
    }

    sqlite3_close(db);
    return 0;
}
