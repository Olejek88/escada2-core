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
#include <iostream>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/value.h>
#include "LightFlags.h"

int coordinatorFd;
bool mtmZigbeeStarted = false;
uint8_t TAG[] = "mtmzigbee";
pthread_mutex_t mtmZigbeeStopMutex;
bool mtmZigbeeStopIssued;
DBase *mtmZigbeeDBase;
int32_t mtmZigBeeThreadId;
Kernel *kernel;
bool isSunInit;
bool isSunSet, isTwilightEnd, isTwilightStart, isSunRise;
std::map<std::string, LightFlags> lightFlags;
std::string coordinatorUuid;
bool isCheckCoordinatorRespond = true;

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
    coordinatorUuid.assign(tInfo->device_uuid, 36);

    if (!mtmZigbeeStarted) {
        isSunInit = false;
        isSunSet = false;
        isTwilightEnd = false;
        isTwilightStart = false;
        isSunRise = false;

        mtmZigbeeStarted = true;
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] device thread started", TAG);
        if (mtmZigbeeInit(MTM_ZIGBEE_COM_PORT, port, speed) == 0) {
            // запускаем цикл разбора пакетов
            mtmZigbeePktListener(mtmZigbeeDBase, mtmZigBeeThreadId);
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


void mtmZigbeePktListener(DBase *dBase, int32_t threadId) {
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
    time_t currentTime, heartBeatTime = 0, syncTimeTime = 0, checkSensorTime = 0, checkAstroTime = 0, checkOutPacket = 0, checkCoordinatorTime = 0;
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
#ifdef DEBUG
//                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] found SOF", TAG);
#endif
            } else if (!isFrameLen) {
                isFrameLen = true;
                seek[i++] = frameLen = data;
#ifdef DEBUG
//                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] found frame len", TAG);
#endif
            } else if (!isCommand) {
                commandByteCount++;
                seek[i++] = data;
                if (commandByteCount == 2) {
                    commandByteCount = 0;
                    isCommand = true;
#ifdef DEBUG
//                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] found command", TAG);
#endif
                }
            } else if (!isFrameData && frameDataByteCount < frameLen) {
                seek[i++] = data;
                frameDataByteCount++;
                if (frameDataByteCount == frameLen) {
                    isFrameData = true;
                    frameDataByteCount = 0;
#ifdef DEBUG
//                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] found frame data", TAG);
#endif
                }
            } else {
                // нашли контрольную сумму
                seek[i++] = data;
#ifdef DEBUG
//                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] found FCS", TAG);
#endif

                // пакет вроде как разобран
                // нужно проверить контрольную сумму фрейма
                fcs = compute_fcs(seek, i);
                if (fcs == seek[i - 1]) {
#ifdef DEBUG
//                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] frame good", TAG);
#endif

                    // складываем полученный пакет в список
                    zb_item = (struct zb_pkt_item *) malloc(sizeof(struct zb_pkt_item));
                    zb_item->len = i;
                    zb_item->pkt = malloc(zb_item->len);
                    memcpy(zb_item->pkt, seek, zb_item->len);
                    SLIST_INSERT_HEAD(&zb_queue_head, zb_item, items);
                } else {
#ifdef DEBUG
//                    kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] frame bad", TAG);
#endif
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
#ifdef DEBUG
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] processing zb packet...", TAG);
#endif

                zb_item = SLIST_FIRST(&zb_queue_head);
                mtmZigbeeProcessInPacket((uint8_t *) zb_item->pkt, zb_item->len);
                SLIST_REMOVE_HEAD(&zb_queue_head, items);
                free(zb_item->pkt);
                free(zb_item);
            }

            // проверяем, не отключили ли запуск потока, если да, остановить выполнение
            // обновляем значение c_time в таблице thread раз в 5 секунд
            currentTime = time(nullptr);
            if (currentTime - heartBeatTime >= 5) {
                heartBeatTime = currentTime;
                char query[512] = {0};
                MYSQL_RES *res;
                MYSQL_ROW row;
                my_ulonglong nRows;
                int isWork = 0;
                sprintf(query, "SELECT * FROM threads WHERE _id = %d", threadId);
                res = mtmZigbeeDBase->sqlexec(query);
                if (res) {
                    nRows = mysql_num_rows(res);
                    if (nRows == 1) {
                        mtmZigbeeDBase->makeFieldsList(res);
                        row = mysql_fetch_row(res);
                        if (row != nullptr) {
                            isWork = std::stoi(row[mtmZigbeeDBase->getFieldIndex("work")]);
                        } else {
                            // ошибка получения записи из базы, останавливаем поток
                            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] Read thread record get null", TAG);
                            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] Stopping thread", TAG);
                            mysql_free_result(res);
                            return;
                        }
                    } else {
                        // записи о потоке нет, либо их больше одной, останавливаем поток
                        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] Thread record not single, or not exists", TAG);
                        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] Stopping thread", TAG);
                        mysql_free_result(res);
                        return;
                    }

                    mysql_free_result(res);

                    if (isWork == 1) {
                        // обновляем статус
                        UpdateThreads(*mtmZigbeeDBase, threadId, 0, 1, nullptr);
                    } else {
                        // поток "остановили"
                        sprintf(query, "UPDATE threads SET status=%d, changedAt=FROM_UNIXTIME(%lu) WHERE _id=%d", 0,
                                currentTime, threadId);
                        res = mtmZigbeeDBase->sqlexec(query);
                        mysql_free_result(res);
                        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] Thread stopped from GUI", TAG);
                        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] Stopping thread", TAG);
                        return;
                    }
                }
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
                ssize_t rc = send_mtm_cmd(coordinatorFd, 0xFFFF, &current_time, kernel);
                if (rc == -1) {
                    kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
                    // останавливаем поток с целью его последующего автоматического запуска и инициализации
                    mtmZigbeeStopThread(mtmZigbeeDBase, threadId);
                    AddDeviceRegister(*mtmZigbeeDBase, (char *) coordinatorUuid.data(),
                                      (char *) "Ошибка записи в порт координатора");
                    return;
                }
