#include <mysql/mysql.h>
#include <cstring>
#include "kernel.h"
#include "dbase.h"
#include "MtmZigbee.h"
#include "ce102.h"
#include <uuid/uuid.h>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/value.h>

extern Kernel *kernel;
extern uint8_t TAG[];

void log_buffer_hex(uint8_t *buffer, size_t buffer_size) {
    uint8_t message[1024];
    for (int i = 0; i < buffer_size; i++) {
        sprintf((char *) &message[i * 6], "0x%02x, ", buffer[i]);
    }

    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s\n", TAG, message);
}

bool findDevice(DBase *mtmZigbeeDBase, uint8_t *addr, uint8_t *uuid) {
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

    sprintf((char *) query, "SELECT * FROM device WHERE address LIKE '%s'", addr);
    res = mtmZigbeeDBase->sqlexec((char *) query);
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

std::string getSChannelConfig(DBase *mtmZigbeeDBase, std::string *sChannelUuid) {
    ulong nRows;
    MYSQL_ROW row;
    uint8_t query[1024];
    MYSQL_RES *res;
    std::string config;

    sprintf((char *) query,
            "SELECT * FROM sensor_config WHERE sensorChannelUuid = '%s'", sChannelUuid->data());
    res = mtmZigbeeDBase->sqlexec((char *) query);
    if (res) {
        nRows = mysql_num_rows(res);
        if (nRows == 1) {
            mtmZigbeeDBase->makeFieldsList(res);
            row = mysql_fetch_row(res);
            if (row) {
                config = std::string(row[mtmZigbeeDBase->getFieldIndex("config")]);
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

std::string findSChannel(DBase *mtmZigbeeDBase, uint8_t *deviceUuid, uint8_t regIdx, const char *measureType) {
    ulong nRows;
    MYSQL_ROW row;
    uint8_t query[1024];
    MYSQL_RES *res;
    std::string result;

    sprintf((char *) query,
            "SELECT * FROM sensor_channel WHERE deviceUuid = '%s' AND register = '%d' AND measureTypeUuid = '%s'",
            deviceUuid, regIdx, measureType);
    res = mtmZigbeeDBase->sqlexec((char *) query);
    if (res) {
        nRows = mysql_num_rows(res);
        if (nRows == 1) {
            mtmZigbeeDBase->makeFieldsList(res);
            row = mysql_fetch_row(res);
            if (row) {
                result = std::string(row[mtmZigbeeDBase->getFieldIndex("uuid")]);
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

bool updateMeasureValue(DBase *mtmZigbeeDBase, uint8_t *uuid, double value, time_t changedTime) {

    MYSQL_RES *res;
    char query[1024];

    sprintf(query,
            "UPDATE data SET value=%f, date=FROM_UNIXTIME(%ld), changedAt=FROM_UNIXTIME(%ld) WHERE uuid = '%s'",
            value, changedTime, changedTime, uuid);
#ifdef DEBUG
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s\n", TAG, query);
#endif

    res = mtmZigbeeDBase->sqlexec((const char *) query);
    if (res) {
        mysql_free_result(res);
    }

    return mtmZigbeeDBase->isError();
}

bool storeMeasureValue(DBase *mtmZigbeeDBase, uint8_t *uuid, std::string *channelUuid, double value, time_t createTime,
                       time_t changedTime) {
    MYSQL_RES *res;
    char query[1024];

    sprintf(query,
            "INSERT INTO data (uuid, sensorChannelUuid, value, date, createdAt) value('%s', '%s', %f, FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld))",
            uuid, channelUuid->data(), value, createTime, changedTime);
#ifdef DEBUG
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s\n", TAG, query);
#endif

    res = mtmZigbeeDBase->sqlexec((const char *) query);
    if (res) {
        mysql_free_result(res);
    }

    return mtmZigbeeDBase->isError();
}

bool
createSChannel(DBase *mtmZigbeeDBase, uint8_t *uuid, const char *channelTitle, uint8_t sensorIndex, uint8_t *deviceUuid,
               const char *channelTypeUuid, time_t createTime) {
    char query[1024];
    MYSQL_RES *res;
    sprintf((char *) query,
            "INSERT INTO sensor_channel (uuid, title, register, deviceUuid, measureTypeUuid, createdAt) value('%s', '%s', '%d', '%s', '%s', FROM_UNIXTIME(%ld))",
            uuid, channelTitle, sensorIndex, deviceUuid, channelTypeUuid, createTime);
#ifdef DEBUG
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s\n", TAG, query);
#endif

    res = mtmZigbeeDBase->sqlexec((const char *) query);
    if (res) {
        mysql_free_result(res);
    }

    return mtmZigbeeDBase->isError();
}

std::string findMeasure(DBase *mtmZigbeeDBase, std::string *sChannelUuid, uint8_t regIdx) {
    ulong nRows;
    MYSQL_ROW row;
    uint8_t query[1024];
    MYSQL_RES *res;
    std::string result;

    sprintf((char *) query, "SELECT * FROM data WHERE sensorChannelUuid = '%s' AND type = '%d'", sChannelUuid->data(),
            regIdx);
    res = mtmZigbeeDBase->sqlexec((char *) query);
    if (res) {
        nRows = mysql_num_rows(res);
        if (nRows == 1) {
            mtmZigbeeDBase->makeFieldsList(res);
            row = mysql_fetch_row(res);
            if (row) {
                result = std::string(row[mtmZigbeeDBase->getFieldIndex("uuid")]);
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

void makeCoordinatorStatus(DBase *mtmZigbeeDBase, uint8_t *address, const uint8_t *packetBuffer) {
    uint8_t deviceUuid[37];
    std::string sChannelUuid;
    uuid_t newUuid;
    uint8_t newUuidString[37];
    std::string measureUuid;
    time_t createTime = time(nullptr);
    int threshold;
    Json::Reader reader;
    Json::Value obj;

    memset(deviceUuid, 0, 37);
    if (!findDevice(mtmZigbeeDBase, address, deviceUuid)) {
        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Неудалось найти устройство с адресом %s", address);
        return;
    }

    // найти канал по устройству sensor_channel и regIdx
    sChannelUuid = findSChannel(mtmZigbeeDBase, deviceUuid, MTM_ZB_CHANNEL_COORD_IN1_IDX, CHANNEL_IN1);
    if (sChannelUuid.empty()) {
        // если нет, создать
        uuid_generate(newUuid);
        uuid_unparse_upper(newUuid, (char *) newUuidString);
        if (!createSChannel(mtmZigbeeDBase, newUuidString, MTM_ZB_CHANNEL_COORD_IN1_TITLE,
                            MTM_ZB_CHANNEL_COORD_IN1_IDX, deviceUuid, CHANNEL_IN1, createTime)) {
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Неудалось канал измерение",
                              MTM_ZB_CHANNEL_COORD_IN1_TITLE);
        } else {
            sChannelUuid = std::string((char *) newUuidString);
        }
    }

    if (!sChannelUuid.empty()) {
        uint16_t value = *(uint16_t *) (&packetBuffer[34]);
        // получаем конфигурацию канала измерения
        threshold = 1024;
        std::string config = getSChannelConfig(mtmZigbeeDBase, &sChannelUuid);
        if (!config.empty()) {
            reader.parse(config, obj); // reader can also read strings
            if (!obj["threshold"].empty()) {
                try {
                    threshold = stoi(obj["threshold"].asString());
                } catch (std::invalid_argument &invalidArgument) {
                }
            }
        }

        value = value > threshold;
        measureUuid = findMeasure(mtmZigbeeDBase, &sChannelUuid, MTM_ZB_CHANNEL_COORD_IN1_IDX);
        if (!measureUuid.empty()) {
            if (!updateMeasureValue(mtmZigbeeDBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG,
                                  "Не удалось обновить измерение", MTM_ZB_CHANNEL_COORD_IN1_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            memset(newUuidString, 0, 37);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (!storeMeasureValue(mtmZigbeeDBase, newUuidString, &sChannelUuid, (double) value, createTime,
                                   createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG,
                                  "Не удалось сохранить измерение", MTM_ZB_CHANNEL_COORD_IN1_TITLE);
            }
        }
    }

    // найти канал по устройству sensor_channel и regIdx
    sChannelUuid = findSChannel(mtmZigbeeDBase, deviceUuid, MTM_ZB_CHANNEL_COORD_IN2_IDX, CHANNEL_IN2);
    if (sChannelUuid.empty()) {
        // если нет, создать
        uuid_generate(newUuid);
        uuid_unparse_upper(newUuid, (char *) newUuidString);
        if (!createSChannel(mtmZigbeeDBase, newUuidString, MTM_ZB_CHANNEL_COORD_IN2_TITLE,
                            MTM_ZB_CHANNEL_COORD_IN2_IDX, deviceUuid, CHANNEL_IN2, createTime)) {
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Неудалось канал измерение",
                              MTM_ZB_CHANNEL_COORD_IN2_TITLE);
        } else {
            sChannelUuid = std::string((char *) newUuidString);
        }
    }

    if (!sChannelUuid.empty()) {
        uint16_t value = *(uint16_t *) (&packetBuffer[36]);
        // получаем конфигурацию канала измерения
        threshold = 1024;
        std::string config = getSChannelConfig(mtmZigbeeDBase, &sChannelUuid);
        if (!config.empty()) {
            reader.parse(config, obj); // reader can also read strings
            if (!obj["threshold"].empty()) {
                try {
                    threshold = stoi(obj["threshold"].asString());
                } catch (std::invalid_argument &invalidArgument) {
                }
            }
        }

        value = value > threshold;
        measureUuid = findMeasure(mtmZigbeeDBase, &sChannelUuid, MTM_ZB_CHANNEL_COORD_IN2_IDX);
        if (!measureUuid.empty()) {
            if (!updateMeasureValue(mtmZigbeeDBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG,
                                  "Не удалось обновить измерение", MTM_ZB_CHANNEL_COORD_IN2_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (!storeMeasureValue(mtmZigbeeDBase, (uint8_t *) measureUuid.data(), &sChannelUuid, (double) value,
                                   createTime, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG,
                                  "Не удалось сохранить измерение", MTM_ZB_CHANNEL_COORD_IN2_TITLE);
            }
        }
    }

    // найти канал по устройству sensor_channel и regIdx (цифровой пин управления контактором)
    sChannelUuid = findSChannel(mtmZigbeeDBase, deviceUuid, MTM_ZB_CHANNEL_COORD_DIGI1_IDX, CHANNEL_DIGI1);
    if (sChannelUuid.empty()) {
        // если нет, создать
        uuid_generate(newUuid);
        uuid_unparse_upper(newUuid, (char *) newUuidString);
        if (!createSChannel(mtmZigbeeDBase, newUuidString, MTM_ZB_CHANNEL_COORD_DIGI1_TITLE,
                            MTM_ZB_CHANNEL_COORD_DIGI1_IDX, deviceUuid, CHANNEL_DIGI1,
                            createTime)) {
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Неудалось канал измерение",
                              MTM_ZB_CHANNEL_COORD_DIGI1_TITLE);
        } else {
            sChannelUuid = std::string((char *) newUuidString);
        }
    }

    if (!sChannelUuid.empty()) {
        uint16_t value = *(uint16_t *) (&packetBuffer[32]);
        value &= 0x0040u;
        value = value >> 6; // NOLINT(hicpp-signed-bitwise)
        measureUuid = findMeasure(mtmZigbeeDBase, &sChannelUuid, MTM_ZB_CHANNEL_COORD_DIGI1_IDX);
        if (!measureUuid.empty()) {
            if (!updateMeasureValue(mtmZigbeeDBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG,
                                  "Не удалось обновить измерение", MTM_ZB_CHANNEL_COORD_DIGI1_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (!storeMeasureValue(mtmZigbeeDBase, newUuidString, &sChannelUuid, (double) value, createTime,
                                   createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG,
                                  "Не удалось сохранить измерение", MTM_ZB_CHANNEL_COORD_DIGI1_TITLE);
            }
        }
    }
}

void makeLightStatus(DBase *mtmZigbeeDBase, uint8_t *address, const uint8_t *packetBuffer) {
    uint8_t deviceUuid[37];
//    uint8_t sChannelUuid[37];
    std::string sChannelUuid;
    uuid_t newUuid;
    uint8_t newUuidString[37] = {0};
//    uint8_t measureUuid[37] = {0};
    std::string measureUuid;
    time_t createTime = time(nullptr);
    int8_t value;

    memset(deviceUuid, 0, 37);
    if (!findDevice(mtmZigbeeDBase, address, deviceUuid)) {
        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Неудалось найти устройство с адресом %s", address);
        return;
    }

    // найти канал по устройству sensor_channel и regIdx (Температура светильника)
    sChannelUuid = findSChannel(mtmZigbeeDBase, deviceUuid, MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_IDX, CHANNEL_T);
    if (sChannelUuid.empty()) {
        // если нет, создать
        uuid_generate(newUuid);
        uuid_unparse_upper(newUuid, (char *) newUuidString);
        if (!createSChannel(mtmZigbeeDBase, newUuidString, MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_TITLE,
                            MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_IDX,
                            deviceUuid, CHANNEL_T, createTime)) {
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Неудалось канал измерение ",
                              MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_TITLE);
        }
    }

    value = packetBuffer[34];
    if (!sChannelUuid.empty()) {
        measureUuid = findMeasure(mtmZigbeeDBase, &sChannelUuid, MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_IDX);
        if (!measureUuid.empty()) {
            if (!updateMeasureValue(mtmZigbeeDBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Не удалось обновить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (!storeMeasureValue(mtmZigbeeDBase, newUuidString, &sChannelUuid, (double) value, createTime,
                                   createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Не удалось сохранить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_TITLE);
            }
        }
    }

    // найти канал по устройству sensor_channel и regIdx (Ток светильника)
    sChannelUuid = findSChannel(mtmZigbeeDBase, deviceUuid, MTM_ZB_CHANNEL_LIGHT_CURRENT_IDX, CHANNEL_I);
    if (sChannelUuid.empty()) {
        // если нет, создать
        uuid_generate(newUuid);
        uuid_unparse_upper(newUuid, (char *) newUuidString);
        if (!createSChannel(mtmZigbeeDBase, newUuidString, MTM_ZB_CHANNEL_LIGHT_CURRENT_TITLE,
                            MTM_ZB_CHANNEL_LIGHT_CURRENT_IDX,
                            deviceUuid, CHANNEL_I, createTime)) {
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Неудалось канал измерение ",
                              MTM_ZB_CHANNEL_LIGHT_CURRENT_TITLE);
        }
    }

    value = packetBuffer[33];
    if (!sChannelUuid.empty()) {
        measureUuid = findMeasure(mtmZigbeeDBase, &sChannelUuid, MTM_ZB_CHANNEL_LIGHT_CURRENT_IDX);
        if (!measureUuid.empty()) {
            if (!updateMeasureValue(mtmZigbeeDBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Не удалось обновить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_CURRENT_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (!storeMeasureValue(mtmZigbeeDBase, newUuidString, &sChannelUuid, (double) value, createTime,
                                   createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Не удалось сохранить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_CURRENT_TITLE);
            }
        }
    }

    // найти канал по устройству sensor_channel и regIdx (Состояние светильника)
    sChannelUuid = findSChannel(mtmZigbeeDBase, deviceUuid, MTM_ZB_CHANNEL_LIGHT_STATUS_IDX, CHANNEL_STATUS);
    if (sChannelUuid.empty()) {
        // если нет, создать
        uuid_generate(newUuid);
        uuid_unparse_upper(newUuid, (char *) newUuidString);
        if (!createSChannel(mtmZigbeeDBase, newUuidString, MTM_ZB_CHANNEL_LIGHT_STATUS_TITLE,
                            MTM_ZB_CHANNEL_LIGHT_STATUS_IDX,
                            deviceUuid, CHANNEL_STATUS, createTime)) {
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Неудалось канал измерение ",
                              MTM_ZB_CHANNEL_LIGHT_STATUS_TITLE);
        }
    }

    uint16_t alerts = *(uint16_t *) &packetBuffer[31];
    value = alerts & 0x0001u;
    if (!sChannelUuid.empty()) {
        measureUuid = findMeasure(mtmZigbeeDBase, &sChannelUuid, MTM_ZB_CHANNEL_LIGHT_STATUS_IDX);
        if (!measureUuid.empty()) {
            if (!updateMeasureValue(mtmZigbeeDBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Не удалось обновить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_STATUS_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (!storeMeasureValue(mtmZigbeeDBase, newUuidString, &sChannelUuid, (double) value, createTime,
                                   createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Не удалось сохранить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_STATUS_TITLE);
            }
        }
    }
}
