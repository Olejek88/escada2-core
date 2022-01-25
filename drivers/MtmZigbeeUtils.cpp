#include <mysql/mysql.h>
#include <cstring>
#include "kernel.h"
#include "dbase.h"
#include "MtmZigbee.h"
#include "ce102.h"
#include <uuid/uuid.h>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/value.h>
#include <suninfo.h>
#include <function.h>
#include "main.h"
#include <ctime>
#include "drivers/SensorChannel.h"
#include "drivers/Device.h"
#include "lightUtils.h"

extern bool isSunInit;
extern bool isSunSet, isTwilightEnd, isTwilightStart, isSunRise;
extern int coordinatorFd;
extern std::string coordinatorUuid;

void log_buffer_hex(uint8_t *buffer, size_t buffer_size) {
    Kernel *kernel = &Kernel::Instance();
    uint8_t message[1024];
    for (int i = 0; i < buffer_size; i++) {
        sprintf((char *) &message[i * 2], "%02X", buffer[i]);
    }

    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] MTM packet: %s", TAG, message);
}

bool findDevice(DBase *dBase, std::string *deviceUuid, uint8_t *uuid) {
    ulong nRows;
    uint32_t nFields;
    MYSQL_FIELD *field;
    int32_t fieldUuidIdx = -1;
    const char *fieldUuid = "uuid";
    MYSQL_ROW row;
    unsigned long *lengths;
    long flen;
    uint8_t query[1024];
    MYSQL_RES *res;

    sprintf((char *) query, "SELECT * FROM device WHERE uuid = '%s' AND deleted=0", deviceUuid->data());
    res = dBase->sqlexec((char *) query);
    if (res) {
        nRows = mysql_num_rows(res);
        if (nRows == 1) {
            nFields = mysql_num_fields(res);
            char *headers[nFields];

            for (uint32_t i = 0; (field = mysql_fetch_field(res)); i++) {
                headers[i] = field->name;
                if (strcmp(fieldUuid, headers[i]) == 0) {
                    fieldUuidIdx = i;
                }
            }

            row = mysql_fetch_row(res);
            lengths = mysql_fetch_lengths(res);
            if (row) {
                flen = lengths[fieldUuidIdx];
                strncpy((char *) uuid, row[fieldUuidIdx], flen);
                mysql_free_result(res);
            } else {
                mysql_free_result(res);
                return false;
            }
        } else {
            mysql_free_result(res);
            return false;
        }
    } else {
        return false;
    }

    return true;
}

Device *findDeviceByAddress(DBase *dBase, std::string *address) {
    ulong nRows;
    MYSQL_ROW row;
    uint8_t query[1024];
    MYSQL_RES *res;

    sprintf((char *) query, "SELECT * FROM device WHERE address = '%s' AND deleted=0", address->data());
    res = dBase->sqlexec((char *) query);
    if (res) {
        nRows = mysql_num_rows(res);
        if (nRows == 1) {
            auto item = new Device();
            dBase->makeFieldsList(res);
            row = mysql_fetch_row(res);
            if (row) {
                item->_id = std::stoull(dBase->getFieldValue(row, "_id"));
                item->uuid = dBase->getFieldValue(row, "uuid");
                item->name = dBase->getFieldValue(row, "name");
                item->address = dBase->getFieldValue(row, "address");
                item->deviceTypeUuid = dBase->getFieldValue(row, "deviceTypeUuid");
                item->serial = dBase->getFieldValue(row, "serial");
                item->port = dBase->getFieldValue(row, "port");
                item->interface = std::stoi(dBase->getFieldValue(row, "interface"));
                item->deviceStatusUuid = dBase->getFieldValue(row, "deviceStatusUuid");
                item->last_date = dBase->getFieldValue(row, "last_date");
                item->nodeUuid = dBase->getFieldValue(row, "nodeUuid");
                item->linkTimeout = std::stoi(dBase->getFieldValue(row, "linkTimeout"));
                item->q_att = std::stoi(dBase->getFieldValue(row, "q_att"));
                item->q_errors = std::stoi(dBase->getFieldValue(row, "q_errors"));
                item->dev_time = dBase->getFieldValue(row, "dev_time");
                item->protocol = std::stoi(dBase->getFieldValue(row, "protocol"));
                item->number = dBase->getFieldValue(row, "number");
                item->createdAt = dBase->getFieldValue(row, "createdAt");
                item->changedAt = dBase->getFieldValue(row, "changedAt");
                mysql_free_result(res);
                return item;
            } else {
                mysql_free_result(res);
                return nullptr;
            }

        } else {
            mysql_free_result(res);
            return nullptr;
        }
    } else {
        return nullptr;
    }
}

std::string getSChannelConfig(DBase *dBase, std::string *sChannelUuid) {
    ulong nRows;
    MYSQL_ROW row;
    uint8_t query[1024];
    MYSQL_RES *res;
    std::string config;

    sprintf((char *) query, "SELECT * FROM sensor_config WHERE sensorChannelUuid = '%s'", sChannelUuid->data());
    res = dBase->sqlexec((char *) query);
    if (res) {
        nRows = mysql_num_rows(res);
        if (nRows > 0) {
            dBase->makeFieldsList(res);
            row = mysql_fetch_row(res);
            if (row) {
                config = std::string(row[dBase->getFieldIndex("config")]);
                mysql_free_result(res);
            } else {
                mysql_free_result(res);
            }
        } else {
            mysql_free_result(res);
        }
    }

    return config;
}

