#define MAX_QUERY_LENGTH    300

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

    int openConnection(char *driver, char *host, char *user, char *pass, char *table); //connect to the database
    int disconnect(); //disconnect from the database
    MYSQL_RES *sqlexec(const char *query);
};
