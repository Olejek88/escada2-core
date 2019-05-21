#define MAX_QUERY_LENGTH    300

#include <mysql/mysql.h>
#include "const.h"
#include "mysql/mysql.h"

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
    char table[MAX_STR];

    DBase();

    int openConnection(); //connect to the database
    int disconnect(); //disconnect from the database
    MYSQL_RES *sqlexec(const char *query);
    uint16_t  DBase::GetChannel (uint16_t prm, uint16_t pipe, uint16_t device);
    bool DBase::StoreData(uint16_t device, uint16_t prm, uint16_t type, uint16_t status, double value, char *data, uint16_t channel);
};
