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
#include "LightFlags.h"
#include "main.h"

extern Kernel *kernel;
extern uint8_t TAG[];
extern bool isSunInit;
extern bool isSunSet, isTwilightEnd, isTwilightStart, isSunRise;
extern int coordinatorFd;
extern std::map<std::string, LightFlags> lightFlags;
extern std::string coordinatorUuid;

void log_buffer_hex(uint8_t *buffer, size_t buffer_size) {
    uint8_t message[1024];
    for (int i = 0; i < buffer_size; i++) {
        sprintf((char *) &message[i * 2], "%02x", buffer[i]);
    }

    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] MTM packet: %s", TAG, message);
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

bool updateMeasureValue(DBase *dBase, uint8_t *uuid, double value, time_t changedTime) {

    MYSQL_RES *res;
    char query[1024];

    sprintf(query,
            "UPDATE data SET value=%f, date=FROM_UNIXTIME(%ld), changedAt=FROM_UNIXTIME(%ld) WHERE uuid = '%s'",
            value, changedTime, changedTime, uuid);
#ifdef DEBUG
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s", TAG, query);
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
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s", TAG, query);
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
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s", TAG, query);
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

    memset(deviceUuid, 0, 37);
    if (!findDevice(dBase, address, deviceUuid)) {
        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, "Неудалось найти устройство с адресом", address);
        // TODO: отправить сообщение в журнал
        return;
    }

    // найти канал по устройству sensor_channel и regIdx
    std::string in1ChannelUuid = findSChannel(dBase, deviceUuid, MTM_ZB_CHANNEL_COORD_IN1_IDX, CHANNEL_IN1);
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

        value = value > threshold;
        measureUuid = findMeasure(dBase, &in1ChannelUuid, MTM_ZB_CHANNEL_COORD_IN1_IDX);
        if (!measureUuid.empty()) {
            if (updateMeasureValue(dBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG,
                                  "Не удалось обновить измерение", MTM_ZB_CHANNEL_COORD_IN1_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            memset(newUuidString, 0, 37);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (storeMeasureValue(dBase, newUuidString, &in1ChannelUuid, (double) value, createTime, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG,
                                  "Не удалось сохранить измерение", MTM_ZB_CHANNEL_COORD_IN1_TITLE);
            }
        }
    }


    // найти канал по устройству sensor_channel и regIdx
    std::string in2ChannelUuid = findSChannel(dBase, deviceUuid, MTM_ZB_CHANNEL_COORD_IN2_IDX, CHANNEL_IN2);
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

        value = value > threshold;
        measureUuid = findMeasure(dBase, &in2ChannelUuid, MTM_ZB_CHANNEL_COORD_IN2_IDX);
        if (!measureUuid.empty()) {
            if (updateMeasureValue(dBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG,
                                  "Не удалось обновить измерение", MTM_ZB_CHANNEL_COORD_IN2_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (storeMeasureValue(dBase, newUuidString, &in2ChannelUuid, (double) value,
                                  createTime, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG,
                                  "Не удалось сохранить измерение", MTM_ZB_CHANNEL_COORD_IN2_TITLE);
            }
        }
    }

    // найти канал по устройству sensor_channel и regIdx (цифровой пин управления контактором)
    std::string digi1ChannelUuid = findSChannel(dBase, deviceUuid, MTM_ZB_CHANNEL_COORD_DIGI1_IDX, CHANNEL_DIGI1);
    if (!digi1ChannelUuid.empty()) {
        uint16_t value = *(uint16_t *) (&packetBuffer[32]);
        value &= 0x0040u;
        value = value >> 6; // NOLINT(hicpp-signed-bitwise)
        measureUuid = findMeasure(dBase, &digi1ChannelUuid, MTM_ZB_CHANNEL_COORD_DIGI1_IDX);
        if (!measureUuid.empty()) {
            if (updateMeasureValue(dBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG,
                                  "Не удалось обновить измерение", MTM_ZB_CHANNEL_COORD_DIGI1_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (storeMeasureValue(dBase, newUuidString, &digi1ChannelUuid, (double) value, createTime, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG,
                                  "Не удалось сохранить измерение", MTM_ZB_CHANNEL_COORD_DIGI1_TITLE);
            }
        }
    }
}

void makeLightStatus(DBase *dBase, uint8_t *address, const uint8_t *packetBuffer) {
    uint8_t deviceUuid[37];
    uuid_t newUuid;
    uint8_t newUuidString[37] = {0};
    std::string measureUuid;
    time_t createTime = time(nullptr);
    int8_t value;

    memset(deviceUuid, 0, 37);
    if (!findDevice(dBase, address, deviceUuid)) {
        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, "Неудалось найти устройство с адресом", address);
        // TODO: отправить сообщение в журнал
        return;
    }

    // найти канал по устройству sensor_channel и regIdx (Температура светильника)
    std::string tempChannelUuid = findSChannel(dBase, deviceUuid, MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_IDX, CHANNEL_T);
    if (!tempChannelUuid.empty()) {
        value = packetBuffer[34];
        measureUuid = findMeasure(dBase, &tempChannelUuid, MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_IDX);
        if (!measureUuid.empty()) {
            if (updateMeasureValue(dBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, "Не удалось обновить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (storeMeasureValue(dBase, newUuidString, &tempChannelUuid, (double) value, createTime, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, "Не удалось сохранить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_TITLE);
            }
        }
    }


    // найти канал по устройству sensor_channel и regIdx (Потребляемая мощность светильника)
    std::string powerChannelUuid = findSChannel(dBase, deviceUuid, MTM_ZB_CHANNEL_LIGHT_POWER_IDX, CHANNEL_W);
    if (!powerChannelUuid.empty()) {
        value = packetBuffer[33];
        measureUuid = findMeasure(dBase, &powerChannelUuid, MTM_ZB_CHANNEL_LIGHT_POWER_IDX);
        if (!measureUuid.empty()) {
            if (updateMeasureValue(dBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, "Не удалось обновить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_POWER_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (storeMeasureValue(dBase, newUuidString, &powerChannelUuid, (double) value, createTime, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, "Не удалось сохранить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_POWER_TITLE);
            }
        }
    }

    // найти канал по устройству sensor_channel и regIdx (Состояние светильника)
    std::string statusChannelUuid = findSChannel(dBase, deviceUuid, MTM_ZB_CHANNEL_LIGHT_STATUS_IDX, CHANNEL_STATUS);
    if (!statusChannelUuid.empty()) {
        uint16_t alerts = *(uint16_t *) &packetBuffer[31];
        value = alerts & 0x0001u;
        measureUuid = findMeasure(dBase, &statusChannelUuid, MTM_ZB_CHANNEL_LIGHT_STATUS_IDX);
        if (!measureUuid.empty()) {
            if (updateMeasureValue(dBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, "Не удалось обновить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_STATUS_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (storeMeasureValue(dBase, newUuidString, &statusChannelUuid, (double) value, createTime, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, "Не удалось сохранить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_STATUS_TITLE);
            }
        }
    }
}

void makeLightRssiHopsStatus(DBase *dBase, uint8_t *address, const uint8_t *packetBuffer) {
    uint8_t deviceUuid[37];
    uuid_t newUuid;
    uint8_t newUuidString[37] = {0};
    std::string measureUuid;
    time_t createTime = time(nullptr);
    int8_t value;

    memset(deviceUuid, 0, 37);
    if (!findDevice(dBase, address, deviceUuid)) {
        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, "Неудалось найти устройство с адресом", address);
        // TODO: отправить сообщение в журнал
        return;
    }

    // найти канал по устройству sensor_channel и regIdx (RSSI)
    std::string rssiChannelUuid = findSChannel(dBase, deviceUuid, MTM_ZB_CHANNEL_LIGHT_RSSI_IDX, CHANNEL_RSSI);
    if (!rssiChannelUuid.empty()) {
        // уровень сигнала идёт в младшем байте статуса второго устройства
        // 0-30 байт служебная информация zb
        // 31-32 alert
        // 33,34 power,temp
        // 35,36 rssi,hop count
        value = packetBuffer[35];
        measureUuid = findMeasure(dBase, &rssiChannelUuid, MTM_ZB_CHANNEL_LIGHT_RSSI_IDX);
        if (!measureUuid.empty()) {
            if (updateMeasureValue(dBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, "Не удалось обновить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_RSSI_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (storeMeasureValue(dBase, newUuidString, &rssiChannelUuid, (double) value, createTime, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, "Не удалось сохранить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_RSSI_TITLE);
            }
        }
    }

    // найти канал по устройству sensor_channel и regIdx (Hop count)
    std::string hopsChannelUuid = findSChannel(dBase, deviceUuid, MTM_ZB_CHANNEL_LIGHT_HOP_COUNT_IDX,
                                               CHANNEL_HOP_COUNT);
    if (!hopsChannelUuid.empty()) {
        value = packetBuffer[36];
        measureUuid = findMeasure(dBase, &hopsChannelUuid, MTM_ZB_CHANNEL_LIGHT_HOP_COUNT_IDX);
        if (!measureUuid.empty()) {
            if (updateMeasureValue(dBase, (uint8_t *) measureUuid.data(), value, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, "Не удалось обновить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_HOP_COUNT_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (storeMeasureValue(dBase, newUuidString, &hopsChannelUuid, (double) value, createTime, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, "Не удалось сохранить измерение",
                                  MTM_ZB_CHANNEL_LIGHT_HOP_COUNT_TITLE);
            }
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

    memset(deviceUuid, 0, 37);
    if (!findDevice(dBase, address, deviceUuid)) {
        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, "Неудалось найти устройство с адресом", address);
        // TODO: отправить сообщение в журнал
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

void checkAstroEvents(time_t currentTime, double lon, double lat, DBase *dBase, int32_t threadId) {
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

        isTimeAboveSunSet = checkTime >= sunSetTime;
        isTimeLessSunSet = checkTime < sunSetTime;
        isTimeAboveSunRise = checkTime >= sunRiseTime;
        isTimeLessSunRise = checkTime < sunRiseTime;

        isTimeAboveTwilightStart = checkTime >= twilightStartTime;
        isTimeLessTwilightStart = checkTime < twilightStartTime;
        isTimeAboveTwilightEnd = checkTime >= twilightEndTime;
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
            ssize_t rc = switchAllLight(100);
            if (rc == -1) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
                // останавливаем поток с целью его последующего автоматического запуска и инициализации
                mtmZigbeeStopThread(dBase, threadId);
                AddDeviceRegister(*dBase, (char *) coordinatorUuid.data(),
                                  (char *) "Ошибка записи в порт координатора");
                return;
            }

            // передаём команду "астро событие" "закат"
            action.data = (0x02 << 8 | 0x01); // NOLINT(hicpp-signed-bitwise)
            rc = send_mtm_cmd(coordinatorFd, 0xFFFF, &action, kernel);
            if (rc == -1) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
                // останавливаем поток с целью его последующего автоматического запуска и инициализации
                mtmZigbeeStopThread(dBase, threadId);
                AddDeviceRegister(*dBase, (char *) coordinatorUuid.data(),
                                  (char *) "Ошибка записи в порт координатора");
                return;
            }
#ifdef DEBUG
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] закат", TAG);
#endif
        } else if ((isTimeAboveTwilightEnd || isTimeLessTwilightStart) && (!isTwilightEnd || !isSunInit)) {
            isSunInit = true;
            isSunSet = false;
            isTwilightEnd = true;
            isTwilightStart = false;
            isSunRise = false;

            // передаём команду "астро событие" "конец сумерек"
            action.data = (0x01 << 8 | 0x00); // NOLINT(hicpp-signed-bitwise)
            ssize_t rc = send_mtm_cmd(coordinatorFd, 0xFFFF, &action, kernel);
            if (rc == -1) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
                // останавливаем поток с целью его последующего автоматического запуска и инициализации
                mtmZigbeeStopThread(dBase, threadId);
                AddDeviceRegister(*dBase, (char *) coordinatorUuid.data(),
                                  (char *) "Ошибка записи в порт координатора");
                return;
            }
#ifdef DEBUG
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] конец сумерек", TAG);
#endif
        } else if ((isTimeAboveTwilightStart && isTimeLessSunRise) && (!isTwilightStart || !isSunInit)) {
            isSunInit = true;
            isSunSet = false;
            isTwilightEnd = false;
            isTwilightStart = true;
            isSunRise = false;
            // передаём команду "астро событие" "начало сумерек"
            action.data = (0x03 << 8 | 0x00); // NOLINT(hicpp-signed-bitwise)
            ssize_t rc = send_mtm_cmd(coordinatorFd, 0xFFFF, &action, kernel);
            if (rc == -1) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
                // останавливаем поток с целью его последующего автоматического запуска и инициализации
                mtmZigbeeStopThread(dBase, threadId);
                AddDeviceRegister(*dBase, (char *) coordinatorUuid.data(),
                                  (char *) "Ошибка записи в порт координатора");
                return;
            }