std::string findSChannel(DBase *dBase, uint8_t *deviceUuid, uint8_t regIdx, const char *measureType) {
    ulong nRows;
    MYSQL_ROW row;
    uint8_t query[1024];
    MYSQL_RES *res;
    std::string result;

    sprintf((char *) query,
            "SELECT * FROM sensor_channel WHERE deviceUuid = '%s' AND register = '%d' AND measureTypeUuid = '%s'",
            deviceUuid, regIdx, measureType);
    res = dBase->sqlexec((char *) query);
    if (res) {
        nRows = mysql_num_rows(res);
        if (nRows > 0) {
            dBase->makeFieldsList(res);
            row = mysql_fetch_row(res);
            if (row) {
                result = std::string(row[dBase->getFieldIndex("uuid")]);
                mysql_free_result(res);
            } else {
                mysql_free_result(res);
            }
        } else {
            mysql_free_result(res);
        }
    }

    return result;
}

SensorChannel *findSensorChannelsByDevice(DBase *dBase, std::string *deviceUuid, uint16_t *size) {
    ulong nRows;
    MYSQL_ROW row;
    uint8_t query[1024];
    MYSQL_RES *res;
    SensorChannel *list = nullptr;

    *size = 0;

    sprintf((char *) query, "SELECT * FROM sensor_channel WHERE deviceUuid = '%s'", deviceUuid->data());
    res = dBase->sqlexec((char *) query);
    if (res) {
        nRows = mysql_num_rows(res);
        if (nRows > 0) {
            *size = nRows;
            list = new SensorChannel[nRows];
            dBase->makeFieldsList(res);
            uint16_t idx = 0;
            while ((row = mysql_fetch_row(res)) != nullptr) {
                list[idx]._id = std::stoll(dBase->getFieldValue(row, "_id"));
                list[idx].uuid = dBase->getFieldValue(row, "uuid");
                list[idx].title = dBase->getFieldValue(row, "title");
                char *reg = dBase->getFieldValue(row, "register");
                if (!std::string(reg).empty()) {
                    try {
                        list[idx].reg = std::stoi(reg);
                    } catch (std::invalid_argument &e) {
                        list[idx].reg = -1;
                    }
                } else {
                    list[idx].reg = -1;
                }

                list[idx].deviceUuid = dBase->getFieldValue(row, "deviceUuid");
                list[idx].measureTypeUuid = dBase->getFieldValue(row, "measureTypeUuid");
                list[idx].createdAt = dBase->getFieldValue(row, "createdAt");
                list[idx].changedAt = dBase->getFieldValue(row, "changedAt");
                idx++;
            }

            mysql_free_result(res);
            return list;
        } else {
            mysql_free_result(res);
            return nullptr;
        }
    } else {
        return nullptr;
    }
}

bool updateMeasureValue(DBase *dBase, uint8_t *uuid, double value, time_t changedTime) {

    MYSQL_RES *res;
    char query[1024];
    Kernel *kernel = &Kernel::Instance();

    sprintf(query,
            "UPDATE data SET value=%f, date=FROM_UNIXTIME(%ld), changedAt=FROM_UNIXTIME(%ld) WHERE uuid = '%s'",
            value, changedTime, changedTime, uuid);
    if (kernel->isDebug) {
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s", TAG, query);
    }

    res = dBase->sqlexec((const char *) query);
    if (res) {
        mysql_free_result(res);
    }

    return dBase->isError();
}

bool updateMeasureValueExt(DBase *dBase, uint8_t *uuid, int32_t type, double value, time_t changedTime) {

    MYSQL_RES *res;
    char query[1024];
    Kernel *kernel = &Kernel::Instance();

    sprintf(query,
            "UPDATE data SET type=%d, value=%f, date=FROM_UNIXTIME(%ld), changedAt=FROM_UNIXTIME(%ld) WHERE uuid = '%s'",
            type, value, changedTime, changedTime, uuid);
    if (kernel->isDebug) {
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s", TAG, query);
    }

    res = dBase->sqlexec((const char *) query);
    if (res) {
        mysql_free_result(res);
    }

    return dBase->isError();
}

bool storeMeasureValue(DBase *dBase, uint8_t *uuid, std::string *channelUuid, double value, time_t createTime,
                       time_t changedTime) {
    MYSQL_RES *res;
    char query[1024];
    Kernel *kernel = &Kernel::Instance();

    sprintf(query,
            "INSERT INTO data (uuid, sensorChannelUuid, value, date, createdAt, changedAt) value('%s', '%s', %f, FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld))",
            uuid, channelUuid->data(), value, createTime, changedTime, changedTime);
    if (kernel->isDebug) {
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s", TAG, query);
    }

    res = dBase->sqlexec((const char *) query);
    if (res) {
        mysql_free_result(res);
    }

    return dBase->isError();
}

bool insertMeasureValue(DBase *dBase, uint8_t *uuid, std::string *channelUuid, int32_t type, double value,
                        time_t createTime, time_t changedTime) {
    MYSQL_RES *res;
    char query[1024];
    Kernel *kernel = &Kernel::Instance();

    sprintf(query,
            "INSERT INTO data (uuid, sensorChannelUuid, type, value, date, createdAt, changedAt) value('%s', '%s', %d, %f, FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld))",
            uuid, channelUuid->data(), type, value, createTime, changedTime, changedTime);
    if (kernel->isDebug) {
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s", TAG, query);
    }

    res = dBase->sqlexec((const char *) query);
    if (res) {
        mysql_free_result(res);
    }

    return dBase->isError();
}

