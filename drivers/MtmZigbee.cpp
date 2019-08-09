#include <unistd.h>
#include <cstdio>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <cstring>
#include <nettle/base64.h>
#include <cstdio>
#include <zigbeemtm.h>
#include <zconf.h>
#include "dbase.h"
#include "TypeThread.h"
#include "MtmZigbee.h"
#include "kernel.h"
#include "function.h"
#include "ce102.h"
#include <uuid/uuid.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include "suninfo.h"
#include <stdlib.h>

int coordinatorFd;
bool mtmZigbeeStarted = false;
uint8_t TAG[] = "mtmzigbee";
pthread_mutex_t mtmZigbeeStopMutex;
bool mtmZigbeeStopIssued;
DBase *mtmZigbeeDBase;
int32_t mtmZigBeeThreadId;
Kernel *kernel;
bool isSunInit;
bool isLightEnabled;

void *mtmZigbeeDeviceThread(void *pth) { // NOLINT
    uint16_t speed;
    uint8_t *port;
    kernel = &Kernel::Instance();
    auto *tInfo = (TypeThread *) pth;

    mtmZigBeeThreadId = tInfo->id;
    speed = tInfo->speed;
    int portStrLen = strlen(tInfo->port);
    port = (uint8_t *) malloc(portStrLen + 1);
    memset((void *) port, 0, portStrLen + 1);
    strcpy((char *) port, tInfo->port);

    if (!mtmZigbeeStarted) {
        isSunInit = false;
        isLightEnabled = false;

        mtmZigbeeStarted = true;
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] device thread started", TAG);
        if (mtmZigbeeInit(MTM_ZIGBEE_COM_PORT, port, speed) == 0) {
            // запускаем цикл разбора пакетов
            mtmZigbeePktListener(mtmZigBeeThreadId);
        }
    } else {
        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] thread already started", TAG);
        free(port);
        return nullptr;
    }

    mtmZigbeeStarted = false;
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] device thread ended", TAG);

    if (coordinatorFd > 0) {
        close(coordinatorFd);
    }

    mtmZigbeeDBase->disconnect();
    delete mtmZigbeeDBase;
    free(port);

    return nullptr;
}