#ifdef DEBUG
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] начало сумерек", TAG);
#endif
        } else if ((isTimeAboveSunRise && isTimeLessSunSet) && (!isSunRise || !isSunInit)) {
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
            ssize_t rc = send_mtm_cmd(coordinatorFd, 0xFFFF, &action, kernel);
            if (rc == -1) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
                // останавливаем поток с целью его последующего автоматического запуска и инициализации
                mtmZigbeeStopThread(dBase, threadId);
                AddDeviceRegister(*dBase, (char *) coordinatorUuid.data(),
                                  (char *) "Ошибка записи в порт координатора");
                return;
            }
#ifdef DEBUG
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] восход", TAG);
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

void checkLightProgram(DBase *dBase, time_t currentTime, double lon, double lat, int32_t threadId) {
//    struct tm ttm = {0};
//    ttm.tm_year = 119;
//    ttm.tm_mon = 7;
//    ttm.tm_mday = 28;
//    ttm.tm_hour = 00;
//    ttm.tm_min = 0;
//    ttm.tm_sec = 21133;
//    ttm.tm_gmtoff = 5 * 3600;
//    currentTime = std::mktime(&ttm);

    bool isDay = false;
    MYSQL_RES *res;
    MYSQL_ROW row;
    std::string currentProgram;
    struct tm *ctm = localtime(&currentTime);
    uint64_t checkTime = ctm->tm_hour * 3600 + ctm->tm_min * 60 + ctm->tm_sec;
    uint64_t time1raw = 0, time2raw = 0;
    uint64_t time1loc = 0, time2loc = 0;
    double rise, set;
    sun_rise_set(ctm->tm_year + 1900, ctm->tm_mon + 1, ctm->tm_mday, lon, lat, &rise, &set);
    auto sunRiseTime = (uint64_t) (rise * 3600 + ctm->tm_gmtoff);
    auto sunSetTime = (uint64_t) (set * 3600 + ctm->tm_gmtoff);
    double twilightStart, twilightEnd;
    civil_twilight(ctm->tm_year + 1900, ctm->tm_mon + 1, ctm->tm_mday, lon, lat, &twilightStart, &twilightEnd);
    auto twilightStartTime = (uint64_t) (twilightStart * 3600 + ctm->tm_gmtoff);
    auto twilightEndTime = (uint64_t) (twilightEnd * 3600 + ctm->tm_gmtoff);
    uint64_t twilightEndTimeLoc = 86400 - twilightEndTime;
    uint64_t nightLen =
            86400 -
            ((sunSetTime - sunRiseTime) + (sunRiseTime - twilightStartTime) + (twilightEndTime - sunSetTime));
    auto dayLen = sunSetTime - sunRiseTime;
    uint64_t twilightLen = (sunRiseTime - twilightStartTime) + (twilightEndTime - sunSetTime);

    std::string query = std::string("SELECT device.address AS address, device_program.title AS title, "
                                    "value1, time2, value2, time3, value3, time4, value4, value5 "
                                    "FROM device "
                                    "LEFT JOIN device_config ON device.uuid = device_config.deviceUuid "
                                    "LEFT JOIN device_program ON device_config.value = device_program.title "
                                    "WHERE device.deviceTypeUuid = 'CFD3C7CC-170C-4764-9A8D-10047C8B8B1D' "
                                    "AND device_config.parameter = 'Программа' "
                                    "ORDER BY device_program.title");

#ifdef DEBUG
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] checkTime: %llu", TAG, checkTime);
    kernel->log.ulogw(LOG_LEVEL_INFO,
                      "[%s] twilightStartTime: %llu, sunRiseTime: %llu, sunSetTime: %llu, twilightEndTime: %llu",
                      TAG, twilightStartTime, sunRiseTime, sunSetTime, twilightEndTime);
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] dayLen: %llu, nightLen: %llu, twilightLen: %llu, sum: %llu",
                      TAG, dayLen, nightLen, twilightLen, dayLen + nightLen + twilightLen);
