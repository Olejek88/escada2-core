#ifndef ESCADA_CORE_DBASE_H
#define ESCADA_CORE_DBASE_H

#define MAX_QUERY_LENGTH    300

#include <mysql/mysql.h>
#include <stdint.h>
#include <mysql/mysql.h>
#include "const.h"

class DBase {
private:
    MYSQL *mysql;
    MYSQL_ROW row;
    MYSQL_RES *res;
public:
    char driver[MAX_STR];
    char host[MAX_STR];
    char user[MAX_STR];
    char pass[MAX_STR];
    char database[MAX_STR];

    DBase();

    int openConnection(); //connect to the database
    int disconnect(); //disconnect from the database
    MYSQL_RES *sqlexec(const char *query);

    char *GetChannel(char *measureTypeUuid, uint16_t channel, char *deviceUuid);
    bool StoreData(uint16_t type, uint16_t status, double value, char  *data, char *channelUuid);
};

#endif //ESCADA_CORE_DBASE_H