void mtmZigbeePktListener(int32_t threadId) {
    bool run = true;
    int64_t count;
    uint32_t i = 0;
    uint8_t data;
    uint8_t seek[1024];
    //---
    bool isSof = false;
    bool isFrameLen = false;
    uint8_t frameLen = 0;
    bool isCommand = false;
    uint16_t commandByteCount = 0;
    bool isFrameData = false;
    uint8_t frameDataByteCount = 0;
    uint8_t fcs;
    time_t currentTime, heartBeatTime = 0, syncTimeTime = 0, checkSensorTime = 0, checkAstroTime = 0;
    struct tm *localTime;

    struct zb_pkt_item {
//        zigbee_frame frame;
        void *pkt;
        uint32_t len;
        SLIST_ENTRY(zb_pkt_item) items;
    };
//    struct zb_queue *zb_queue_ptr;
    SLIST_HEAD(zb_queue, zb_pkt_item)
            zb_queue_head = SLIST_HEAD_INITIALIZER(zb_queue_head);
    SLIST_INIT(&zb_queue_head);
//    zb_queue_ptr = (struct zb_queue *) (&zb_queue_head);
    struct zb_pkt_item *zb_item;

    mtmZigbeeSetRun(true);

    while (run) {
        count = read(coordinatorFd, &data, 1);
        if (count > 0) {
//            printf("data: %02X\n", data);

            // TODO: сделать вложенные if
            // начинаем разбор
            if (!isSof && data == SOF) {
                i = 0;
                isSof = true;
                seek[i++] = data;
                if (DEBUG) {
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] found SOF\n", TAG);
                }
            } else if (!isFrameLen) {
                isFrameLen = true;
                seek[i++] = frameLen = data;
                if (DEBUG) {
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] found frame len\n", TAG);
                }
            } else if (!isCommand) {
                commandByteCount++;
                seek[i++] = data;
                if (commandByteCount == 2) {
                    commandByteCount = 0;
                    isCommand = true;
                    if (DEBUG) {
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] found command\n", TAG);
                    }
                }
            } else if (!isFrameData && frameDataByteCount < frameLen) {
                seek[i++] = data;
                frameDataByteCount++;
                if (frameDataByteCount == frameLen) {
                    isFrameData = true;
                    frameDataByteCount = 0;
                    if (DEBUG) {
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] found frame data\n", TAG);
                    }
                }
            } else {
                // нашли контрольную сумму
                seek[i++] = data;
                if (DEBUG) {
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] found FCS\n", TAG);
                }

                // пакет вроде как разобран
                // нужно проверить контрольную сумму фрейма
                fcs = compute_fcs(seek, i);
                if (fcs == seek[i - 1]) {
                    if (DEBUG) {
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] frame good\n", TAG);
                    }

                    // складываем полученный пакет в список
                    zb_item = (struct zb_pkt_item *) malloc(sizeof(struct zb_pkt_item));
                    zb_item->len = i - 1;
                    zb_item->pkt = malloc(zb_item->len);
                    memcpy(zb_item->pkt, seek, zb_item->len);
                    SLIST_INSERT_HEAD(&zb_queue_head, zb_item, items);
                } else {
                    if (DEBUG) {
                        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] frame bad\n", TAG);
                    }
                    // вероятно то что попадает в порт с модуля zigbee уже проверено им самим
                    // как проверить это предположение? попробовать послать порченый пакет.
                    // либо он не будет отправлен, либо не попадёт в порт т.к. порченый, либо попадёт в порт мне на обработку
                    // считаем что такое не возможно - проверить
                }

                // сбрасываем состояние алгоритма разбора пакета zigbee
                isSof = false;
                isFrameLen = false;
                isCommand = false;
                isFrameData = false;
                i = 0;
            }
        } else {
            // есть свободное время, разбираем список полученных пакетов
            while (!SLIST_EMPTY(&zb_queue_head)) {
                if (DEBUG) {
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] processing zb packet...\n", TAG);
                }

                zb_item = SLIST_FIRST(&zb_queue_head);
                mtmZigbeeProcessInPacket((uint8_t *) zb_item->pkt, zb_item->len);
                SLIST_REMOVE_HEAD(&zb_queue_head, items);
                free(zb_item->pkt);
                free(zb_item);
            }

            // обновляем значение c_time в таблице thread раз в 5 секунд
            currentTime = time(nullptr);
            if (currentTime - heartBeatTime >= 5) {
                heartBeatTime = currentTime;
                UpdateThreads(*mtmZigbeeDBase, threadId, 0, 1);
            }

            // рассылаем пакет с текущим "временем" раз в 10 секунд
            currentTime = time(nullptr);
            if (currentTime - syncTimeTime >= 10) {
                syncTimeTime = currentTime;
                mtm_cmd_current_time current_time;
                current_time.header.type = MTM_CMD_TYPE_CURRENT_TIME;
                current_time.header.protoVersion = MTM_VERSION_0;
                localTime = localtime(&currentTime);
                current_time.time = localTime->tm_hour * 60 + localTime->tm_min;
                ssize_t rc = send_mtm_cmd(coordinatorFd, 0xFFFF, &current_time);
                if (DEBUG) {
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.\n", TAG, rc);
                }
            }

            // опрашиваем датчики на локальном координаторе
            currentTime = time(nullptr);
            if (currentTime - checkSensorTime >= 10) {
                checkSensorTime = currentTime;
                zigbee_mt_cmd_af_data_request req = {0};
                req.dst_addr = 0x0000;
                req.sep = 0xE8;
                req.dep = 0xE8;
                req.cid = 0x0103;
                ssize_t rc = send_zb_cmd(coordinatorFd, AF_DATA_REQUEST, &req);
                if (DEBUG) {
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld\n", TAG, rc);
                }
            }

            // проверка на наступление астрономических событий
            currentTime = time(nullptr);
            if (currentTime - checkAstroTime > 60) {
                checkAstroTime = currentTime;
                struct tm *ctm = localtime(&currentTime);
                double lon = 0, lat = 0;
                double rise, set;
                int rs;
                uint64_t checkTime;
                uint64_t sunRiseTime;
                uint64_t sunSetTime;
                mtm_cmd_action action = {0};

                MYSQL_RES *res = mtmZigbeeDBase->sqlexec("SELECT * FROM node LIMIT 1");
                if (res) {
                    MYSQL_ROW row = mysql_fetch_row(res);
                    mtmZigbeeDBase->makeFieldsList(res);
                    if (row) {
                        lon = strtod(row[mtmZigbeeDBase->getFieldIndex("longitude")], nullptr);
                        lat = strtod(row[mtmZigbeeDBase->getFieldIndex("latitude")], nullptr);
                    }

                    mysql_free_result(res);
                }

                rs = sun_rise_set(ctm->tm_year + 1900, ctm->tm_mon + 1, ctm->tm_mday, lon, lat, &rise, &set);
                bool isCheckTimeAboveSunSet;
                bool isCheckTimeLessSunSet;
                bool isCheckTimeAboveSunRise;
                bool isCheckTimeLessSunRise;
                switch (rs) {
                    case 0:
                        checkTime = ctm->tm_hour * 3600 + ctm->tm_min * 60 + ctm->tm_sec;
                        sunRiseTime = (uint64_t) (rise * 3600 + ctm->tm_gmtoff);
                        sunSetTime = (uint64_t) (set * 3600 + ctm->tm_gmtoff);

                        action.header.type = MTM_CMD_TYPE_ACTION;
                        action.header.protoVersion = MTM_VERSION_0;
                        action.device = MTM_DEVICE_LIGHT;

                        isCheckTimeAboveSunSet = checkTime > sunSetTime;
                        isCheckTimeLessSunSet = checkTime < sunSetTime;
                        isCheckTimeAboveSunRise = checkTime > sunRiseTime;
                        isCheckTimeLessSunRise = checkTime < sunRiseTime;

                        if ((isCheckTimeAboveSunSet || isCheckTimeLessSunRise) && (!isLightEnabled || !isSunInit)) {
                            isSunInit = true;
                            isLightEnabled = true;
                            // включаем контактор, зажигаем светильники, отправляем команду "закат"
                            switchContactor(true, MBEE_API_DIGITAL_LINE7);
                            // даём задержку для того чтоб стартанули модули в светильниках
                            // т.к. неизвестно, питаются они через контактор или всё время под напряжением
                            sleep(5);
                            switchAllLight(100);
                            // передаём команду "астро событие" "закат"
                            action.data = (0x02 << 8 | 0x01); // NOLINT(hicpp-signed-bitwise)
                            ssize_t rc = send_mtm_cmd(coordinatorFd, 0xFFFF, &action);
                            if (DEBUG) {
                                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld\n", TAG, rc);
                            }
                        } else if ((isCheckTimeAboveSunRise && isCheckTimeLessSunSet) &&
                                   (isLightEnabled || !isSunInit)) {
                            isSunInit = true;
                            isLightEnabled = false;
                            // выключаем контактор, гасим светильники, отправляем команду "восход"
                            switchContactor(false, MBEE_API_DIGITAL_LINE7);
                            // на всякий случай, если светильники всегда под напряжением
                            switchAllLight(0);
                            // передаём команду "астро событие" "восход"
                            action.data = (0x02 << 8 | 0x00); // NOLINT(hicpp-signed-bitwise)
                            ssize_t rc = send_mtm_cmd(coordinatorFd, 0xFFFF, &action);
                            if (DEBUG) {
                                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld\n", TAG, rc);
                            }
                        } else {
                            // ситуация когда мы не достигли условий переключения состояния светильников
                        }

                        break;
                    case 1:
                        // солнце всегда над горизонтом - принудительно отключаем контактор, гасим все светильники?
                        break;
                    case -1:
                        // солнце всегда за горизонтом - принудительно включаем контактор, зажигаем все светильники?
                        break;
                    default:
                        break;
                }

            }

            mtmZigbeeProcessOutPacket();

            run = mtmZigbeeGetRun();

            usleep(10000);
        }
    }
}

