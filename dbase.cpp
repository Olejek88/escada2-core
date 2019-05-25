#include "errors.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstddef>

#define MODULE_NAME    "[dbase]"

#include "dbase.h"
#include "main.h"
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

//---------------------------------------------------------------------------------------------------
// function store archive data to database
bool DBase::StoreData(uint16_t type, uint16_t status, double value, char  *data, uint16_t channel) {
    MYSQL_RES *res;
    char query[500];
    if (type==TYPE_CURRENTS) {
        sprintf(query, "SELECT * FROM data WHERE sensor_channel=%d AND type=%d", channel, type);
        res = sqlexec(query);
        if (res && (mysql_fetch_row(res))) {
            sprintf(query, "UPDATE data SET value=%f, date=date WHERE sensor_channel='%d' AND type='%d'",
                    value, channel, type);
            res = sqlexec(query);
        } else {
            sprintf(query, "INSERT INTO data(type,value,sensor_channel,status,measure_type) VALUES('0','%d','%f','%d','%d','%s','%d')",
                    type, value, channel, status, data, 0);
            res = sqlexec(query);
        }
        if (res) mysql_free_result(res);
        return true;
    } else {
        sprintf(query, "SELECT * FROM data WHERE sensor_channel=%d AND type=%d AND date='%s'", channel, type, data);
        res = sqlexec(query);
        if (res && (mysql_fetch_row(res))) {
            sprintf(query,
                    "UPDATE data SET value=%f,status=%d,date=date WHERE type='%d' AND sensor_channel=%d AND date='%s'",
                    value, status, type, channel, data);
            res = sqlexec(query);
        }
        if (res) mysql_free_result(res);
    }
    return true;
}

//-----------------------------------------------------------------------------    
uint16_t DBase::GetChannel(uint16_t measureType, uint16_t channel, uint16_t device) {
    MYSQL_RES *res;
    MYSQL_ROW row;
    char query[500];
    // TODO если несколько каналов одного типа на устройстве
    sprintf(query, "SELECT * FROM sensor_channel WHERE measureType=%d AND device=%d", measureType, device);
    res = sqlexec(query);
    if (res) {
        row = mysql_fetch_row(res);
        if (row)
            return (uint16_t)atoi(row[0]);
    }
    return 0;
}