bool
createSChannel(DBase *dBase, uint8_t *uuid, const char *channelTitle, uint8_t sensorIndex, uint8_t *deviceUuid,
               const char *channelTypeUuid, time_t createTime) {
    char query[1024];
    MYSQL_RES *res;
    Kernel *kernel = &Kernel::Instance();

    sprintf((char *) query,
            "INSERT INTO sensor_channel (uuid, title, register, deviceUuid, measureTypeUuid, createdAt) value('%s', '%s', '%d', '%s', '%s', FROM_UNIXTIME(%ld))",
            uuid, channelTitle, sensorIndex, deviceUuid, channelTypeUuid, createTime);
    if (kernel->isDebug) {
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s", TAG, query);
    }

    res = dBase->sqlexec((const char *) query);
    if (res) {
        mysql_free_result(res);
    }

    return dBase->isError();
}

std::string findMeasure(DBase *dBase, std::string *sChannelUuid, uint8_t regIdx) {
    ulong nRows;
    MYSQL_ROW row;
    uint8_t query[1024];
    MYSQL_RES *res;
    std::string result;

    sprintf((char *) query, "SELECT * FROM data WHERE sensorChannelUuid = '%s' AND type = '%d'", sChannelUuid->data(),
            regIdx);
    res = dBase->sqlexec((char *) query);
    if (res) {
        nRows = mysql_num_rows(res);
        if (nRows > 0) {
            dBase->makeFieldsList(res);
            row = mysql_fetch_row(res);
            if (row) {
                result = std::string(row[dBase->getFieldIndex("uuid")]);
                mysql_free_result(res);
            } else {
                mysql_free_result(res);
                return result;
            }
        } else {
            mysql_free_result(res);
            return result;
        }
    } else {
        return result;
    }

    return result;
}