ssize_t switchAllLight(uint16_t level) {
    mtm_cmd_action action = {0};
    action.header.type = MTM_CMD_TYPE_ACTION;
    action.header.protoVersion = MTM_VERSION_0;
    action.device = MTM_DEVICE_LIGHT;
    action.data = level;
    ssize_t rc = send_mtm_cmd(coordinatorFd, 0xFFFF, &action);
    return rc;
}

ssize_t switchContactor(bool enable, uint8_t line) {
    zigbee_mt_cmd_af_data_request request = {0};
    request.dst_addr = 0x0000; // пока тупо локальному координатору отправляем
    request.dep = MBEE_API_END_POINT;
    request.sep = MBEE_API_END_POINT;
    request.cid = enable ? line : line | 0x80u;
    request.tid = 0x00;
    request.opt = ZB_O_APS_ACKNOWLEDGE; // флаг для получения подтверждения с конечного устройства а не с первого хопа.
    request.rad = MAX_PACKET_HOPS;
    request.adl = 0x00;
    ssize_t rc = send_zb_cmd(coordinatorFd, AF_DATA_REQUEST, &request);
    return rc;
}

void mtmZigbeeProcessOutPacket() {
    uint8_t query[1024];
    MYSQL_RES *res;
    MYSQL_ROW row;
    uint8_t fieldIdx;
    u_long nRows;
    unsigned long *lengths;
    long flen;
    uint8_t tmpAddr[1024];
    uint8_t tmpData[1024];
    uint8_t mtmPkt[512];
    uint64_t dstAddr;
    ssize_t rc;

    sprintf((char *) query, "SELECT * FROM light_message WHERE dateOut IS NULL;");
    if (DEBUG) {
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s\n", TAG, query);
    }

    res = mtmZigbeeDBase->sqlexec((char *) query);
    if (res) {
        nRows = mysql_num_rows(res);
        if (nRows == 0) {
            mysql_free_result(res);
            return;
        }

        for (uint32_t i = 0; i < nRows; i++) {
            row = mysql_fetch_row(res);
            lengths = mysql_fetch_lengths(res);
            mtmZigbeeDBase->makeFieldsList(res);
            if (row) {
                fieldIdx = mtmZigbeeDBase->getFieldIndex("address");
                flen = lengths[fieldIdx];
                memset(tmpAddr, 0, 1024);
                strncpy((char *) tmpAddr, row[fieldIdx], flen);
                if (DEBUG) {
                    kernel->log.ulogw(LOG_LEVEL_INFO, "Addr: %s, ", tmpAddr);
                }

                dstAddr = strtoull((char *) tmpAddr, nullptr, 16);

                fieldIdx = mtmZigbeeDBase->getFieldIndex("data");
                flen = lengths[fieldIdx];
                memset(tmpData, 0, 1024);
                strncpy((char *) tmpData, row[fieldIdx], flen);
                if (DEBUG) {
                    kernel->log.ulogw(LOG_LEVEL_INFO, "Data: %s\n", tmpData);
                }

                struct base64_decode_ctx b64_ctx = {};
                size_t decoded = 512;
                base64_decode_init(&b64_ctx);
                if (base64_decode_update(&b64_ctx, &decoded, mtmPkt, flen, tmpData)) {
                    if (base64_decode_final(&b64_ctx)) {
                        uint8_t pktType = mtmPkt[0];
                        switch (pktType) {
                            case MTM_CMD_TYPE_CONFIG:
                                if (DEBUG) {
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_CONFIG\n", TAG);
                                }

                                mtm_cmd_config config;
                                config.header.type = mtmPkt[0];
                                config.header.protoVersion = mtmPkt[1];
                                config.device = mtmPkt[2];
                                config.min = *(uint16_t *) &mtmPkt[3];
                                config.max = *(uint16_t *) &mtmPkt[5];

                                if (dstAddr == 0xffff) {
                                    rc = send_mtm_cmd(coordinatorFd, dstAddr, &config);
                                } else {
                                    rc = send_mtm_cmd_ext(coordinatorFd, dstAddr, &config);
                                }

                                if (DEBUG) {
                                    log_buffer_hex(mtmPkt, decoded);
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.\n", TAG, rc);
                                }

                                break;
                            case MTM_CMD_TYPE_CONFIG_LIGHT:
                                if (DEBUG) {
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_CONFIG_LIGHT\n", TAG);
                                }

                                mtm_cmd_config_light config_light;
                                config_light.header.type = mtmPkt[0];
                                config_light.header.protoVersion = mtmPkt[1];
                                config_light.device = mtmPkt[2];
                                for (int nCfg = 0; nCfg < MAX_LIGHT_CONFIG; nCfg++) {
                                    config_light.config[nCfg].time = *(uint16_t *) &mtmPkt[3 + nCfg * 4];
                                    config_light.config[nCfg].value = *(uint16_t *) &mtmPkt[3 + nCfg * 4 + 2];
                                }

                                if (dstAddr == 0xFFFF) {
                                    rc = send_mtm_cmd(coordinatorFd, dstAddr, &config_light);
                                } else {
                                    rc = send_mtm_cmd_ext(coordinatorFd, dstAddr, &config_light);
                                }

                                if (DEBUG) {
                                    log_buffer_hex(mtmPkt, decoded);
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.\n", TAG, rc);
                                }

                                break;
                            case MTM_CMD_TYPE_CURRENT_TIME:
                                if (DEBUG) {
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_CURRENT_TIME\n", TAG);
                                }

                                mtm_cmd_current_time current_time;
                                current_time.header.type = mtmPkt[0];
                                current_time.header.protoVersion = mtmPkt[1];
                                current_time.time = *(uint16_t *) &mtmPkt[2];

                                if (dstAddr == 0xFFFF) {
                                    rc = send_mtm_cmd(coordinatorFd, dstAddr, &current_time);
                                } else {
                                    rc = send_mtm_cmd_ext(coordinatorFd, dstAddr, &current_time);
                                }

                                if (DEBUG) {
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.\n", TAG, rc);
                                    log_buffer_hex(mtmPkt, decoded);
                                }

                                break;
                            case MTM_CMD_TYPE_ACTION:
                                if (DEBUG) {
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_ACTION\n", TAG);
                                }

                                mtm_cmd_action action;
                                action.header.type = mtmPkt[0];
                                action.header.protoVersion = mtmPkt[1];
                                action.device = mtmPkt[2];
                                action.data = *(uint16_t *) &mtmPkt[3];

                                if (dstAddr == 0xFFFF) {
                                    rc = send_mtm_cmd(coordinatorFd, dstAddr, &action);
                                } else {
                                    rc = send_mtm_cmd_ext(coordinatorFd, dstAddr, &action);
                                }

                                if (DEBUG) {
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.\n", TAG, rc);
                                    log_buffer_hex(mtmPkt, decoded);
                                }

                                break;
                            case MTM_CMD_TYPE_CONTACTOR:
                                if (DEBUG) {
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_CONTACTOR\n", TAG);
                                    log_buffer_hex(mtmPkt, decoded);
                                }

                                rc = switchContactor(mtmPkt[3], mtmPkt[2]);
                                break;
                            default:
                                rc = -1;
                                break;
                        }

                        if (rc > 0) {
                            sprintf((char *) query, "UPDATE light_message SET dateOut=FROM_UNIXTIME(%ld) WHERE _id=%ld",
                                    time(nullptr), strtoul(row[0], nullptr, 10));
                            MYSQL_RES *updRes = mtmZigbeeDBase->sqlexec((char *) query);
                            if (updRes) {
                                mysql_free_result(updRes);
                            }
                        }
                    }
                }
            }
        }

        mysql_free_result(res);
    }

}