#ifdef DEBUG
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.", TAG, rc);
#endif
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
                ssize_t rc = send_zb_cmd(coordinatorFd, AF_DATA_REQUEST, &req, kernel);
                if (rc == -1) {
                    kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
                    // останавливаем поток с целью его последующего автоматического запуска и инициализации
                    mtmZigbeeStopThread(mtmZigbeeDBase, threadId);
                    AddDeviceRegister(*mtmZigbeeDBase, (char *) coordinatorUuid.data(),
                                      (char *) "Ошибка записи в порт координатора");
                    return;
                }
#ifdef DEBUG
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
#endif
            }

            // получаем версию модуля, по полученному ответу понимаем что модуль работает
            currentTime = time(nullptr);
            if (currentTime - checkCoordinatorTime >= 15) {
                if (!isCheckCoordinatorRespond) {
                    // координатор не ответил
                    kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR Coordinator not answer for rquest module version",
                                      TAG);
                    // останавливаем поток с целью его последующего автоматического запуска и инициализации
                    mtmZigbeeStopThread(mtmZigbeeDBase, threadId);
                    AddDeviceRegister(*mtmZigbeeDBase, (char *) coordinatorUuid.data(),
                                      (char *) "Координатор не ответил на запрос");
                    return;
                }

                // сбрасываем флаг полученного ответа от координатора
                isCheckCoordinatorRespond = false;
