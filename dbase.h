#ifndef ESCADA_CORE_DBASE_H
#define ESCADA_CORE_DBASE_H

#define MAX_QUERY_LENGTH    300

#include <mysql/mysql.h>
#include <stdint.h>
#include "const.h"
#include <string.h>

class DBase {
private:
    MYSQL *mysql = nullptr;
    char fields[32][64];
    uint8_t nFields;
public:
    char driver[MAX_STR];
    char host[MAX_STR];
    char user[MAX_STR];
    char pass[MAX_STR];
    char database[MAX_STR];

    DBase() {
        memset(driver, 0, MAX_STR);
        memset(host, 0, MAX_STR);
        memset(user, 0, MAX_STR);
        memset(pass, 0, MAX_STR);
        memset(database, 0, MAX_STR);
    }

    int openConnection(); //connect to the database
    int disconnect(); //disconnect from the database
    MYSQL_RES *sqlexec(const char *query);

    char *GetChannel(char *measureTypeUuid, uint16_t channel, char *deviceUuid);
    bool StoreData(uint16_t type, uint16_t status, double value, char  *data, char *channelUuid);

    int8_t makeFieldsList(MYSQL_RES *res);

    int8_t getFieldIndex(const char *fieldName);
};

#endif //ESCADA_CORE_DBASE_H