void mtmZigbeeProcessInPacket(uint8_t *pktBuff, uint32_t len) {

    uint16_t cmd = *(uint16_t *) (&pktBuff[2]);
    uint8_t pktType;
    uint8_t resultBuff[1024] = {};
    struct base64_encode_ctx b64_ctx = {};
    int encoded_bytes;
    uint8_t dstEndPoint;
    uint16_t cluster;

    uint8_t deviceUuid[64];
    uint8_t sChannelUuid[64];
    uuid_t newUuid;
    uint8_t newMeasureUuid[37] = {0};

    uint8_t query[1024];
    uint8_t addr[32];
    MYSQL_RES *res;
    time_t createTime = time(nullptr);

    switch (cmd) {
        case AF_DATA_RESPONSE:
            if (DEBUG) {
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] AF_DATA_RESPONSE\n", TAG);
            }

            dstEndPoint = pktBuff[6];
            cluster = *(uint16_t *) (&pktBuff[4]);
            if (dstEndPoint != MBEE_API_END_POINT) {
                return;
            }

            switch (cluster) { // NOLINT
                case 0x0103 :
                    uint16_t value;

                    memset(addr, 0, 32);
                    sprintf((char *) addr, "%02X%02X%02X%02X%02X%02X%02X%02X",
                            pktBuff[17], pktBuff[16], pktBuff[15], pktBuff[14],
                            pktBuff[13], pktBuff[12], pktBuff[11], pktBuff[10]);

                    if (DEBUG) {
                        kernel->log.ulogw(LOG_LEVEL_INFO, "get sensor data packet!!!!!\n");
                    }

                    memset(deviceUuid, 0, 64);
                    if (findDevice(addr, deviceUuid)) {
                        // найти канал по устройству sensor_channel и regIdx
                        memset(sChannelUuid, 0, 64);
                        if (!findSChannel(deviceUuid, MTM_ZB_CHANNEL_COORD_IN1_IDX, sChannelUuid)) {
                            // если нет, создать
                            uuid_generate(newUuid);
                            uuid_unparse_upper(newUuid, (char *) sChannelUuid);
                            if (!createSChannel(sChannelUuid, MTM_ZB_CHANNEL_COORD_IN1_TITLE,
                                                MTM_ZB_CHANNEL_COORD_IN1_IDX, deviceUuid, CHANNEL_IN1, createTime)) {
                                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Неудалось канал измерение",
                                                  MTM_ZB_CHANNEL_COORD_IN1_TITLE);
                            }
                        }

                        // создать новое измерение для канала
                        uuid_generate(newUuid);
                        memset(newMeasureUuid, 0, 37);
                        uuid_unparse_upper(newUuid, (char *) newMeasureUuid);
                        value = *(uint16_t *) (&pktBuff[34]);
                        if (!storeMeasureValue(newMeasureUuid, sChannelUuid, (double) value, createTime, createTime)) {
                            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG,
                                              "Не удалось сохранить измерение", MTM_ZB_CHANNEL_COORD_IN1_TITLE);
                        }

                        // найти канал по устройству sensor_channel и regIdx
                        memset(sChannelUuid, 0, 64);
                        if (!findSChannel(deviceUuid, MTM_ZB_CHANNEL_COORD_IN2_IDX, sChannelUuid)) {
                            // если нет, создать
                            uuid_generate(newUuid);
                            uuid_unparse_upper(newUuid, (char *) sChannelUuid);
                            if (!createSChannel(sChannelUuid, MTM_ZB_CHANNEL_COORD_IN2_TITLE,
                                                MTM_ZB_CHANNEL_COORD_IN2_IDX, deviceUuid, CHANNEL_IN2, createTime)) {
                                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Неудалось канал измерение",
                                                  MTM_ZB_CHANNEL_COORD_IN2_TITLE);
                            }
                        }

                        // создать новое измерение для канала
                        uuid_generate(newUuid);
                        memset(newMeasureUuid, 0, 37);
                        uuid_unparse_upper(newUuid, (char *) newMeasureUuid);
                        value = *(uint16_t *) (&pktBuff[36]);
                        if (!storeMeasureValue(newMeasureUuid, sChannelUuid, (double) value, createTime, createTime)) {
                            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG,
                                              "Не удалось сохранить измерение", MTM_ZB_CHANNEL_COORD_IN2_TITLE);
                        }

                        // найти канал по устройству sensor_channel и regIdx (цифровой пин управления контактором)
                        memset(sChannelUuid, 0, 64);
                        if (!findSChannel(deviceUuid, MTM_ZB_CHANNEL_COORD_DIGI1_IDX, sChannelUuid)) {
                            // если нет, создать
                            uuid_generate(newUuid);
                            uuid_unparse_upper(newUuid, (char *) sChannelUuid);
                            if (!createSChannel(sChannelUuid, MTM_ZB_CHANNEL_COORD_DIGI1_TITLE,
                                                MTM_ZB_CHANNEL_COORD_DIGI1_IDX, deviceUuid, CHANNEL_DIGI1,
                                                createTime)) {
                                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG, "Неудалось канал измерение",
                                                  MTM_ZB_CHANNEL_COORD_DIGI1_TITLE);
                            }
                        }

                        // создать новое измерение для канала
                        uuid_generate(newUuid);
                        memset(newMeasureUuid, 0, 37);
                        uuid_unparse_upper(newUuid, (char *) newMeasureUuid);
                        value = *(uint16_t *) (&pktBuff[32]);
                        value &= 0x0040u;
                        value = value >> 6; // NOLINT(hicpp-signed-bitwise)
                        if (!storeMeasureValue(newMeasureUuid, sChannelUuid, (double) value, createTime, createTime)) {
                            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG,
                                              "Не удалось сохранить измерение", MTM_ZB_CHANNEL_COORD_DIGI1_TITLE);
                        }
                    }

                    break;
                default:
                    break;
            }

            break;
        case AF_INCOMING_MSG:
            if (DEBUG) {
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] AF_INCOMING_MSG\n", TAG);
            }

            dstEndPoint = pktBuff[11];
            cluster = *(uint16_t *) (&pktBuff[6]);
            if (dstEndPoint != MTM_API_END_POINT || cluster != MTM_API_CLUSTER) {
                break;
            }

            pktType = pktBuff[21];
            switch (pktType) {
                case MTM_CMD_TYPE_STATUS:
                    len = len - 33;
                    if (DEBUG) {
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] MTM_CMD_TYPE_STATUS\n", TAG);
                        log_buffer_hex(&pktBuff[21], len + 12);
                    }

                    base64_encode_init(&b64_ctx);