//                uint8_t buff[] = {
//                        0xFE, 0x16, 0x48, 0x81, 0x00, 0x01, 0xE8, 0x00,
//                        0xFF, 0xFF, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02,
//                        0x01, 0x00, 0x00, 0x00, 0x05, 0x01, 0x02, 0x03,
//                        0x04, 0x05, 0x32
//                };
//                send_cmd(coordinatorFd, buff, sizeof(buff), kernel);

                checkCoordinatorTime = currentTime;
                zigbee_mt_cmd_af_data_request req = {0};
                req.dst_addr = 0x0000;
                req.sep = 0xE8;
                req.dep = 0xE8;
                req.cid = 0x0100;
                ssize_t rc = send_zb_cmd(coordinatorFd, AF_DATA_REQUEST, &req, kernel);
                if (rc == -1) {
                    kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
                    // останавливаем поток с целью его последующего автоматического запуска и инициализации
                    mtmZigbeeStopThread(mtmZigbeeDBase, threadId);
                    AddDeviceRegister(*mtmZigbeeDBase, (char *) coordinatorUuid.data(),
                                      (char *) "Ошибка записи в порт координатора");
                    return;
                }
#ifdef DEBUG
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
#endif
            }

            // проверка на наступление астрономических событий
            currentTime = time(nullptr);
            if (currentTime - checkAstroTime > 60) {
                double lon = 0, lat = 0;
                checkAstroTime = currentTime;
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

                // управление контактором, рассылка пакетов светильникам
                checkAstroEvents(currentTime, lon, lat, dBase, threadId);

                // рассылка пакетов светильникам по параметрам заданным в программах
                checkLightProgram(mtmZigbeeDBase, currentTime, lon, lat, threadId);
            }

            currentTime = time(nullptr);
            if (currentTime - checkOutPacket > 2) {
                checkOutPacket = currentTime;
                mtmZigbeeProcessOutPacket(threadId);
            }

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
    ssize_t rc = send_mtm_cmd(coordinatorFd, 0xFFFF, &action, kernel);
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
    ssize_t rc = send_zb_cmd(coordinatorFd, AF_DATA_REQUEST, &request, kernel);
    return rc;
}

ssize_t resetCoordinator() {
    int dtrFlag = TIOCM_DTR;
    ioctl(coordinatorFd, TIOCMBIS, &dtrFlag);
    ioctl(coordinatorFd, TIOCMBIC, &dtrFlag);
    sleep(2);
    return 1;
}