void makeCoordinatorStatus(DBase *dBase, uint8_t *address, const uint8_t *packetBuffer) {
    uint8_t deviceUuid[37];
    uuid_t newUuid;
    uint8_t newUuidString[37];
    std::string measureUuid;
    time_t createTime = time(nullptr);
    int threshold;
    Json::Reader reader;
    Json::Value obj;
    char message[1024];
    uint16_t oldValue;
    char query[1024];
    MYSQL_RES *res;
    MYSQL_ROW row;
    std::string addressString = std::string((char *) address);
    Kernel *kernel = &Kernel::Instance();

    memset(deviceUuid, 0, 37);
    if (!findDevice(dBase, &addressString, deviceUuid)) {
        sprintf(message, "Не удалось найти устройство с адресом %s", address);
        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, message);
        AddDeviceRegister(dBase, (char *) coordinatorUuid.data(), message);
        return;
    }

    // найти канал по устройству sensor_channel и regIdx
    std::string in1ChannelUuid = findSChannel(dBase, deviceUuid, MTM_ZB_CHANNEL_COORD_DOOR_IDX, CHANNEL_DOOR_STATE);
    if (!in1ChannelUuid.empty()) {
        uint16_t value = *(uint16_t *) (&packetBuffer[34]);
        // получаем конфигурацию канала измерения
        threshold = 1024;
        std::string config = getSChannelConfig(dBase, &in1ChannelUuid);
        if (!config.empty()) {
            reader.parse(config, obj); // reader can also read strings
            if (!obj["threshold"].empty()) {
                try {
                    threshold = stoi(obj["threshold"].asString());
                } catch (std::invalid_argument &invalidArgument) {
                }
            }
        }

        oldValue = 0;
        value = value > threshold;
        measureUuid = findMeasure(dBase, &in1ChannelUuid, MTM_ZB_CHANNEL_COORD_DOOR_IDX);
        if (!measureUuid.empty()) {
            sprintf(query, "SELECT * FROM data WHERE uuid='%s'", measureUuid.data());
            res = dBase->sqlexec(query);
            dBase->makeFieldsList(res);
            row = mysql_fetch_row(res);
            oldValue = (uint16_t) std::stoi(row[dBase->getFieldIndex("value")]);
            mysql_free_result(res);
            if (updateMeasureValue(dBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG,
                                  "Не удалось обновить измерение", MTM_ZB_CHANNEL_COORD_DOOR_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            memset(newUuidString, 0, 37);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (storeMeasureValue(dBase, newUuidString, &in1ChannelUuid, (double) value, createTime, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG,
                                  "Не удалось сохранить измерение", MTM_ZB_CHANNEL_COORD_DOOR_TITLE);
            }
        }

        if (oldValue != value) {
            sprintf(message, "Дверь %s.", value == 0 ? "закрыта" : "открыта");
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s", TAG, message);
            AddDeviceRegister(dBase, (char *) coordinatorUuid.data(), message);
        }
    }

    // найти канал по устройству sensor_channel и regIdx
    std::string in2ChannelUuid = findSChannel(dBase, deviceUuid, MTM_ZB_CHANNEL_COORD_CONTACTOR_IDX,
                                              CHANNEL_CONTACTOR_STATE);
    if (!in2ChannelUuid.empty()) {
        uint16_t value = *(uint16_t *) (&packetBuffer[36]);
        // получаем конфигурацию канала измерения
        threshold = 1024;
        std::string config = getSChannelConfig(dBase, &in2ChannelUuid);
        if (!config.empty()) {
            reader.parse(config, obj); // reader can also read strings
            if (!obj["threshold"].empty()) {
                try {
                    threshold = stoi(obj["threshold"].asString());
                } catch (std::invalid_argument &invalidArgument) {
                }
            }
        }

        oldValue = 0;
        value = value > threshold;
        measureUuid = findMeasure(dBase, &in2ChannelUuid, MTM_ZB_CHANNEL_COORD_CONTACTOR_IDX);
        if (!measureUuid.empty()) {
            sprintf(query, "SELECT * FROM data WHERE uuid='%s'", measureUuid.data());
            res = dBase->sqlexec(query);
            dBase->makeFieldsList(res);
            row = mysql_fetch_row(res);
            oldValue = (uint16_t) std::stoi(row[dBase->getFieldIndex("value")]);
            mysql_free_result(res);
            if (updateMeasureValue(dBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG,
                                  "Не удалось обновить измерение", MTM_ZB_CHANNEL_COORD_CONTACTOR_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (storeMeasureValue(dBase, newUuidString, &in2ChannelUuid, (double) value,
                                  createTime, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG,
                                  "Не удалось сохранить измерение", MTM_ZB_CHANNEL_COORD_CONTACTOR_TITLE);
            }
        }

        if (oldValue != value) {
            sprintf(message, "Контактор %s.", value == 0 ? "включен" : "отключен");
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s", TAG, message);
            AddDeviceRegister(dBase, (char *) coordinatorUuid.data(), message);
        }
    }

    // найти канал по устройству sensor_channel и regIdx (цифровой пин управления контактором)
    std::string digi1ChannelUuid = findSChannel(dBase, deviceUuid, MTM_ZB_CHANNEL_COORD_RELAY_IDX, CHANNEL_RELAY_STATE);
    if (!digi1ChannelUuid.empty()) {
        uint16_t value = *(uint16_t *) (&packetBuffer[32]);
        value &= 0x0040u;
        value = value >> 6; // NOLINT(hicpp-signed-bitwise)
        oldValue = 0;
        measureUuid = findMeasure(dBase, &digi1ChannelUuid, MTM_ZB_CHANNEL_COORD_RELAY_IDX);
        if (!measureUuid.empty()) {
            sprintf(query, "SELECT * FROM data WHERE uuid='%s'", measureUuid.data());
            res = dBase->sqlexec(query);
            dBase->makeFieldsList(res);
            row = mysql_fetch_row(res);
            oldValue = (uint16_t) std::stoi(row[dBase->getFieldIndex("value")]);
            mysql_free_result(res);
            if (updateMeasureValue(dBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG,
                                  "Не удалось обновить измерение", MTM_ZB_CHANNEL_COORD_RELAY_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (storeMeasureValue(dBase, newUuidString, &digi1ChannelUuid, (double) value, createTime, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG,
                                  "Не удалось сохранить измерение", MTM_ZB_CHANNEL_COORD_RELAY_TITLE);
            }
        }

        if (oldValue != value) {
            sprintf(message, "Реле контактора %s.", value == 0 ? "отключено" : "включено");
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s", TAG, message);
            AddDeviceRegister(dBase, (char *) coordinatorUuid.data(), message);
        }
    }
}

void storeMeasureValueExt(DBase *dBase, SensorChannel *sc, int16_t value, bool instant) {
    uuid_t newUuid;
    uint8_t newUuidString[37] = {0};
    time_t createTime = time(nullptr);
    Kernel *kernel = &Kernel::Instance();

    int8_t type = instant ? 0 : 1;
    std::string measureUuid = findMeasure(dBase, &sc->uuid, type);
    if (!measureUuid.empty() && instant) {
        if (updateMeasureValueExt(dBase, (uint8_t *) measureUuid.data(), type, value, createTime)) {
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, "Не удалось обновить измерение", sc->title.data());
        }
    } else {
        // создать новое измерение для канала
        uuid_generate(newUuid);
        uuid_unparse_upper(newUuid, (char *) newUuidString);
        if (insertMeasureValue(dBase, newUuidString, &sc->uuid, type, (double) value, createTime, createTime)) {
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, "Не удалось сохранить измерение", sc->title.data());
        }
    }
}

void makeCoordinatorTemperature(DBase *dBase, uint8_t *address, const uint8_t *packetBuffer) {
    uint8_t deviceUuid[37];
    std::string sChannelUuid;
    uuid_t newUuid;
    uint8_t newUuidString[37] = {0};
    std::string measureUuid;
    time_t createTime = time(nullptr);
    int8_t value;
    char message[1024];
    std::string addressString = std::string((char *) address);
    Kernel *kernel = &Kernel::Instance();

    memset(deviceUuid, 0, 37);
    if (!findDevice(dBase, &addressString, deviceUuid)) {
        sprintf(message, "Не удалось найти устройство с адресом %s", address);
        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s", TAG, message);
        AddDeviceRegister(dBase, (char *) coordinatorUuid.data(), message);
        return;
    }

    // найти канал по устройству sensor_channel и regIdx (температрута)
    sChannelUuid = findSChannel(dBase, deviceUuid, MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_IDX, CHANNEL_T);
    if (!sChannelUuid.empty()) {
        // температура лежит в двух байтах начиная с 21-го
        uint16_t tempCount = *(uint16_t *) &packetBuffer[21];
        value = (int8_t) ((tempCount - 1480) / 4.5) + 25;
        measureUuid = findMeasure(dBase, &sChannelUuid, MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_IDX);
        if (!measureUuid.empty()) {
            if (updateMeasureValue(dBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, "Не удалось обновить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (storeMeasureValue(dBase, newUuidString, &sChannelUuid, (double) value, createTime, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, "Не удалось сохранить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_TITLE);
            }
        }
    }
}

/**
 *
 * @param time
 * @param dtm
 */
void fillTimeStruct(double time, struct tm *dtm) {
    time = time + (double) dtm->tm_gmtoff / 3600;
    dtm->tm_hour = (int) time;
    dtm->tm_min = (int) ((time - dtm->tm_hour) * 60);
    dtm->tm_sec = (int) ((((time - dtm->tm_hour) * 60) - dtm->tm_min) * 60);
}

void checkAstroEvents(time_t currentTime, double lon, double lat, DBase *dBase, int32_t threadId) {
    struct tm ctm = {0};
    struct tm tmp_tm = {0};
    double rise, set;
    double twilightStart, twilightEnd;
    int rs;
    int civ;

    uint64_t sunRiseTime;
    uint64_t sunSetTime;
    uint64_t twilightStartTime;
    uint64_t twilightEndTime;
    uint64_t twilightLength;
    uint64_t nightLength;
    uint64_t calcNightLength;
    double nightRate;
    mtm_cmd_action action = {0};

    MYSQL_RES *res;
    MYSQL_ROW row;
    std::string query;

    Kernel *kernel = &Kernel::Instance();

    localtime_r(&currentTime, &ctm);

    rs = sun_rise_set(ctm.tm_year + 1900, ctm.tm_mon + 1, ctm.tm_mday, lon, lat, &rise, &set);
    bool isTimeAboveSunSet;
    bool isTimeLessSunSet;
    bool isTimeAboveSunRise;
    bool isTimeLessSunRise;
    civ = civil_twilight(ctm.tm_year + 1900, ctm.tm_mon + 1, ctm.tm_mday, lon, lat, &twilightStart, &twilightEnd);
    bool isTimeAboveTwilightStart;
    bool isTimeLessTwilightStart;
    bool isTimeAboveTwilightEnd;
    bool isTimeLessTwilightEnd;

    localtime_r(&currentTime, &tmp_tm);

    // расчитываем длительность сумерек по реальным данным восхода и начала сумерек
    fillTimeStruct(rise, &tmp_tm);
    sunRiseTime = mktime(&tmp_tm);
    fillTimeStruct(twilightStart, &tmp_tm);
    twilightStartTime = mktime(&tmp_tm);
    twilightLength = sunRiseTime - twilightStartTime;
    // расчитываем реальную длительность ночи, с сумерками
    fillTimeStruct(set, &tmp_tm);
    sunSetTime = mktime(&tmp_tm);
    nightLength = 86400 - (sunSetTime - sunRiseTime);


    // пытаемся получить данные из календаря
    sunRiseTime = 0;
    sunSetTime = 0;
    query.append(
            "SELECT unix_timestamp(nct.date) AS time, type FROM node_control AS nct WHERE DATE(nct.date)=CURRENT_DATE()");
    res = dBase->sqlexec(query.data());
    if (res != nullptr) {
        dBase->makeFieldsList(res);
        while ((row = mysql_fetch_row(res)) != nullptr) {
            if (std::stoi(row[dBase->getFieldIndex("type")]) == 0) {
                sunRiseTime = std::stoull(row[dBase->getFieldIndex("time")]);
            } else if (std::stoi(row[dBase->getFieldIndex("type")]) == 1) {
                sunSetTime = std::stoull(row[dBase->getFieldIndex("time")]);
            }
        }

        mysql_free_result(res);
    }


    if (rs == 0 && civ == 0) {
        if (sunRiseTime == 0) {
            fillTimeStruct(rise, &tmp_tm);
            sunRiseTime = mktime(&tmp_tm);
        }

        if (sunSetTime == 0) {
            fillTimeStruct(set, &tmp_tm);
            sunSetTime = mktime(&tmp_tm);
        }

        // расчитываем коэффициент как отношение расчитаной длительности ночи к реальной
        calcNightLength = 86400 - (sunSetTime - sunRiseTime);
        nightRate = (double) calcNightLength / nightLength;

        // расчитываем время начала/конца сумерек относительно рассвета/заката (которые возможно получили из календаря)
        // устанавливая их длительность пропорционально изменившейся длительности ночи
        twilightStartTime = sunRiseTime - (uint64_t) (twilightLength * nightRate);
        twilightEndTime = sunSetTime + (uint64_t) (twilightLength * nightRate);

        action.header.type = MTM_CMD_TYPE_ACTION;
        action.header.protoVersion = MTM_VERSION_0;
        action.device = MTM_DEVICE_LIGHT;

        isTimeAboveSunSet = currentTime >= sunSetTime;
        isTimeLessSunSet = currentTime < sunSetTime;
        isTimeAboveSunRise = currentTime >= sunRiseTime;
        isTimeLessSunRise = currentTime < sunRiseTime;

        isTimeAboveTwilightStart = currentTime >= twilightStartTime;
        isTimeLessTwilightStart = currentTime < twilightStartTime;
        isTimeAboveTwilightEnd = currentTime >= twilightEndTime;
        isTimeLessTwilightEnd = currentTime < twilightEndTime;

        if ((isTimeAboveSunSet && isTimeLessTwilightEnd) && (!isSunSet || !isSunInit)) {
            isSunInit = true;
            isSunSet = true;
            isTwilightEnd = false;
            isTwilightStart = false;
            isSunRise = false;

            // включаем контактор
            switchContactor(true, MBEE_API_DIGITAL_LINE7);
            char message[1024];
            sprintf(message, "Наступил закат, включаем реле контактора.");
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s", TAG, message);
            AddDeviceRegister(dBase, (char *) coordinatorUuid.data(), message);

            // даём задержку для того чтоб стартанули модули в светильниках
            // т.к. неизвестно, питаются они через контактор или всё время под напряжением
            sleep(5);

            // зажигаем светильники
            ssize_t rc;
//            rc = switchAllLight(100);
//            if (rc == -1) {
//                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
//                // останавливаем поток с целью его последующего автоматического запуска и инициализации
//                mtmZigbeeStopThread(dBase, threadId);
//                AddDeviceRegister(dBase, (char *) coordinatorUuid.data(),
//                                  (char *) "Ошибка записи в порт координатора");
//                return;
//            }

            // передаём команду "астро событие" "закат"
            action.data = (0x02 << 8 | 0x01); // NOLINT(hicpp-signed-bitwise)
            rc = send_mtm_cmd(coordinatorFd, 0xFFFF, &action, kernel);
            if (rc == -1) {
                lostZBCoordinator(dBase, threadId, &coordinatorUuid);
                return;
            }

            if (kernel->isDebug) {
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] закат", TAG);
            }
        } else if ((isTimeAboveTwilightEnd || isTimeLessTwilightStart) && (!isTwilightEnd || !isSunInit)) {
            isSunInit = true;
            isSunSet = false;
            isTwilightEnd = true;
            isTwilightStart = false;
            isSunRise = false;

            // включаем контактор
            switchContactor(true, MBEE_API_DIGITAL_LINE7);
            char message[1024];
            sprintf(message, "Наступил конец сумерек, включаем реле контактора.");
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s", TAG, message);
//            AddDeviceRegister(dBase, (char *) coordinatorUuid.data(), message);

            // даём задержку для того чтоб стартанули модули в светильниках
            // т.к. неизвестно, питаются они через контактор или всё время под напряжением
            sleep(5);

            // передаём команду "астро событие" "конец сумерек"
            action.data = (0x01 << 8 | 0x00); // NOLINT(hicpp-signed-bitwise)
            ssize_t rc = send_mtm_cmd(coordinatorFd, 0xFFFF, &action, kernel);
            if (rc == -1) {
                lostZBCoordinator(dBase, threadId, &coordinatorUuid);
                return;
            }

            if (kernel->isDebug) {
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] конец сумерек", TAG);
            }
        } else if ((isTimeAboveTwilightStart && isTimeLessSunRise) && (!isTwilightStart || !isSunInit)) {
            isSunInit = true;
            isSunSet = false;
            isTwilightEnd = false;
            isTwilightStart = true;
            isSunRise = false;

            // включаем контактор
            switchContactor(true, MBEE_API_DIGITAL_LINE7);
            char message[1024];
            sprintf(message, "Наступило начало сумерек, включаем реле контактора.");
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s", TAG, message);
//            AddDeviceRegister(dBase, (char *) coordinatorUuid.data(), message);

            // даём задержку для того чтоб стартанули модули в светильниках
            // т.к. неизвестно, питаются они через контактор или всё время под напряжением
            sleep(5);

            // передаём команду "астро событие" "начало сумерек"
            action.data = (0x03 << 8 | 0x00); // NOLINT(hicpp-signed-bitwise)
            ssize_t rc = send_mtm_cmd(coordinatorFd, 0xFFFF, &action, kernel);
            if (rc == -1) {
                lostZBCoordinator(dBase, threadId, &coordinatorUuid);
                return;
            }

            if (kernel->isDebug) {
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] начало сумерек", TAG);
            }
        } else if ((isTimeAboveSunRise && isTimeLessSunSet) && (!isSunRise || !isSunInit)) {
            isSunInit = true;
            isSunSet = false;
            isTwilightEnd = false;
            isTwilightStart = false;
            isSunRise = true;

            // выключаем контактор, гасим светильники, отправляем команду "восход"
            switchContactor(false, MBEE_API_DIGITAL_LINE7);
            char message[1024];
            sprintf(message, "Наступил восход, выключаем реле контактора.");
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s", TAG, message);
            AddDeviceRegister(dBase, (char *) coordinatorUuid.data(), message);

            // строим список неисправных светильников
            makeLostLightList(dBase, kernel);
            // на всякий случай, если светильники всегда под напряжением
            switchAllLight(0);
            // передаём команду "астро событие" "восход"
            action.data = (0x00 << 8 | 0x00); // NOLINT(hicpp-signed-bitwise)
            ssize_t rc = send_mtm_cmd(coordinatorFd, 0xFFFF, &action, kernel);
            if (rc == -1) {
                lostZBCoordinator(dBase, threadId, &coordinatorUuid);
                return;
            }

            if (kernel->isDebug) {
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] восход", TAG);
            }
        } else {
            // ситуация когда мы не достигли условий переключения состояния светильников
            // такого не должно происходить
        }
    } else {
        // TODO: добавить оставшуюся логику
        // Т.е. у нас может быть целый набор состояний. сонце никогда не восходит/заходит,
        // сумерки длятся постоянно(полярная ночь)/ни когда не наступают(полярный день)
        // нужно отрабатывать эти события!
//                    switch (rs) {
//                        case 0:
//
//                            break;
//                        case 1:
//                            // солнце всегда над горизонтом - принудительно отключаем контактор, гасим все светильники?
//                            break;
//                        case -1:
//                            // солнце всегда за горизонтом - принудительно включаем контактор, зажигаем все светильники?
//                            break;
//                        default:
//                            break;
//                    }
    }
}