#ifdef __APPLE__
                encoded_bytes = base64_encode_update(&b64_ctx, (char *) resultBuff, get_mtm_command_size(pktType),
                                                     reinterpret_cast<const uint8_t *>((size_t) &pktBuff[21]));
                base64_encode_final(&b64_ctx, reinterpret_cast<char *>(resultBuff + encoded_bytes));
#elif __USE_GNU
                    encoded_bytes = base64_encode_update(&b64_ctx, resultBuff, len + 12, &pktBuff[21]);
                    base64_encode_final(&b64_ctx, resultBuff + encoded_bytes);
#endif

                    memset(addr, 0, 32);
                    sprintf((char *) addr, "%02X%02X%02X%02X%02X%02X%02X%02X",
                            pktBuff[30], pktBuff[29], pktBuff[28], pktBuff[27],
                            pktBuff[26], pktBuff[25], pktBuff[24], pktBuff[23]);

                    // TODO: нужно ли сохранять эти данные?
                    sprintf((char *) query,
                            "INSERT INTO light_answer (address, data, createdAt, changedAt, dateIn) value('%s', '%s', FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld))",
                            addr, resultBuff, createTime, createTime, createTime);
                    if (DEBUG) {
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s\n", TAG, query);
                    }

                    res = mtmZigbeeDBase->sqlexec((const char *) query);
                    if (res) {
                        mysql_free_result(res);
                    }

                    for (uint8_t statusIdx = 0; statusIdx < len / 2; statusIdx++) {
                        switch (statusIdx) {
                            case 0 :
                                memset(deviceUuid, 0, 64);
                                if (findDevice(addr, deviceUuid)) {
                                    // найти канал по устройству sensor_channel и regIdx (Температура светильника)
                                    memset(sChannelUuid, 0, 64);
                                    if (!findSChannel(deviceUuid, MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_IDX, sChannelUuid)) {
                                        // если нет, создать
                                        uuid_generate(newUuid);
                                        uuid_unparse_upper(newUuid, (char *) sChannelUuid);
                                        if (!createSChannel(sChannelUuid, MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_TITLE,
                                                            MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_IDX, deviceUuid, CHANNEL_T,
                                                            createTime)) {
                                            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG,
                                                              "Неудалось канал измерение ",
                                                              MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_TITLE);
                                        }
                                    }

                                    // создать новое измерение для канала
                                    uuid_generate(newUuid);
                                    memset(newMeasureUuid, 0, 37);
                                    uuid_unparse_upper(newUuid, (char *) newMeasureUuid);
                                    int8_t value = pktBuff[34];
                                    if (!storeMeasureValue(newMeasureUuid, sChannelUuid, (double) value, createTime,
                                                           createTime)) {
                                        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG,
                                                          "Не удалось сохранить измерение",
                                                          MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_TITLE);
                                    }

                                    // найти канал по устройству sensor_channel и regIdx (Ток светильника)
                                    memset(sChannelUuid, 0, 64);
                                    if (!findSChannel(deviceUuid, MTM_ZB_CHANNEL_LIGHT_CURRENT_IDX, sChannelUuid)) {
                                        // если нет, создать
                                        uuid_generate(newUuid);
                                        uuid_unparse_upper(newUuid, (char *) sChannelUuid);
                                        if (!createSChannel(sChannelUuid, MTM_ZB_CHANNEL_LIGHT_CURRENT_TITLE,
                                                            MTM_ZB_CHANNEL_LIGHT_CURRENT_IDX, deviceUuid, CHANNEL_I,
                                                            createTime)) {
                                            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG,
                                                              "Неудалось канал измерение ",
                                                              MTM_ZB_CHANNEL_LIGHT_CURRENT_TITLE);
                                        }
                                    }

                                    // создать новое измерение для канала
                                    uuid_generate(newUuid);
                                    memset(newMeasureUuid, 0, 37);
                                    uuid_unparse_upper(newUuid, (char *) newMeasureUuid);
                                    value = pktBuff[33];
                                    if (!storeMeasureValue(newMeasureUuid, sChannelUuid, (double) value, createTime,
                                                           createTime)) {
                                        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG,
                                                          "Не удалось сохранить измерение",
                                                          MTM_ZB_CHANNEL_LIGHT_CURRENT_TITLE);
                                    }

                                    // найти канал по устройству sensor_channel и regIdx (Состояние светильника)
                                    memset(sChannelUuid, 0, 64);
                                    if (!findSChannel(deviceUuid, MTM_ZB_CHANNEL_LIGHT_STATUS_IDX, sChannelUuid)) {
                                        // если нет, создать
                                        uuid_generate(newUuid);
                                        uuid_unparse_upper(newUuid, (char *) sChannelUuid);
                                        if (!createSChannel(sChannelUuid, MTM_ZB_CHANNEL_LIGHT_STATUS_TITLE,
                                                            MTM_ZB_CHANNEL_LIGHT_STATUS_IDX, deviceUuid, CHANNEL_STATUS,
                                                            createTime)) {
                                            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG,
                                                              "Неудалось канал измерение ",
                                                              MTM_ZB_CHANNEL_LIGHT_STATUS_TITLE);
                                        }
                                    }

                                    // создать новое измерение для канала
                                    uuid_generate(newUuid);
                                    memset(newMeasureUuid, 0, 37);
                                    uuid_unparse_upper(newUuid, (char *) newMeasureUuid);
                                    uint16_t alerts = *(uint16_t *) &pktBuff[31];
                                    value = alerts & 0x0001u;
                                    if (!storeMeasureValue(newMeasureUuid, sChannelUuid, (double) value, createTime,
                                                           createTime)) {
                                        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s\n", TAG,
                                                          "Не удалось сохранить измерение",
                                                          MTM_ZB_CHANNEL_LIGHT_STATUS_TITLE);
                                    }
                                }
                                break;
                            case 1 :
                            case 2 :
                            case 3 :
                            case 4 :
                            case 5 :
                            case 6 :
                            case 7 :
                            case 8 :
                            case 9 :
                            case 10 :
                            case 11 :
                            case 12 :
                            case 13 :
                            case 14 :
                            case 15 :
                            default:
                                break;
                        }
                    }
                    break;
                case MTM_CMD_TYPE_CONFIG:
                case MTM_CMD_TYPE_CONFIG_LIGHT:
                case MTM_CMD_TYPE_CURRENT_TIME:
                case MTM_CMD_TYPE_ACTION:
                default:
                    break;
            }
            break;
        case AF_INCOMING_MSG_EXT:
            if (DEBUG) {
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] AF_INCOMING_MSG_EXT\n", TAG);
            }

            break;
        default:
            break;
    }
}