#endif

    res = dBase->sqlexec(query.data());
    if (res) {
        dBase->makeFieldsList(res);
        while ((row = mysql_fetch_row(res)) != nullptr) {
            int percent;
            if (currentProgram != row[dBase->getFieldIndex("title")]) {
                currentProgram = row[dBase->getFieldIndex("title")];
                // нужно пересчитать параметры программы
                percent = std::stoi(row[dBase->getFieldIndex("time2")]);
                time1raw = twilightEndTime + (uint64_t) (nightLen * (1.0 / (100.0 / percent)));
                percent = std::stoi(row[dBase->getFieldIndex("time3")]);
                time2raw = time1raw + (uint64_t) (nightLen * (1.0 / (100.0 / percent)));

                time1loc = time1raw > 86400 ? time1raw - 86400 : time1raw;
                time2loc = time2raw > 86400 ? time2raw - 86400 : time2raw;
#ifdef DEBUG
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] time1raw: %llu, time2raw: %llu", TAG, time1raw, time2raw);
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] time1loc: %llu, time2loc: %llu", TAG, time1loc, time2loc);
#endif
            }

            ssize_t rc;
            bool processed = false;
            std::string address = row[dBase->getFieldIndex("address")];
            // интервал от заката до конца сумерек
            if (!lightFlags[address].isPeriod1()) {
                if (twilightEndTime > 86400) {
                    if ((checkTime >= sunSetTime && checkTime < 86400) ||
                        (checkTime >= 0 && checkTime < twilightEndTimeLoc)) {
                        processed = true;
#ifdef DEBUG
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s period 1 overnight", TAG, address.data());
#endif
                    }
                } else {
                    if (checkTime >= sunSetTime && checkTime < twilightEndTime) {
                        processed = true;
#ifdef DEBUG
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s period 1", TAG, address.data());
#endif
                    }
                }

                if (processed) {
                    lightFlags[address].setPeriod1Active();
                    rc = sendLightLevel((char *) address.data(), row[dBase->getFieldIndex("value1")]);
                    if (rc == -1) {
                        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
                        // останавливаем поток с целью его последующего автоматического запуска и инициализации
                        mtmZigbeeStopThread(dBase, threadId);
                        AddDeviceRegister(*dBase, (char *) coordinatorUuid.data(),
                                          (char *) "Ошибка записи в порт координатора");
                        return;
                    }
#ifdef DEBUG
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] checkTime: %ld", TAG, checkTime);
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
#endif
                }
            }

            // интервал от конца сумерек до длительности заданной time1
            processed = false;
            if (!lightFlags[address].isPeriod2()) {
                if (time1raw > 86400) {
                    // переход через полночь
                    if ((checkTime >= twilightEndTime && checkTime < 86400) ||
                        (checkTime >= 0 && checkTime < time1loc)) {
                        processed = true;
#ifdef DEBUG
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s period 2 overnight", TAG, address.data());
#endif
                    }
                } else {
                    if (checkTime >= twilightEndTime && checkTime < time1loc) {
                        processed = true;
#ifdef DEBUG
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s period 2", TAG, address.data());
#endif
                    }
                }

                if (processed) {
                    lightFlags[address].setPeriod2Active();
                    rc = sendLightLevel((char *) address.data(), row[dBase->getFieldIndex("value2")]);
                    if (rc == -1) {
                        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
                        // останавливаем поток с целью его последующего автоматического запуска и инициализации
                        mtmZigbeeStopThread(dBase, threadId);
                        AddDeviceRegister(*dBase, (char *) coordinatorUuid.data(),
                                          (char *) "Ошибка записи в порт координатора");
                        return;
                    }
#ifdef DEBUG
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] checkTime: %ld", TAG, checkTime);
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
#endif
                }
            }

            // интервал от time1 до длительности заданной time2
            processed = false;
            if (!lightFlags[address].isPeriod3()) {
                if (time1loc > time2loc) {
                    // переход через полночь (time1 находится до полуночи, time2 после полуночи)
                    if ((checkTime >= time1raw && checkTime < 86400) || (checkTime >= 0 && checkTime < time2loc)) {
                        processed = true;
#ifdef DEBUG
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s period 3 overnight time2 after", TAG,
                                          address.data());
#endif
                    }
                } else {
                    if (checkTime >= time1loc && checkTime < time2loc) {
                        processed = true;
#ifdef DEBUG
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s period 3", TAG, address.data());
#endif
                    }
                }

                if (processed) {
                    lightFlags[address].setPeriod3Active();
                    rc = sendLightLevel((char *) address.data(), row[dBase->getFieldIndex("value3")]);
                    if (rc == -1) {
                        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
                        // останавливаем поток с целью его последующего автоматического запуска и инициализации
                        mtmZigbeeStopThread(dBase, threadId);
                        AddDeviceRegister(*dBase, (char *) coordinatorUuid.data(),
                                          (char *) "Ошибка записи в порт координатора");
                        return;
                    }
#ifdef DEBUG
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] checkTime: %ld", TAG, checkTime);
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
#endif
                }
            }

            // интервал от time2 до начала сумерек
            processed = false;
            if (!lightFlags[address].isPeriod4()) {
                if (time2raw > 86400) {
                    // переход через полночь (time2 находится после полуночи)
                    if (checkTime >= time2loc && checkTime < twilightStartTime) {
                        processed = true;
#ifdef DEBUG
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s period 4 overnight", TAG, address.data());
#endif
                    }
                } else {
                    if ((checkTime >= time2loc && checkTime < 86400) ||
                        (checkTime >= 0 && checkTime < twilightStartTime)) {
                        processed = true;
#ifdef DEBUG
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s period 4", TAG, address.data());
#endif
                    }
                }

                if (processed) {
                    lightFlags[address].setPeriod4Active();
                    rc = sendLightLevel((char *) address.data(), row[dBase->getFieldIndex("value4")]);
                    if (rc == -1) {
                        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
                        // останавливаем поток с целью его последующего автоматического запуска и инициализации
                        mtmZigbeeStopThread(dBase, threadId);
                        AddDeviceRegister(*dBase, (char *) coordinatorUuid.data(),
                                          (char *) "Ошибка записи в порт координатора");
                        return;
                    }
#ifdef DEBUG
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] checkTime: %ld", TAG, checkTime);
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
#endif
                }
            }

            // интервал от начала сумерек до восхода
            if (!lightFlags[address].isPeriod5()) {
                if (checkTime >= twilightStartTime && checkTime < sunRiseTime) {
                    lightFlags[address].setPeriod5Active();
                    rc = sendLightLevel((char *) address.data(), row[dBase->getFieldIndex("value5")]);
                    if (rc == -1) {
                        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
                        // останавливаем поток с целью его последующего автоматического запуска и инициализации
                        mtmZigbeeStopThread(dBase, threadId);
                        AddDeviceRegister(*dBase, (char *) coordinatorUuid.data(),
                                          (char *) "Ошибка записи в порт координатора");
                        return;
                    }
#ifdef DEBUG
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s period 5", TAG, address.data());
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] checkTime: %llu", TAG, checkTime);
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
#endif
                }
            }

            // день
            if (!lightFlags[address].isDay()) {
                if (checkTime >= sunRiseTime && checkTime < sunSetTime) {
                    isDay = true;
                    lightFlags[address].setDayActive();
#ifdef DEBUG
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s period day", TAG, address.data());
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] checkTime: %llu", TAG, checkTime);
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
#endif
                }
            }

            // TODO: пересмотреть алгоритм, для выявления подобного события
            // длина суммы периодов меньше длины ночи или равна 0
