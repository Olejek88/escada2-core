#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstddef>
#include "errors.h"
#include <uuid/uuid.h>

#define MODULE_NAME    "[dbase]"

#include "dbase.h"
#include "main.h"
#include "kernel.h"
#include "tinyxml2.h"
#include <string.h>

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
        snprintf(database, MAX_STR, "%s", doc.FirstChildElement("database")->FirstChildElement("database")->GetText());
    }

    char query[MAX_QUERY_LENGTH];
    if (strlen(pass) == 0)
        sprintf(pass, "%s", "");
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
    if (!mysql_real_connect(mysql, host, user, pass, database, 0, nullptr, 0)) {
        currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "%s connecting to database........failed [[%d] %s]",
                                        MODULE_NAME, mysql_errno(mysql), mysql_error(mysql));
        mysql_close(mysql);
        return ERROR;
    } else {
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "%s connecting to database........success", MODULE_NAME);
    }
    snprintf(query, MAX_QUERY_LENGTH, "USE %s", database);
    mysql_real_query(mysql, query, strlen(query));
    snprintf(query, MAX_QUERY_LENGTH, "SET CHARSET 'UTF8'");
    mysql_real_query(mysql, query, strlen(query));
    snprintf(query, MAX_QUERY_LENGTH, "SET CHARACTER_SET_CONNECTION = 'UTF8'");
    mysql_real_query(mysql, query, strlen(query));

    //successfully connected to the database
    return OK;
}

int DBase::disconnect() {
    if (mysql) mysql_close(mysql);
    mysql_library_end();
    return OK;
}


MYSQL_RES *DBase::sqlexec(const char *query) {
    MYSQL_RES *result = nullptr;
    if (mysql) {
        mysql_query(mysql, query);
        //store the results
        result = mysql_store_result(mysql);
    }

    return result;
}

//---------------------------------------------------------------------------------------------------
// function store archive data to database
bool DBase::StoreData(uint16_t type, uint16_t parameter, double value, char *data, char *channelUuid) {
    MYSQL_RES *res;
    char query[500];
    if (type == TYPE_CURRENTS || type == TYPE_TOTAL_CURRENT) {
        sprintf(query, "SELECT * FROM data WHERE sensorChannelUuid='%s' AND type=%d AND parameter=%d", channelUuid,
                type, parameter);
        pRes = sqlexec(query);
        if (res && (row = mysql_fetch_row(res))) {
            mysql_free_result(pRes);
            sprintf(query,
                    "UPDATE data SET value=%f, date=CURRENT_TIMESTAMP(), changedAt=CURRENT_TIMESTAMP() WHERE sensorChannelUuid='%s' AND type='%d' AND parameter='%d'",
                    value, channelUuid, type, parameter);
            res = sqlexec(query);
        } else {
            uuid_t newUuid;
            char newUuidString[37] = {0};
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, newUuidString);
            sprintf(query,
                    "INSERT INTO data(uuid, type,value,sensorChannelUuid, date) VALUES('%s', '0','%f','%s', CURRENT_TIMESTAMP())",
                    newUuidString, type, value, channelUuid, parameter);
            pRes = sqlexec(query);
            mysql_free_result(pRes);
        }

        return true;
    } else {
        sprintf(query, "SELECT * FROM data WHERE sensorChannelUuid='%s' AND type=%d AND date='%s' AND parameter=%d", channelUuid, type,
                data, parameter);
        pRes = sqlexec(query);
        printf("%s = %ld\n", query, pRes);
        if (pRes && (row = mysql_fetch_row(pRes))) {
            mysql_free_result(pRes);
//            printf("U row=%ld\n", row);
            sprintf(query,
                    "UPDATE data SET value=%f, date=date, changedAt=CURRENT_TIMESTAMP() WHERE type='%d' AND sensorChannelUuid='%s' AND date='%s'",
                    value, type, channelUuid, data);
            pRes = sqlexec(query);
            mysql_free_result(pRes);
        } else {
//            printf("I row=%ld %s\n", row, mysql_error(mysql));
            uuid_t newUuid;
            char newUuidString[37] = {0};
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, newUuidString);
            sprintf(query,
                    "INSERT INTO data(uuid, type, value, sensorChannelUuid, date, parameter, createdAt) VALUES('%s', '%d','%f','%s','%s', '%d', CURRENT_TIMESTAMP())",
                    newUuidString, type, value, channelUuid, data ,parameter);
            pRes = sqlexec(query);
            mysql_free_result(pRes);
        }
    }

    return true;
}

//-----------------------------------------------------------------------------    
char *DBase::GetChannel(char *measureTypeUuid, uint16_t channel, char *deviceUuid) {
    MYSQL_RES *pRes;
    MYSQL_ROW mysqlRow;
    MYSQL_ROW firstRow = nullptr;
    char query[500];
    // TODO если несколько каналов одного типа на устройстве
    sprintf(query, "SELECT * FROM sensor_channel WHERE measureTypeUuid='%s' AND deviceUuid='%s'", measureTypeUuid,
            deviceUuid);
    pRes = sqlexec(query);
    if (pRes) {
        mysqlRow = mysql_fetch_row(pRes);
        if (mysqlRow) {
            // тупо указываем второе поле, которое как предполагается будет uuid
            firstRow = &mysqlRow[1];
        }
    }

    if (firstRow != nullptr) {
        char *uuid = (char *) malloc(37);
        strncpy(uuid, (const char *) *firstRow, 36);
        uuid[36] = 0;
        mysql_free_result(pRes);
        return uuid;
    } else {
        return nullptr;
    }
}

int8_t DBase::makeFieldsList(MYSQL_RES *res) {
    MYSQL_FIELD *field;
    int8_t i;
    for (i = 0; (field = mysql_fetch_field(res)); i++) {
        strcpy(fields[i], field->name);
    }

    nFields = i;
    return i;
}

int8_t DBase::getFieldIndex(const char *fieldName) {
    for (uint32_t i = 0; i < nFields; i++) {
        if (strcmp(fieldName, fields[i]) == 0) {
            return i;
        }
    }

    return -1;
}

bool DBase::isError() {
    return *mysql_error(mysql) == 0;
}

const char *DBase::getErrorString() {
    return mysql_error(mysql);
}