int32_t mtmZigbeeInit(int32_t mode, uint8_t *path, uint32_t speed) {
    struct termios serialPortSettings{};

    // TODO: видимо нужно как-то проверить что всё путём с соединением.
    mtmZigbeeDBase = new DBase();

    if (mtmZigbeeDBase->openConnection() != 0) {
        return -1;
    }

    if (mode == MTM_ZIGBEE_FIFO) {
        // создаём fifo для тестов
        const char *fifo = "/tmp/zbfifo";
        coordinatorFd = open(fifo, O_NONBLOCK | O_RDWR | O_NOCTTY); // NOLINT(hicpp-signed-bitwise)
        if (coordinatorFd == -1) {
            // пробуем создать
            coordinatorFd = mkfifo((char *) fifo, 0666);
            if (coordinatorFd == -1) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] FIFO can not make!!!\n", TAG);
                return -1;
            } else {
                close(coordinatorFd);
                coordinatorFd = open((char *) fifo, O_NONBLOCK | O_RDWR | O_NOCTTY); // NOLINT(hicpp-signed-bitwise)
                if (coordinatorFd == -1) {
                    kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] FIFO can not open!!!\n", TAG);
                    return -1;
                }
            }
        }
    } else {
        if (access((char *) path, F_OK) != -1) {
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] init %s\n", TAG, path);
        } else {
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s path not exists\n", TAG, path);
            return -2;
        }

        // открываем порт
        coordinatorFd = open((char *) path, O_NONBLOCK | O_RDWR | O_NOCTTY); // NOLINT(hicpp-signed-bitwise)
        if (coordinatorFd == -1) {
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] can not open file\n", TAG);
            return -3;
        }

        // инициализируем
        tcgetattr(coordinatorFd, &serialPortSettings);

        /* Setting the Baud rate */
        cfsetispeed(&serialPortSettings, mtmZigbeeGetSpeed(speed));
        cfsetospeed(&serialPortSettings, mtmZigbeeGetSpeed(speed));

        /* 8N1 Mode */
        serialPortSettings.c_cflag &= ~PARENB;   /* Disables the Parity Enable bit(PARENB),So No Parity   */ // NOLINT(hicpp-signed-bitwise)
        serialPortSettings.c_cflag &= HUPCL;     /* принудительно выключаем DTR так как через него мы управляем сбросом модуля zigbee */ // NOLINT(hicpp-signed-bitwise)
        serialPortSettings.c_cflag &= ~CSTOPB;   /* CSTOPB = 2 Stop bits,here it is cleared so 1 Stop bit */ // NOLINT(hicpp-signed-bitwise)
        serialPortSettings.c_cflag &= ~CSIZE;    /* Clears the mask for setting the data size             */ // NOLINT(hicpp-signed-bitwise)
        serialPortSettings.c_cflag |= CS8; // NOLINT(hicpp-signed-bitwise)
        serialPortSettings.c_cflag &= ~CRTSCTS;
        serialPortSettings.c_cflag |= CREAD | CLOCAL; // NOLINT(hicpp-signed-bitwise)

        serialPortSettings.c_iflag &= ~(IXON | IXOFF | IXANY); // NOLINT(hicpp-signed-bitwise)
        serialPortSettings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG | ECHONL);   // NOLINT(hicpp-signed-bitwise)
        serialPortSettings.c_oflag &= ~OPOST;  // NOLINT(hicpp-signed-bitwise)

        serialPortSettings.c_cc[VMIN] = 1;
        serialPortSettings.c_cc[VTIME] = 5;

        cfmakeraw(&serialPortSettings);

        if ((tcsetattr(coordinatorFd, TCSANOW, &serialPortSettings)) != 0) {
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR ! in Setting attributes\n", TAG);
            return -4;
        } else {
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] BaudRate = 38400\nStopBits = 1\nParity = none\n", TAG);
        }

        tcflush(coordinatorFd, TCIFLUSH);   /* Discards old data in the rx buffer            */
    }


    send_zb_cmd(coordinatorFd, ZB_SYSTEM_RESET, nullptr);

    sleep(1);

    // регистрируем свою конечную точку
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Register our end point.\n", TAG);
    zigbee_mt_af_register af_register;
    af_register.ep = MTM_API_END_POINT;
    af_register.app_prof_id = MTM_PROFILE_ID;
    af_register.app_device_id = 0x0101;
    af_register.app_device_version = 0x01;
    af_register.latency_req = NO_LATENCY;
    af_register.app_num_in_clusters = 1;
    af_register.app_in_cluster_list[0] = 0xFC00;
    af_register.app_num_out_clusters = 1;
    af_register.app_out_cluster_list[0] = 0xFC00;
    ssize_t rc = send_zb_cmd(coordinatorFd, AF_REGISTER, &af_register);
    if (DEBUG) {
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld\n", TAG, rc);
    }

    // тестовый пакет с состоянием светильника
