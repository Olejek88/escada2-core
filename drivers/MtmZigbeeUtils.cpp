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

extern Kernel *kernel;
extern uint8_t TAG[];
extern bool isSunInit;
extern bool isSunSet, isTwilightEnd, isTwilightStart, isSunRise;
extern int coordinatorFd;

void log_buffer_hex(uint8_t *buffer, size_t buffer_size) {
    uint8_t message[1024];
    for (int i = 0; i < buffer_size; i++) {
        sprintf((char *) &message[i * 6], "0x%02x, ", buffer[i]);
    }

    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s\n", TAG, message);
}

bool findDevice(DBase *dBase, uint8_t *addr, uint8_t *uuid) {
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
        if (nRows == 1) {
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
        if (nRows == 1) {
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

bool updateMeasureValue(DBase *dBase, uint8_t *uuid, double value, time_t changedTime) {

    MYSQL_RES *res;
    char query[1024];

    sprintf(query,
            "UPDATE data SET value=%f, date=FROM_UNIXTIME(%ld), changedAt=FROM_UNIXTIME(%ld) WHERE uuid = '%s'",
            value, changedTime, changedTime, uuid);
#ifdef DEBUG
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s\n", TAG, query);
#endif

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

    sprintf(query,
            "INSERT INTO data (uuid, sensorChannelUuid, value, date, createdAt) value('%s', '%s', %f, FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld))",
            uuid, channelUuid->data(), value, createTime, changedTime);
#ifdef DEBUG
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s\n", TAG, query);
#endif

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
    sprintf((char *) query,
            "INSERT INTO sensor_channel (uuid, title, register, deviceUuid, measureTypeUuid, createdAt) value('%s', '%s', '%d', '%s', '%s', FROM_UNIXTIME(%ld))",
            uuid, channelTitle, sensorIndex, deviceUuid, channelTypeUuid, createTime);
#ifdef DEBUG
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s\n", TAG, query);
#endif

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
        if (nRows == 1) {
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
    std::string sChannelUuid;
    uuid_t newUuid;
    uint8_t newUuidString[37];
    std::string measureUuid;
    time_t createTime = time(nullptr);
    int threshold;
    Json::Reader reader;
    Json::Value obj;

    memset(deviceUuid, 0, 37);
    if (!findDevice(dBase, address, deviceUuid)) {
        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Неудалось найти устройство с адресом %s", address);
        return;
    }

    // найти канал по устройству sensor_channel и regIdx
    sChannelUuid = findSChannel(dBase, deviceUuid, MTM_ZB_CHANNEL_COORD_IN1_IDX, CHANNEL_IN1);
    if (sChannelUuid.empty()) {
        // если нет, создать
        uuid_generate(newUuid);
        uuid_unparse_upper(newUuid, (char *) newUuidString);
        if (!createSChannel(dBase, newUuidString, MTM_ZB_CHANNEL_COORD_IN1_TITLE,
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
        std::string config = getSChannelConfig(dBase, &sChannelUuid);
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
        measureUuid = findMeasure(dBase, &sChannelUuid, MTM_ZB_CHANNEL_COORD_IN1_IDX);
        if (!measureUuid.empty()) {
            if (!updateMeasureValue(dBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG,
                                  "Не удалось обновить измерение", MTM_ZB_CHANNEL_COORD_IN1_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            memset(newUuidString, 0, 37);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (!storeMeasureValue(dBase, newUuidString, &sChannelUuid, (double) value, createTime,
                                   createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG,
                                  "Не удалось сохранить измерение", MTM_ZB_CHANNEL_COORD_IN1_TITLE);
            }
        }
    }

    // найти канал по устройству sensor_channel и regIdx
    sChannelUuid = findSChannel(dBase, deviceUuid, MTM_ZB_CHANNEL_COORD_IN2_IDX, CHANNEL_IN2);
    if (sChannelUuid.empty()) {
        // если нет, создать
        uuid_generate(newUuid);
        uuid_unparse_upper(newUuid, (char *) newUuidString);
        if (!createSChannel(dBase, newUuidString, MTM_ZB_CHANNEL_COORD_IN2_TITLE,
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
        std::string config = getSChannelConfig(dBase, &sChannelUuid);
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
        measureUuid = findMeasure(dBase, &sChannelUuid, MTM_ZB_CHANNEL_COORD_IN2_IDX);
        if (!measureUuid.empty()) {
            if (!updateMeasureValue(dBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG,
                                  "Не удалось обновить измерение", MTM_ZB_CHANNEL_COORD_IN2_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (!storeMeasureValue(dBase, (uint8_t *) measureUuid.data(), &sChannelUuid, (double) value,
                                   createTime, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG,
                                  "Не удалось сохранить измерение", MTM_ZB_CHANNEL_COORD_IN2_TITLE);
            }
        }
    }

    // найти канал по устройству sensor_channel и regIdx (цифровой пин управления контактором)
    sChannelUuid = findSChannel(dBase, deviceUuid, MTM_ZB_CHANNEL_COORD_DIGI1_IDX, CHANNEL_DIGI1);
    if (sChannelUuid.empty()) {
        // если нет, создать
        uuid_generate(newUuid);
        uuid_unparse_upper(newUuid, (char *) newUuidString);
        if (!createSChannel(dBase, newUuidString, MTM_ZB_CHANNEL_COORD_DIGI1_TITLE,
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
        measureUuid = findMeasure(dBase, &sChannelUuid, MTM_ZB_CHANNEL_COORD_DIGI1_IDX);
        if (!measureUuid.empty()) {
            if (!updateMeasureValue(dBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG,
                                  "Не удалось обновить измерение", MTM_ZB_CHANNEL_COORD_DIGI1_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (!storeMeasureValue(dBase, newUuidString, &sChannelUuid, (double) value, createTime,
                                   createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG,
                                  "Не удалось сохранить измерение", MTM_ZB_CHANNEL_COORD_DIGI1_TITLE);
            }
        }
    }
}

void makeLightStatus(DBase *dBase, uint8_t *address, const uint8_t *packetBuffer) {
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
    if (!findDevice(dBase, address, deviceUuid)) {
        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Неудалось найти устройство с адресом %s", address);
        return;
    }

    // найти канал по устройству sensor_channel и regIdx (Температура светильника)
    sChannelUuid = findSChannel(dBase, deviceUuid, MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_IDX, CHANNEL_T);
    if (sChannelUuid.empty()) {
        // если нет, создать
        uuid_generate(newUuid);
        uuid_unparse_upper(newUuid, (char *) newUuidString);
        if (!createSChannel(dBase, newUuidString, MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_TITLE,
                            MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_IDX,
                            deviceUuid, CHANNEL_T, createTime)) {
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Неудалось канал измерение ",
                              MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_TITLE);
        }
    }

    value = packetBuffer[34];
    if (!sChannelUuid.empty()) {
        measureUuid = findMeasure(dBase, &sChannelUuid, MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_IDX);
        if (!measureUuid.empty()) {
            if (!updateMeasureValue(dBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Не удалось обновить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (!storeMeasureValue(dBase, newUuidString, &sChannelUuid, (double) value, createTime,
                                   createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Не удалось сохранить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_TITLE);
            }
        }
    }

    // найти канал по устройству sensor_channel и regIdx (Ток светильника)
    sChannelUuid = findSChannel(dBase, deviceUuid, MTM_ZB_CHANNEL_LIGHT_CURRENT_IDX, CHANNEL_I);
    if (sChannelUuid.empty()) {
        // если нет, создать
        uuid_generate(newUuid);
        uuid_unparse_upper(newUuid, (char *) newUuidString);
        if (!createSChannel(dBase, newUuidString, MTM_ZB_CHANNEL_LIGHT_CURRENT_TITLE,
                            MTM_ZB_CHANNEL_LIGHT_CURRENT_IDX,
                            deviceUuid, CHANNEL_I, createTime)) {
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Неудалось канал измерение ",
                              MTM_ZB_CHANNEL_LIGHT_CURRENT_TITLE);
        }
    }

    value = packetBuffer[33];
    if (!sChannelUuid.empty()) {
        measureUuid = findMeasure(dBase, &sChannelUuid, MTM_ZB_CHANNEL_LIGHT_CURRENT_IDX);
        if (!measureUuid.empty()) {
            if (!updateMeasureValue(dBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Не удалось обновить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_CURRENT_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (!storeMeasureValue(dBase, newUuidString, &sChannelUuid, (double) value, createTime,
                                   createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Не удалось сохранить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_CURRENT_TITLE);
            }
        }
    }

    // найти канал по устройству sensor_channel и regIdx (Состояние светильника)
    sChannelUuid = findSChannel(dBase, deviceUuid, MTM_ZB_CHANNEL_LIGHT_STATUS_IDX, CHANNEL_STATUS);
    if (sChannelUuid.empty()) {
        // если нет, создать
        uuid_generate(newUuid);
        uuid_unparse_upper(newUuid, (char *) newUuidString);
        if (!createSChannel(dBase, newUuidString, MTM_ZB_CHANNEL_LIGHT_STATUS_TITLE,
                            MTM_ZB_CHANNEL_LIGHT_STATUS_IDX,
                            deviceUuid, CHANNEL_STATUS, createTime)) {
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Неудалось канал измерение ",
                              MTM_ZB_CHANNEL_LIGHT_STATUS_TITLE);
        }
    }

    uint16_t alerts = *(uint16_t *) &packetBuffer[31];
    value = alerts & 0x0001u;
    if (!sChannelUuid.empty()) {
        measureUuid = findMeasure(dBase, &sChannelUuid, MTM_ZB_CHANNEL_LIGHT_STATUS_IDX);
        if (!measureUuid.empty()) {
            if (!updateMeasureValue(dBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Не удалось обновить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_STATUS_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (!storeMeasureValue(dBase, newUuidString, &sChannelUuid, (double) value, createTime,
                                   createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Не удалось сохранить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_STATUS_TITLE);
            }
        }
    }
}

void checkAstroEvents(time_t currentTime, double lon, double lat) {
    struct tm *ctm = localtime(&currentTime);
    double rise, set;
    double twilightStart, twilightEnd;
    int rs;
    int civ;
    uint64_t checkTime;
    uint64_t sunRiseTime;
    uint64_t sunSetTime;
    uint64_t twilightStartTime;
    uint64_t twilightEndTime;
    mtm_cmd_action action = {0};

    rs = sun_rise_set(ctm->tm_year + 1900, ctm->tm_mon + 1, ctm->tm_mday, lon, lat, &rise, &set);
    bool isTimeAboveSunSet;
    bool isTimeLessSunSet;
    bool isTimeAboveSunRise;
    bool isTimeLessSunRise;
    civ = civil_twilight(ctm->tm_year + 1900, ctm->tm_mon + 1, ctm->tm_mday, lon, lat, &twilightStart,
                         &twilightEnd);
    bool isTimeAboveTwilightStart;
    bool isTimeLessTwilightStart;
    bool isTimeAboveTwilightEnd;
    bool isTimeLessTwilightEnd;

    if (rs == 0 && civ == 0) {
        checkTime = ctm->tm_hour * 3600 + ctm->tm_min * 60 + ctm->tm_sec;
        sunRiseTime = (uint64_t) (rise * 3600 + ctm->tm_gmtoff);
        sunSetTime = (uint64_t) (set * 3600 + ctm->tm_gmtoff);

        twilightStartTime = (uint64_t) (twilightStart * 3600 + ctm->tm_gmtoff);
        twilightEndTime = (uint64_t) (twilightEnd * 3600 + ctm->tm_gmtoff);

        action.header.type = MTM_CMD_TYPE_ACTION;
        action.header.protoVersion = MTM_VERSION_0;
        action.device = MTM_DEVICE_LIGHT;

        isTimeAboveSunSet = checkTime > sunSetTime;
        isTimeLessSunSet = checkTime < sunSetTime;
        isTimeAboveSunRise = checkTime > sunRiseTime;
        isTimeLessSunRise = checkTime < sunRiseTime;

        isTimeAboveTwilightStart = checkTime > twilightStartTime;
        isTimeLessTwilightStart = checkTime < twilightStartTime;
        isTimeAboveTwilightEnd = checkTime > twilightEndTime;
        isTimeLessTwilightEnd = checkTime < twilightEndTime;

        if ((isTimeAboveSunSet && isTimeLessTwilightEnd) && (!isSunSet || !isSunInit)) {
            isSunInit = true;
            isSunSet = true;
            isTwilightEnd = false;
            isTwilightStart = false;
            isSunRise = false;
            // включаем контактор, зажигаем светильники, отправляем команду "закат"
            switchContactor(true, MBEE_API_DIGITAL_LINE7);
            // даём задержку для того чтоб стартанули модули в светильниках
            // т.к. неизвестно, питаются они через контактор или всё время под напряжением
            sleep(5);
            switchAllLight(100);
            // передаём команду "астро событие" "закат"
            action.data = (0x02 << 8 | 0x01); // NOLINT(hicpp-signed-bitwise)
            ssize_t rc = send_mtm_cmd(coordinatorFd, 0xFFFF, &action);
#ifdef DEBUG
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld\n", TAG, rc);
            kernel->log.ulogw(LOG_LEVEL_INFO, "--> закат", TAG);
#endif
        } else if ((isTimeAboveTwilightEnd || isTimeLessTwilightStart) && (!isTwilightEnd || !isSunInit)) {
            isSunInit = true;
            isSunSet = false;
            isTwilightEnd = true;
            isTwilightStart = false;
            isSunRise = false;

            // передаём команду "астро событие" "конец сумерек"
            action.data = (0x01 << 8 | 0x00); // NOLINT(hicpp-signed-bitwise)
            ssize_t rc = send_mtm_cmd(coordinatorFd, 0xFFFF, &action);
#ifdef DEBUG
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld\n", TAG, rc);
            kernel->log.ulogw(LOG_LEVEL_INFO, "--> конец сумерек", TAG);
#endif
        } else if ((isTimeAboveTwilightStart && isTimeLessSunRise) && (!isTwilightStart || !isSunInit)) {
            isSunInit = true;
            isSunSet = false;
            isTwilightEnd = false;
            isTwilightStart = true;
            isSunRise = false;
            // передаём команду "астро событие" "начало сумерек"
            action.data = (0x03 << 8 | 0x00); // NOLINT(hicpp-signed-bitwise)
            ssize_t rc = send_mtm_cmd(coordinatorFd, 0xFFFF, &action);
#ifdef DEBUG
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld\n", TAG, rc);
            kernel->log.ulogw(LOG_LEVEL_INFO, "--> начало сумерек", TAG);
#endif
        } else if ((isTimeAboveSunRise && isTimeLessSunSet) && (isSunRise || !isSunInit)) {
            isSunInit = true;
            isSunSet = false;
            isTwilightEnd = false;
            isTwilightStart = false;
            isSunRise = true;
            // выключаем контактор, гасим светильники, отправляем команду "восход"
            switchContactor(false, MBEE_API_DIGITAL_LINE7);
            // на всякий случай, если светильники всегда под напряжением
            switchAllLight(0);
            // передаём команду "астро событие" "восход"
            action.data = (0x00 << 8 | 0x00); // NOLINT(hicpp-signed-bitwise)
            ssize_t rc = send_mtm_cmd(coordinatorFd, 0xFFFF, &action);
#ifdef DEBUG
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld\n", TAG, rc);
            kernel->log.ulogw(LOG_LEVEL_INFO, "--> восход", TAG);
#endif
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

void checkLightProgram(DBase *dBase, time_t currentTime, double lon, double lat) {
//    struct tm ttm = {0};
//    ttm.tm_year = 119;
//    ttm.tm_mon = 7;
//    ttm.tm_mday = 28;
//    ttm.tm_hour = 00;
//    ttm.tm_min = 0;
//    ttm.tm_sec = 21133;
//    ttm.tm_gmtoff = 5 * 3600;
//    currentTime = std::mktime(&ttm);

    MYSQL_RES *res;
    MYSQL_ROW row;
    std::string currentProgram;
    struct tm *ctm = localtime(&currentTime);
    uint64_t checkTime = ctm->tm_hour * 3600 + ctm->tm_min * 60 + ctm->tm_sec;
    printf("checkTime: %ld\n", checkTime);
    uint64_t time1raw = 0, time2raw = 0, time3raw = 0, time4raw = 0;
    uint64_t time1loc = 0, time2loc = 0, time3loc = 0, time4loc = 0;
    double rise, set;
    double rs = sun_rise_set(ctm->tm_year + 1900, ctm->tm_mon + 1, ctm->tm_mday, lon, lat, &rise, &set);
    auto sunRiseTime = (uint64_t) (rise * 3600 + ctm->tm_gmtoff);
    auto sunSetTime = (uint64_t) (set * 3600 + ctm->tm_gmtoff);
    printf("sunRiseTime: %ld, sunSetTime: %ld\n", sunRiseTime, sunSetTime);
    double civlen = day_length(ctm->tm_year + 1900, ctm->tm_mon + 1, ctm->tm_mday, lon, lat);
    auto dayLen = (uint64_t) (civlen * 3600);
    uint64_t nightLen = 86400 - dayLen;
    printf("dayLen: %ld, nightLen: %ld, sum: %ld\n", dayLen, nightLen, dayLen + nightLen);
    std::string query = std::string("SELECT device.address AS address, device_program.title AS title, "
                                    "time1, value1, time2, value2, time3, value3, time4, value4, time5, value5 "
                                    "FROM device "
                                    "LEFT JOIN device_config ON device.uuid = device_config.deviceUuid "
                                    "LEFT JOIN device_program ON device_config.value = device_program.title "
                                    "WHERE device.deviceTypeUuid = 'CFD3C7CC-170C-4764-9A8D-10047C8B8B1D' "
                                    "AND device_config.parameter = 'Программа' "
                                    "ORDER BY device_program.title");

    res = dBase->sqlexec(query.data());
    if (res) {
        dBase->makeFieldsList(res);
        while ((row = mysql_fetch_row(res)) != nullptr) {
            int percent;
            if (currentProgram != row[dBase->getFieldIndex("title")]) {
                currentProgram = row[dBase->getFieldIndex("title")];
                // нужно пересчитать параметры программы
                percent = std::stoi(row[dBase->getFieldIndex("time1")]);
                time1raw = sunSetTime + (uint64_t) (nightLen * (1 / (100.0 / percent)));
                percent = std::stoi(row[dBase->getFieldIndex("time2")]);
                time2raw = time1raw + (uint64_t) (nightLen * (1.0 / (100.0 / percent)));
                percent = std::stoi(row[dBase->getFieldIndex("time3")]);
                time3raw = time2raw + (uint64_t) (nightLen * (1.0 / (100.0 / percent)));
                percent = std::stoi(row[dBase->getFieldIndex("time4")]);
                time4raw = time3raw + (uint64_t) (nightLen * (1.0 / (100.0 / percent)));
                printf("time1raw: %ld, time2raw: %ld, time3raw: %ld, time4raw: %ld\n", time1raw, time2raw, time3raw,
                       time4raw);

                time1loc = time1raw > 86400 ? time1raw - 86400 : time1raw;
                time2loc = time2raw > 86400 ? time2raw - 86400 : time2raw;
                time3loc = time3raw > 86400 ? time3raw - 86400 : time3raw;
                time4loc = time4raw > 86400 ? time4raw - 86400 : time4raw;
                printf("time1loc: %ld, time2loc: %ld, time3loc: %ld, time4loc: %ld\n", time1loc, time2loc, time3loc,
                       time4loc);
            }

            bool processed = false;
            // интервал от заката до длительности заданной time1
            if (time1raw > time1loc) {
                // переход через полночь
                if ((checkTime > sunSetTime && checkTime < 86400) || (checkTime > 0 && checkTime < time1loc)) {
                    processed = true;
                    printf("period 1 overnight\n");
                }
            } else {
                if (checkTime > sunSetTime && checkTime < time1raw) {
                    processed = true;
                    printf("period 1\n");
                }
            }

            // интервал от time1 до длительности заданной time2
            if (!processed) {
                if (time1loc > time2loc) {
                    // переход через полночь
                    if ((checkTime > time1raw && checkTime < 86400) || (checkTime > 0 && checkTime < time2loc)) {
                        processed = true;
                        printf("period 1 overnight\n");
                    }
                } else {
                    if (checkTime > time1loc && checkTime < time2loc) {
                        processed = true;
                        printf("period 2\n");
                    }
                }
            }

            // интервал от time2 до длительности заданной time3
            if (!processed) {
                if (time2loc > time3loc) {
                    // переход через полночь
                    if ((checkTime > time2raw && checkTime < 86400) || (checkTime > 0 && checkTime < time3loc)) {
                        processed = true;
                        printf("period 3 overnight\n");
                    }
                } else {
                    if (checkTime > time2loc && checkTime < time3loc) {
                        processed = true;
                        printf("period 3\n");
                    }
                }
            }

            // интервал от time3 до длительности заданной time4
            if (!processed) {
                if (time3loc > time4loc) {
                    // переход через полночь
                    if ((checkTime > time3raw && checkTime < 86400) || (checkTime > 0 && checkTime < time4loc)) {
                        processed = true;
                        printf("period 4 overnight\n");
                    }
                } else {
                    if (checkTime > time3loc && checkTime < time4loc) {
                        processed = true;
                        printf("period 4\n");
                    }
                }
            }

            // интервал от time4 до восхода
            if (!processed) {
                if (checkTime > time4loc && checkTime < sunRiseTime) {
                    processed = true;
                    printf("period 5\n");
                }
            }

            // день
            if (!processed) {
                if (checkTime > sunRiseTime && checkTime < sunSetTime) {
                    processed = true;
                    printf("день\n");
                }
            }

            // длина суммы периодов меньше длины ночи
            if (!processed) {
                printf("нет событий\n");
            }

//            if (checkTime > sunSetTime && checkTime < time1raw) {
//                // интервал от заката до окончания сумерек
//                printf("time1\n");
//            } else if (checkTime > time1raw && checkTime < time2raw) {
//                // интервал от окончания сумерек до полуночи
//                printf("time2\n");
//            } else if (checkTime > time2raw && checkTime < time3raw) {
//                // интервал от полуночи до глубокой ночи
//                printf("time3\n");
//            } else if (checkTime > time3raw && checkTime < time4raw) {
//                // интервал от глубой ночи до начала сумерек
//                printf("time4\n");
//            } else if (checkTime > time4raw && checkTime < sunRiseTime) {
//                // интервал от начала сумерек до восхода
//                printf("time5\n");
//            } else if (checkTime > sunRiseTime && checkTime < sunSetTime) {
//                printf("день\n");
//            } else {
//                // условия когда ни каких событий нет. это возможно когда сумма в процентах длительностей интервалов
//                // меньше 100 процентов, либо сейчас день.
//                printf("нет событий\n");
//            }

        }

        mysql_free_result(res);
    }

}