void mtmZigbeeProcessOutPacket(int32_t threadId) {
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
#ifdef DEBUG
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s", TAG, query);
#endif

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
#ifdef DEBUG
                kernel->log.ulogw(LOG_LEVEL_INFO, "Addr: %s, ", tmpAddr);
#endif

                dstAddr = strtoull((char *) tmpAddr, nullptr, 16);

                fieldIdx = mtmZigbeeDBase->getFieldIndex("data");
                flen = lengths[fieldIdx];
                memset(tmpData, 0, 1024);
                strncpy((char *) tmpData, row[fieldIdx], flen);
#ifdef DEBUG
                kernel->log.ulogw(LOG_LEVEL_INFO, "Data: %s", tmpData);
#endif

                struct base64_decode_ctx b64_ctx = {};
                size_t decoded = 512;
                base64_decode_init(&b64_ctx);
                if (base64_decode_update(&b64_ctx, &decoded, mtmPkt, flen, tmpData)) {
                    if (base64_decode_final(&b64_ctx)) {
                        uint8_t pktType = mtmPkt[0];
                        switch (pktType) {
                            case MTM_CMD_TYPE_CONFIG:
#ifdef DEBUG
                                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_CONFIG", TAG);
#endif

                                mtm_cmd_config config;
                                config.header.type = mtmPkt[0];
                                config.header.protoVersion = mtmPkt[1];
                                config.device = mtmPkt[2];
                                config.min = *(uint16_t *) &mtmPkt[3];
                                config.max = *(uint16_t *) &mtmPkt[5];

                                if (dstAddr == 0xffff) {
                                    rc = send_mtm_cmd(coordinatorFd, dstAddr, &config, kernel);
                                } else {
                                    rc = send_mtm_cmd_ext(coordinatorFd, dstAddr, &config, kernel);
                                }

#ifdef DEBUG
                                log_buffer_hex(mtmPkt, decoded);
                                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.", TAG, rc);
#endif

                                break;
                            case MTM_CMD_TYPE_CONFIG_LIGHT:
#ifdef DEBUG
                                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_CONFIG_LIGHT", TAG);
#endif

                                mtm_cmd_config_light config_light;
                                config_light.header.type = mtmPkt[0];
                                config_light.header.protoVersion = mtmPkt[1];
                                config_light.device = mtmPkt[2];
                                for (int nCfg = 0; nCfg < MAX_LIGHT_CONFIG; nCfg++) {
                                    config_light.config[nCfg].time = *(uint16_t *) &mtmPkt[3 + nCfg * 4];
                                    config_light.config[nCfg].value = *(uint16_t *) &mtmPkt[3 + nCfg * 4 + 2];
                                }

                                if (dstAddr == 0xFFFF) {
                                    rc = send_mtm_cmd(coordinatorFd, dstAddr, &config_light, kernel);
                                } else {
                                    rc = send_mtm_cmd_ext(coordinatorFd, dstAddr, &config_light, kernel);
                                }

#ifdef DEBUG
                                log_buffer_hex(mtmPkt, decoded);
                                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.", TAG, rc);
#endif

                                break;
                            case MTM_CMD_TYPE_CURRENT_TIME:
#ifdef DEBUG
                                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_CURRENT_TIME", TAG);
#endif

                                mtm_cmd_current_time current_time;
                                current_time.header.type = mtmPkt[0];
                                current_time.header.protoVersion = mtmPkt[1];
                                current_time.time = *(uint16_t *) &mtmPkt[2];

                                if (dstAddr == 0xFFFF) {
                                    rc = send_mtm_cmd(coordinatorFd, dstAddr, &current_time, kernel);
                                } else {
                                    rc = send_mtm_cmd_ext(coordinatorFd, dstAddr, &current_time, kernel);
                                }

#ifdef DEBUG
                                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.", TAG, rc);
                                log_buffer_hex(mtmPkt, decoded);
#endif

                                break;
                            case MTM_CMD_TYPE_ACTION:
#ifdef DEBUG
                                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_ACTION", TAG);
#endif

                                mtm_cmd_action action;
                                action.header.type = mtmPkt[0];
                                action.header.protoVersion = mtmPkt[1];
                                action.device = mtmPkt[2];
                                action.data = *(uint16_t *) &mtmPkt[3];

                                if (dstAddr == 0xFFFF) {
                                    rc = send_mtm_cmd(coordinatorFd, dstAddr, &action, kernel);
                                } else {
                                    rc = send_mtm_cmd_ext(coordinatorFd, dstAddr, &action, kernel);
                                }

#ifdef DEBUG
                                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.", TAG, rc);
                                log_buffer_hex(mtmPkt, decoded);
#endif

                                break;
                            case MTM_CMD_TYPE_CONTACTOR:
#ifdef DEBUG
                                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_CONTACTOR", TAG);
                                log_buffer_hex(mtmPkt, decoded);
#endif

                                rc = switchContactor(mtmPkt[3], mtmPkt[2]);
                                break;
                            case MTM_CMD_TYPE_RESET_COORDINATOR:
#ifdef DEBUG
                                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_RESET_COORDINATOR", TAG);
                                log_buffer_hex(mtmPkt, decoded);
#endif

                                rc = resetCoordinator();
                                // останавливаем поток с целью его последующего автоматического запуска и инициализации
                                mtmZigbeeStopThread(mtmZigbeeDBase, threadId);
                                AddDeviceRegister(*mtmZigbeeDBase, (char *) coordinatorUuid.data(),
                                                  (char *) "Получена команда сброса координатора");
                                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] Thread stopped by reset coordinator command",
                                                  TAG);
                                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] Stopping thread", TAG);
                                break;
                            default:
                                rc = 0;
                                break;
                        }

                        if (rc > 0) {
                            sprintf((char *) query, "UPDATE light_message SET dateOut=FROM_UNIXTIME(%ld) WHERE _id=%ld",
                                    time(nullptr), strtoul(row[0], nullptr, 10));
                            MYSQL_RES *updRes = mtmZigbeeDBase->sqlexec((char *) query);
                            if (updRes) {
                                mysql_free_result(updRes);
                            }
                        } else if (rc == -1) {
                            // ошибка записи в порт, останавливаем поток
                            // останавливаем поток с целью его последующего автоматического запуска и инициализации
                            mtmZigbeeStopThread(mtmZigbeeDBase, threadId);
                            AddDeviceRegister(*mtmZigbeeDBase, (char *) coordinatorUuid.data(),
                                              (char *) "Ошибка записи в порт координатора");
                            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] Thread stopped by error write to port", TAG);
                            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] Stopping thread", TAG);
                        }
                    }
                }
            }
        }

        mysql_free_result(res);
    }
}