//            if (!processed) {
//                if (!lightFlags[addresses[i]].isNoEvents) {
//                    setNoEventsActive(&lightFlags[addresses[i]]);
//                    printf("[%s] no events by light program\n", TAG);
//                    printf("[%s] checkTime: %ld\n", TAG, checkTime);
//                }
//            }
        }

        if (isDay) {
            ssize_t rc = switchAllLight(0);
            if (rc == -1) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
                // останавливаем поток с целью его последующего автоматического запуска и инициализации
                mtmZigbeeStopThread(dBase, threadId);
                AddDeviceRegister(*dBase, (char *) coordinatorUuid.data(),
                                  (char *) "Ошибка записи в порт координатора");
                return;
            }
#ifdef DEBUG
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Switch all lights off by program", TAG);
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
#endif
        }

        mysql_free_result(res);
    }

}

ssize_t sendLightLevel(char *addrString, char *level) {
    mtm_cmd_action action = {0};
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
    int linkTimeOut = 60;
    char query[1024];
    MYSQL_RES *res;
    MYSQL_ROW row;
    int nRows;
    int contactorState = 0; // считаем что контактор всегда включен, даже если его нет

    // проверяем сотояние контактора, если он включен, тогда следим за состоянием светильников
    sprintf(query, "SELECT mt.* FROM device AS dt\n"
                   "LEFT JOIN sensor_channel AS sct ON sct.deviceUuid=dt.uuid\n"
                   "LEFT JOIN data AS mt ON mt.sensorChannelUuid=sct.uuid\n"
                   "WHERE dt.deviceTypeUuid='%s'\n"
                   "AND sct.measureTypeUuid='%s'", DEVICE_TYPE_ZB_COORDINATOR, CHANNEL_IN2);
    res = dBase->sqlexec(query);
    if (res) {
        nRows = mysql_num_rows(res);
        if (nRows > 0) {
            dBase->makeFieldsList(res);
            row = mysql_fetch_row(res);
            if (row) {
                contactorState = std::stoi(std::string(row[dBase->getFieldIndex("value")]));
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
    sprintf(query, "UPDATE device set deviceStatusUuid='%s', changedAt=current_timestamp() where\n"
                   "    device.uuid in (SELECT sct.deviceUuid FROM sensor_channel as sct\n"
                   "    LEFT JOIN data as mt on mt.sensorChannelUuid=sct.uuid\n"
                   "    WHERE (timestampdiff(second,  mt.changedAt, current_timestamp()) > %d OR mt.changedAt IS NULL)\n"
                   "    group by sct.deviceUuid)\n"
                   "    AND device.deviceTypeUuid IN ('%s', '%s')\n"
                   "    AND device.deviceStatusUuid='%s'", DEVICE_STATUS_NO_CONNECT, linkTimeOut,
            DEVICE_TYPE_ZB_LIGHT, DEVICE_TYPE_ZB_COORDINATOR, DEVICE_STATUS_WORK);
    res = dBase->sqlexec(query);
    mysql_free_result(res);

    // для всех светильников от которых были получены пакеты со статусом менее 30 секунд назад,
    // а статус был "Нет связи", устанавливаем статус "В порядке"
    sprintf(query, "UPDATE device set deviceStatusUuid='%s', changedAt=current_timestamp() where\n"
                   "    device.uuid in (SELECT sct.deviceUuid FROM sensor_channel as sct\n"
                   "    LEFT JOIN data as mt on mt.sensorChannelUuid=sct.uuid\n"
                   "    WHERE (timestampdiff(second,  mt.changedAt, current_timestamp()) < %d)\n"
                   "    group by sct.deviceUuid)\n"
                   "    AND device.deviceTypeUuid IN ('%s', '%s')\n"
                   "    AND device.deviceStatusUuid='%s'", DEVICE_STATUS_WORK, linkTimeOut, DEVICE_TYPE_ZB_LIGHT,
            DEVICE_TYPE_ZB_COORDINATOR, DEVICE_STATUS_NO_CONNECT);
    res = dBase->sqlexec(query);
    mysql_free_result(res);

    // для всех устройств у которых нет каналов измерения (светильники zb, координатор) ставим нет связи
    sprintf(query, "UPDATE device set deviceStatusUuid='%s', changedAt=current_timestamp()\n"
                   "    WHERE device.uuid NOT IN (\n"
                   "    SELECT sct.deviceUuid FROM sensor_channel AS sct GROUP BY sct.deviceUuid\n"
                   "    )\n"
                   "    AND device.deviceTypeUuid IN ('%s', '%s')\n"
                   "    AND device.deviceStatusUuid='%s'", DEVICE_STATUS_NO_CONNECT, DEVICE_TYPE_ZB_LIGHT,
            DEVICE_TYPE_ZB_COORDINATOR, DEVICE_STATUS_WORK);
    res = dBase->sqlexec(query);
    mysql_free_result(res);
}