//    uint8_t buff[] = {
//            0xfe,
//            0x1f, 0x44, 0x81,
//            0x00, 0x00,
//            0x00, 0xfc,
//            0x12, 0x34,
//            0xe9, 0xe9,
//            0x00, 0x00, 0x00,
//            0x01, 0x02, 0x03, 0x04,
//            0x00,
//            0x0e,
//            0x01, 0x00, 0x0a, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x01, 0x00, 0x42, 0x4D,
//            0x0D
//    };
//    send_cmd(coordinatorFd, buff, sizeof(buff));

    return 0;
}

speed_t mtmZigbeeGetSpeed(uint32_t speed) {
    switch (speed) {
        case 9600:
            return B9600;
        case 19200:
            return B19200;
        case 38400:
            return B38400;
        case 57600:
            return B57600;
        case 115200:
            return B115200;
        default:
            return B38400;
    }
}

bool mtmZigbeeGetRun() {
    bool ret;
    pthread_mutex_lock(&mtmZigbeeStopMutex);
    ret = mtmZigbeeStopIssued;
    pthread_mutex_unlock(&mtmZigbeeStopMutex);
    return ret;
}

void mtmZigbeeSetRun(bool val) {
    pthread_mutex_lock(&mtmZigbeeStopMutex);
    mtmZigbeeStopIssued = val;
    pthread_mutex_unlock(&mtmZigbeeStopMutex);
}

