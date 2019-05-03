#include "errors.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstddef>

#define MODULE_NAME    "[dbase]"

#include "dbase.h"
#include "logs.h"
#include "kernel.h"
#include "tinyxml2.h"

DBase::DBase()
= default;

int DBase::openConnection() {
    Kernel &currentKernelInstance = Kernel::Instance();

    tinyxml2::XMLDocument doc;
    if (doc.LoadFile("config/escada.conf") == tinyxml2::XML_SUCCESS) {
        // TODO recode to get|set
        snprintf(driver, MAX_STR, "%s",
                 doc.FirstChildElement("database")->FirstChildElement("driver")->GetText());
        snprintf(user, MAX_STR, "%s", doc.FirstChildElement("database")->FirstChildElement("user")->GetText());
        snprintf(host, MAX_STR, "%s", doc.FirstChildElement("database")->FirstChildElement("host")->GetText());
        snprintf(pass, MAX_STR, "%s", doc.FirstChildElement("database")->FirstChildElement("pass")->GetText());
        snprintf(table, MAX_STR, "%s", doc.FirstChildElement("database")->FirstChildElement("table")->GetText());
    }

    char query[MAX_QUERY_LENGTH];
    if (strlen(pass) == 0)
        sprintf(pass, "");
    if (strlen(user) == 0)
        sprintf(user, "root");
    if (strlen(host) == 0)
        sprintf(host, "localhost");

    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "%s connection %s, %s, %s, %s", MODULE_NAME, driver, host, user,
                                    pass);

    mysql = mysql_init(nullptr);    // init mysql connection
    if (!mysql) {
        currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "%s init mysql database........failed [[%d] %s]", MODULE_NAME,
                                        mysql_errno(mysql), mysql_error(mysql));
        mysql_close(mysql);
        return ERROR;
    }
    if (!mysql_real_connect(mysql, host, user, pass, table, 0, nullptr, 0)) {
        currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "%s connecting to database........failed [[%d] %s]",
                                        MODULE_NAME, mysql_errno(mysql), mysql_error(mysql));
        mysql_close(mysql);
        return ERROR;
    } else {
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "%s connecting to database........success", MODULE_NAME);
    }
    snprintf(query, MAX_QUERY_LENGTH, "USE %s", table);
    mysql_real_query(mysql, query, strlen(query));
    //successfully connected to the database
    return OK;
}

int DBase::disconnect() {
    if (res) mysql_free_result(res);
    if (mysql) mysql_close(mysql);
    return OK;
}


MYSQL_RES *DBase::sqlexec(const char *query) {
    if (mysql) {
        mysql_query(mysql, query);
        //store the results
        res = mysql_store_result(mysql);
    }
    return res;
}