ssize_t sendLightLevel(char *addrString, char *level) {
    mtm_cmd_action action = {0};
    Kernel *kernel = &Kernel::Instance();

    action.header.type = MTM_CMD_TYPE_ACTION;
    action.header.protoVersion = MTM_VERSION_0;
    action.device = MTM_DEVICE_LIGHT;
    action.data = std::stoi(level);
    uint64_t addr = std::stoull(addrString, nullptr, 16);
    return send_mtm_cmd_ext(coordinatorFd, addr, &action, kernel);
}

void mtmZigbeeStopThread(DBase *dBase, int32_t threadId) {
    char query[1024] = {0};
    MYSQL_RES *res;
    mtmZigbeeSetRun(false);
    // поток "остановили"
    sprintf(query, "UPDATE threads SET status=%d, changedAt=FROM_UNIXTIME(%lu) WHERE _id=%d", 0, time(nullptr),
            threadId);
    res = dBase->sqlexec((char *) query);
    mysql_free_result(res);
}

void mtmCheckLinkState(DBase *dBase) {
    std::string query;
    MYSQL_RES *res;
    MYSQL_ROW row;
    int nRows;
    int contactorState = 0; // считаем что контактор всегда включен, даже если его нет
    bool firstItem;
    std::string inParamList;
    char message[1024];
    std::string devType;
    Kernel *kernel = &Kernel::Instance();

    // проверяем состояние контактора, если он включен, тогда следим за состоянием светильников
    query = "SELECT mt.* FROM device AS dt ";
    query.append("LEFT JOIN sensor_channel AS sct ON sct.deviceUuid=dt.uuid ");
    query.append("LEFT JOIN data AS mt ON mt.sensorChannelUuid=sct.uuid ");
    query.append("WHERE dt.deviceTypeUuid='" + std::string(DEVICE_TYPE_ZB_COORDINATOR) + "' ");
    query.append("AND sct.measureTypeUuid='" + std::string(CHANNEL_CONTACTOR_STATE) + "' ");
    query.append("AND deleted=0");
    if (kernel->isDebug) {
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] contactor query: %s", TAG, query.data());
    }

    res = dBase->sqlexec(query.data());
    if (res) {
        nRows = mysql_num_rows(res);
        if (nRows > 0) {
            dBase->makeFieldsList(res);
            row = mysql_fetch_row(res);
            if (row) {
                char *value = row[dBase->getFieldIndex("value")];
                if (value != nullptr) {
                    contactorState = std::stoi(std::string(value));
                }
            }
        }

        mysql_free_result(res);
    }

    // если контактор не включен, ни чего не делаем
    if (contactorState != 0) {
        return;
    }

    // для всех светильников от которых не было пакетов со статусом более linkTimeOut секунд,
    // а статус был "В порядке", устанавливаем статус "Нет связи"
    // для этого, сначала выбираем все устройства которые будут менять статус
    query = "SELECT dt.uuid, dt.address as devAddr, nt.address as nodeAddr, dt.deviceTypeUuid FROM device AS dt ";
    query.append("LEFT JOIN node AS nt ON nt.uuid=dt.nodeUuid ");
    query.append("LEFT JOIN sensor_channel AS sct ON sct.deviceUuid=dt.uuid ");
    query.append("LEFT JOIN data AS mt ON mt.sensorChannelUuid=sct.uuid ");
    query.append("WHERE (timestampdiff(second,  mt.changedAt, current_timestamp()) > dt.linkTimeout ");
    query.append("OR mt.changedAt IS NULL) ");
    query.append("AND (");
    query.append("(dt.deviceTypeUuid='" + std::string(DEVICE_TYPE_ZB_LIGHT)
                 + "' AND sct.measureTypeUuid='" + std::string(CHANNEL_STATUS) + "')");
    query.append(" OR ");
    query.append("(dt.deviceTypeUuid='" + std::string(DEVICE_TYPE_ZB_COORDINATOR)
                 + "' AND sct.measureTypeUuid='" + std::string(CHANNEL_RELAY_STATE) + "')");
    query.append(") ");
    query.append("AND dt.deviceStatusUuid='" + std::string(DEVICE_STATUS_WORK) + "' ");
    query.append("AND deleted=0 ");
    query.append("GROUP BY dt.uuid");
    if (kernel->isDebug) {
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] select to not link query: %s", TAG, query.data());
    }

    res = dBase->sqlexec(query.data());
    firstItem = true;
    if (res) {
        nRows = mysql_num_rows(res);
        if (nRows > 0) {
            dBase->makeFieldsList(res);
            while ((row = mysql_fetch_row(res)) != nullptr) {
                if (row) {
                    if (firstItem) {
                        firstItem = false;
                        inParamList = "'" + std::string(row[dBase->getFieldIndex("uuid")]) + "'";
                    } else {
                        inParamList += ", '" + std::string(row[dBase->getFieldIndex("uuid")]) + "'";
                    }

                    devType = row[dBase->getFieldIndex("deviceTypeUuid")];
                    sprintf(message, "Устройство изменило статус на \"Нет связи\" (%s)",
                            devType == std::string(DEVICE_TYPE_ZB_COORDINATOR) ? row[dBase->getFieldIndex("nodeAddr")]
                                                                               : row[dBase->getFieldIndex("devAddr")]);
                    AddDeviceRegister(dBase, (char *) std::string(row[dBase->getFieldIndex("uuid")]).data(), message);
                }
            }
        }

        mysql_free_result(res);
    }

    if (!inParamList.empty()) {
        query = "UPDATE device SET deviceStatusUuid='" + std::string(DEVICE_STATUS_NO_CONNECT) +
                "', changedAt=current_timestamp() ";
        query.append("WHERE device.uuid IN (" + inParamList + ")");
        if (kernel->isDebug) {
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] update to not link query: %s", TAG, query.data());
        }

        res = dBase->sqlexec(query.data());
        mysql_free_result(res);
    }

    // для всех светильников от которых были получены пакеты со статусом менее limitTimeOut секунд назад,
    // а статус был "Нет связи", устанавливаем статус "В порядке"
    query = "SELECT dt.uuid, dt.address as devAddr, nt.address as nodeAddr, dt.deviceTypeUuid FROM device AS dt ";
    query.append("LEFT JOIN node AS nt ON nt.uuid=dt.nodeUuid ");
    query.append("LEFT JOIN sensor_channel AS sct ON sct.deviceUuid=dt.uuid ");
    query.append("LEFT JOIN data as mt on mt.sensorChannelUuid=sct.uuid ");
    query.append("WHERE (timestampdiff(second,  mt.changedAt, current_timestamp()) < dt.linkTimeout) ");
    query.append("AND (");
    query.append("(dt.deviceTypeUuid='" + std::string(DEVICE_TYPE_ZB_LIGHT)
                 + "' AND sct.measureTypeUuid='" + std::string(CHANNEL_STATUS) + "')");
    query.append(" OR ");
    query.append("(dt.deviceTypeUuid='" + std::string(DEVICE_TYPE_ZB_COORDINATOR)
                 + "' AND sct.measureTypeUuid='" + std::string(CHANNEL_RELAY_STATE) + "')");
    query.append(") ");
    query.append("AND dt.deviceStatusUuid='" + std::string(DEVICE_STATUS_NO_CONNECT) + "' ");
    query.append("AND deleted=0 ");
    query.append("GROUP BY dt.uuid");
    if (kernel->isDebug) {
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] select to work query: %s", TAG, query.data());
    }

    res = dBase->sqlexec(query.data());
    firstItem = true;
    inParamList.clear();
    if (res) {
        nRows = mysql_num_rows(res);
        if (nRows > 0) {
            dBase->makeFieldsList(res);
            while ((row = mysql_fetch_row(res)) != nullptr) {
                if (row) {
                    if (firstItem) {
                        firstItem = false;
                        inParamList = "'" + std::string(row[dBase->getFieldIndex("uuid")]) + "'";
                    } else {
                        inParamList += ", '" + std::string(row[dBase->getFieldIndex("uuid")]) + "'";
                    }

                    devType = row[dBase->getFieldIndex("deviceTypeUuid")];
                    sprintf(message, "Устройство изменило статус на \"В порядке\" (%s)",
                            devType == std::string(DEVICE_TYPE_ZB_COORDINATOR) ? row[dBase->getFieldIndex("nodeAddr")]
                                                                               : row[dBase->getFieldIndex("devAddr")]);
                    AddDeviceRegister(dBase, (char *) std::string(row[dBase->getFieldIndex("uuid")]).data(), message);
                }
            }
        }

        mysql_free_result(res);
    }

    if (!inParamList.empty()) {
        query = "UPDATE device SET deviceStatusUuid='" + std::string(DEVICE_STATUS_WORK) +
                "', changedAt=current_timestamp() ";
        query.append("WHERE device.uuid IN (" + inParamList + ")");
        if (kernel->isDebug) {
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] update to work query: %s", TAG, query.data());
        }

        res = dBase->sqlexec(query.data());
        mysql_free_result(res);
    }

    // для всех устройств у которых нет каналов измерения (светильники zb, координатор) ставим нет связи
    query = "UPDATE device set deviceStatusUuid='" + std::string(DEVICE_STATUS_NO_CONNECT) +
            "', changedAt=current_timestamp() ";
    query.append("WHERE device.uuid NOT IN (");
    query.append("SELECT sct.deviceUuid FROM sensor_channel AS sct GROUP BY sct.deviceUuid");
    query.append(") ");
    query.append("AND device.deviceTypeUuid IN ('" + std::string(DEVICE_TYPE_ZB_LIGHT) + "', '" +
                 std::string(DEVICE_TYPE_ZB_COORDINATOR) + "') ");
    query.append("AND device.deviceStatusUuid='" + std::string(DEVICE_STATUS_WORK) + "' ");
    query.append("AND deleted=0");
    if (kernel->isDebug) {
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] update without measure to not link query: %s", TAG, query.data());
    }

    res = dBase->sqlexec(query.data());
    mysql_free_result(res);
}