void mtmZigbeeProcessInPacket(uint8_t *pktBuff, uint32_t length) {
    uint16_t cmd = *(uint16_t *) (&pktBuff[2]);
    uint8_t pktType;
    uint8_t dstEndPoint;
    uint16_t cluster;
    uint8_t address[32];
    uint8_t mtmLightStatusPktSize;

#ifdef DEBUG
    uint8_t query[1024];
    MYSQL_RES *res;
    time_t createTime = time(nullptr);
#endif

#ifdef DEBUG
    char pktStr[2048] = {0};
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] RAW in packet", TAG);
    for (int i = 0; i < length; i++) {
        sprintf(&pktStr[i * 2], "%02X", pktBuff[i]);
    }

    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s", TAG, pktStr);
#endif

    uint8_t resultBuff[1024] = {};
    struct base64_encode_ctx b64_ctx = {};
    int encoded_bytes;

    switch (cmd) {
        case AF_DATA_RESPONSE:
#ifdef DEBUG
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] AF_DATA_RESPONSE", TAG);
#endif

            dstEndPoint = pktBuff[6];
            cluster = *(uint16_t *) (&pktBuff[4]);
            if (dstEndPoint != MBEE_API_END_POINT) {
                return;
            }

            switch (cluster) { // NOLINT
                case MBEE_API_LOCAL_IOSTATUS_CLUSTER :
                    // состояние линий модуля zigbee
                    memset(address, 0, 32);
                    sprintf((char *) address, "%02X%02X%02X%02X%02X%02X%02X%02X",
                            pktBuff[17], pktBuff[16], pktBuff[15], pktBuff[14],
                            pktBuff[13], pktBuff[12], pktBuff[11], pktBuff[10]);

#ifdef DEBUG
                    kernel->log.ulogw(LOG_LEVEL_INFO, "Get sensor data packet");
#endif

                    makeCoordinatorStatus(mtmZigbeeDBase, address, pktBuff);
                    break;

                case MBEE_API_LOCAL_MODULE_VERSION_CLUSTER :
                    // ни чего не проверяем, ответ получен, значит координатор работает
                    isCheckCoordinatorRespond = true;
#ifdef DEBUG
                    kernel->log.ulogw(LOG_LEVEL_INFO, "Get module version packet");
#endif
                    break;
                default:
                    break;
            }

            break;

        case AF_INCOMING_MSG:
#ifdef DEBUG
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] AF_INCOMING_MSG", TAG);
#endif

            dstEndPoint = pktBuff[11];
            cluster = *(uint16_t *) (&pktBuff[6]);
            if (dstEndPoint != MTM_API_END_POINT || cluster != MTM_API_CLUSTER) {
                break;
            }

            pktType = pktBuff[21];
            switch (pktType) {
                case MTM_CMD_TYPE_STATUS:
                    // размер полезных данных в zb пакете
                    mtmLightStatusPktSize = pktBuff[20];
#ifdef DEBUG
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] MTM_CMD_TYPE_STATUS", TAG);
                    log_buffer_hex(&pktBuff[21], mtmLightStatusPktSize);
