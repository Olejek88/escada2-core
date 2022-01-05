//
// Created by koputo on 05.01.22.
//

#include "EntityParameter.h"

//EntityParameter::EntityParameter(DBase *dBase) : dBase(dBase) {}

bool EntityParameter::loadParameter(const std::string &deviceUuid, const std::string &parameterName) {
    char query[1024] = {0};

    sprintf(query, "SELECT * FROM entity_parameter ept WHERE entityUuid = '%s' AND parameter = '%s';",
            deviceUuid.data(), parameterName.data());
    printf("Select parameter: %s\n", query);
    MYSQL_RES *res = dBase->sqlexec(query);
    if (res) {
        my_ulonglong nRows = mysql_num_rows(res);
        if (nRows == 1) {
            dBase->makeFieldsList(res);
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row != nullptr) {
                _id = std::stoll(dBase->getFieldValue(row, "_id"));
                uuid = row[dBase->getFieldIndex("uuid")];
                entityUuid = deviceUuid;
                parameter = parameterName;
                value = dBase->getFieldValue(row, "value");
                createdAt = dBase->getFieldValue(row, "createdAt");
                changedAt = dBase->getFieldValue(row, "changedAt");
                isNew = false;
                mysql_free_result(res);
                return true;
            }
        }

        mysql_free_result(res);
    }

    return false;
}

bool EntityParameter::save() {
    char query[1024] = {0};

    if (isNew) {
        time_t createTime = time(nullptr);
        sprintf(query, "INSERT INTO entity_parameter (uuid, entityUuid, parameter, value, createdAt, changedAt) "
                       "VALUE('%s', '%s', '%s', '%s', FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld))",
                uuid.data(),
                entityUuid.data(),
                parameter.data(),
                value.data(),
                createTime,
                createTime);
        printf("Insert parameter: %s\n", query);
    } else {
        sprintf(query, "UPDATE entity_parameter SET _id=%ld, uuid='%s', entityUuid='%s',"
                       " parameter='%s', value='%s', changedAt = FROM_UNIXTIME(%ld) WHERE uuid = '%s'",
                _id,
                uuid.data(),
                entityUuid.data(),
                parameter.data(),
                value.data(),
                time(nullptr),
                uuid.data());
        printf("Update parameter: %s\n", query);
    }

    MYSQL_RES *res = dBase->sqlexec(query);
    if (res) {
        mysql_free_result(res);
    }

    return dBase->isError();
}