void lostZBCoordinator(DBase *dBase, int32_t threadId, std::string *coordUuid) {
    std::string query;
    MYSQL_RES *res;
    Kernel *kernel = &Kernel::Instance();

    kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
    // меняем статус координатора на DEVICE_STATUS_NO_CONNECT
    query = "UPDATE device SET deviceStatusUuid='" + std::string(DEVICE_STATUS_NO_CONNECT) +
            "', changedAt=current_timestamp() WHERE uuid='" + *coordUuid + "'";
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s", TAG, query.data());
    res = dBase->sqlexec(query.data());
    mysql_free_result(res);

    // останавливаем поток с целью его последующего автоматического запуска и инициализации
    mtmZigbeeStopThread(dBase, threadId);
    AddDeviceRegister(dBase, (char *) coordUuid->data(), (char *) "Ошибка записи в порт координатора");
}

void setDeviceStatus(DBase *dBase, std::string uuid, std::string statusUuid) {
    std::string query;
    MYSQL_RES *res;
    Kernel *kernel = &Kernel::Instance();

    query = "UPDATE device SET deviceStatusUuid='" + statusUuid +
            "', changedAt=FROM_UNIXTIME(" + std::to_string(time(nullptr)) + ") WHERE uuid='" + uuid + "'";
    if (kernel->isDebug) {
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s", TAG, query.data());
    }

    res = dBase->sqlexec(query.data());
    mysql_free_result(res);
}