#endif

                    memset(address, 0, 32);
                    sprintf((char *) address, "%02X%02X%02X%02X%02X%02X%02X%02X",
                            pktBuff[30], pktBuff[29], pktBuff[28], pktBuff[27],
                            pktBuff[26], pktBuff[25], pktBuff[24], pktBuff[23]);

                    base64_encode_init(&b64_ctx);
#ifdef __APPLE__
                encoded_bytes = base64_encode_update(&b64_ctx, (char *) resultBuff, get_mtm_command_size(pktType),
                                                     reinterpret_cast<const uint8_t *>((size_t) &pktBuff[21]));
                base64_encode_final(&b64_ctx, reinterpret_cast<char *>(resultBuff + encoded_bytes));
#elif __USE_GNU
                    encoded_bytes = base64_encode_update(&b64_ctx, resultBuff, mtmLightStatusPktSize, &pktBuff[21]);
                    base64_encode_final(&b64_ctx, resultBuff + encoded_bytes);
#endif

#ifdef DEBUG
                    sprintf((char *) query,
                            "INSERT INTO light_answer (address, data, createdAt, changedAt, dateIn) value('%s', '%s', FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld))",
                            address, resultBuff, createTime, createTime, createTime);
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s", TAG, query);
                    res = mtmZigbeeDBase->sqlexec((const char *) query);
                    if (res) {
                        mysql_free_result(res);
                    }
#endif
                    // из размера пакета вычитаем два байта заголовка, восемь байт адреса, два байта флагов аварии
                    for (uint8_t statusIdx = 0; statusIdx < (mtmLightStatusPktSize - 12) / 2; statusIdx++) {
                        switch (statusIdx) {
                            case MTM_DEVICE_LIGHT :
                                makeLightStatus(mtmZigbeeDBase, address, pktBuff);
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
#ifdef DEBUG
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] AF_INCOMING_MSG_EXT", TAG);
#endif

            break;

        default:
            break;
    }
}