bool findSChannel(uint8_t *deviceUuid, uint8_t regIdx, uint8_t *sChannelUuid) {
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

    sprintf((char *) query, "SELECT * FROM sensor_channel WHERE deviceUuid LIKE '%s' AND register LIKE '%d'",
            deviceUuid, regIdx);
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
                strncpy((char *) sChannelUuid, row[fieldUuidIdx], flen);
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

bool storeMeasureValue(uint8_t *uuid, uint8_t *channelUuid, double value, time_t createTime, time_t changedTime) {
    MYSQL_RES *res;
    char query[1024];

    sprintf(query,
            "INSERT INTO data (uuid, sensorChannelUuid, value, date, createdAt) value('%s', '%s', %f, FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld))",
            uuid, channelUuid, value, createTime, changedTime);
    if (DEBUG) {
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s\n", TAG, query);
    }

    res = mtmZigbeeDBase->sqlexec((const char *) query);
    if (res) {
        mysql_free_result(res);
    }

    return mtmZigbeeDBase->isError();
}

bool createSChannel(uint8_t *uuid, const char *channelTitle, uint8_t sensorIndex, uint8_t *deviceUuid,
                    const char *channelTypeUuid, time_t createTime) {
    char query[1024];
    MYSQL_RES *res;
    sprintf((char *) query,
            "INSERT INTO sensor_channel (uuid, title, register, deviceUuid, measureTypeUuid, createdAt) value('%s', '%s', '%d', '%s', '%s', FROM_UNIXTIME(%ld))",
            uuid, channelTitle, sensorIndex, deviceUuid, channelTypeUuid, createTime);
    if (DEBUG) {
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s\n", TAG, query);
    }

    res = mtmZigbeeDBase->sqlexec((const char *) query);
    if (res) {
        mysql_free_result(res);
    }

    return mtmZigbeeDBase->isError();
}

bool findDevice(uint8_t *addr, uint8_t *uuid) {
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

void log_buffer_hex(uint8_t *buffer, size_t buffer_size) {
    uint8_t message[1024];
    for (int i = 0; i < buffer_size; i++) {
        sprintf((char *) &message[i * 6], "0x%02x, ", buffer[i]);
    }

    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s\n", TAG, message);
}