int32_t mtmZigbeeInit(int32_t mode, uint8_t *path, uint32_t speed) {
    struct termios serialPortSettings{};
    ssize_t rc;

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
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] FIFO can not make!!!", TAG);
                return -1;
            } else {
                close(coordinatorFd);
                coordinatorFd = open((char *) fifo, O_NONBLOCK | O_RDWR | O_NOCTTY); // NOLINT(hicpp-signed-bitwise)
                if (coordinatorFd == -1) {
                    kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] FIFO can not open!!!", TAG);
                    return -1;
                }
            }
        }
    } else {
        if (access((char *) path, F_OK) != -1) {
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] init %s", TAG, path);
        } else {
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s path not exists", TAG, path);
            return -2;
        }

        // открываем порт
        coordinatorFd = open((char *) path, O_NONBLOCK | O_RDWR | O_NOCTTY); // NOLINT(hicpp-signed-bitwise)
        if (coordinatorFd == -1) {
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] can not open file", TAG);
            return -3;
        }

        resetCoordinator();

        // инициализируем
        tcgetattr(coordinatorFd, &serialPortSettings);

        /* Setting the Baud rate */
        cfsetispeed(&serialPortSettings, mtmZigbeeGetSpeed(speed));
        cfsetospeed(&serialPortSettings, mtmZigbeeGetSpeed(speed));

        /* 8N1 Mode */
        serialPortSettings.c_cflag &= ~PARENB;   /* Disables the Parity Enable bit(PARENB),So No Parity   */ // NOLINT(hicpp-signed-bitwise)
        serialPortSettings.c_cflag &= ~HUPCL;    /* принудительно выключаем DTR при открытом порту, так как через него мы управляем сбросом модуля zigbee */ // NOLINT(hicpp-signed-bitwise)
        serialPortSettings.c_cflag &= ~CSTOPB;   /* CSTOPB = 2 Stop bits,here it is cleared so 1 Stop bit */ // NOLINT(hicpp-signed-bitwise)
        serialPortSettings.c_cflag &= ~CSIZE;    /* Clears the mask for setting the data size             */ // NOLINT(hicpp-signed-bitwise)
        serialPortSettings.c_cflag |= CS8; // NOLINT(hicpp-signed-bitwise)
        serialPortSettings.c_cflag &= ~CRTSCTS;
        serialPortSettings.c_cflag |= (CREAD | CLOCAL); // NOLINT(hicpp-signed-bitwise)

        serialPortSettings.c_iflag &= ~(IXON | IXOFF | IXANY); // NOLINT(hicpp-signed-bitwise)
        serialPortSettings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG | ECHONL);   // NOLINT(hicpp-signed-bitwise)
        serialPortSettings.c_oflag &= ~OPOST;  // NOLINT(hicpp-signed-bitwise)

        serialPortSettings.c_cc[VMIN] = 1;
        serialPortSettings.c_cc[VTIME] = 5;

        cfmakeraw(&serialPortSettings);

        if ((tcsetattr(coordinatorFd, TCSANOW, &serialPortSettings)) != 0) {
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR ! in Setting attributes", TAG);
            return -4;
        } else {
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] BaudRate = 38400\nStopBits = 1\nParity = none\n", TAG);
        }

        tcflush(coordinatorFd, TCIFLUSH);   /* Discards old data in the rx buffer            */
    }


    rc = send_zb_cmd(coordinatorFd, ZB_SYSTEM_RESET, nullptr, kernel);
    if (rc == -1) {
        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
        // останавливаем поток с целью его последующего автоматического запуска и инициализации
        mtmZigbeeStopThread(mtmZigbeeDBase, mtmZigBeeThreadId);
        AddDeviceRegister(*mtmZigbeeDBase, (char *) coordinatorUuid.data(),
                          (char *) "Ошибка записи в порт координатора");
        return -5;
    }

    sleep(1);

    // регистрируем свою конечную точку
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Register our end point.", TAG);
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
    rc = send_zb_cmd(coordinatorFd, AF_REGISTER, &af_register, kernel);
    if (rc == -1) {
        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
        // останавливаем поток с целью его последующего автоматического запуска и инициализации
        mtmZigbeeStopThread(mtmZigbeeDBase, mtmZigBeeThreadId);
        AddDeviceRegister(*mtmZigbeeDBase, (char *) coordinatorUuid.data(),
                          (char *) "Ошибка записи в порт координатора");
        return -5;
    }
#ifdef DEBUG
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld", TAG, rc);
#endif

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
//            0x01, 0x00, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0x01, 0x00, 0x42, 0x4D,
//            0x05

//            0xFE,
//            0x24, 0x44, 0x81,
//            0x00, 0x00, 0x00, 0xFC,
//            0x84, 0x1D, 0xE9, 0xE9, 0x00, 0x00, 0x00, 0x1F,
//            0x1D, 0x34, 0x00, 0x00, 0x10, 0x01, 0x00, 0x07,
//            0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0x00,
//            0x00, 0x6E, 0x00, 0x50, 0x03, 0x84, 0x1D, 0x07,
//            0x00

//            0xFE, 0x2E, 0x48, 0x81, 0x03, 0x01, 0xE8, 0x00,
//            0xFF, 0xFF,
//            0x96, 0x97, 0xAD, 0x04, 0x00, 0x4B, 0x12, 0x00,
//            0x00, 0x00, 0x1D, 0x02, 0xF5, 0x1A,
//            0x01, 0x21, 0x00, 0x00, 0xFF, 0x01, 0xC0, 0x0F,
//            0xBE, 0x01, 0xFF, 0x07, 0x00, 0x00, 0x87, 0x07,
//            0xCC, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//            0x00, 0x00, 0x13
//    };
//    send_cmd(coordinatorFd, buff, sizeof(buff), kernel);

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

