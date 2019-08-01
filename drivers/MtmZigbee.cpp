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

int coordinatorFd;
bool mtmZigbeeStarted = false;
uint8_t TAG[] = "mtmzigbee";
pthread_mutex_t mtmZigbeeStopMutex;
bool mtmZigbeeStopIssued;
DBase *mtmZigbeeDBase;
int32_t mtmZigBeeThreadId;
Kernel *kernel;

void *mtmZigbeeDeviceThread(void *pth) {
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
    time_t currentTime, heartBeatTime = 0, syncTimeTime = 0;
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
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] found SOF\n", TAG);
            } else if (!isFrameLen) {
                isFrameLen = true;
                seek[i++] = frameLen = data;
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] found frame len\n", TAG);
            } else if (!isCommand) {
                commandByteCount++;
                seek[i++] = data;
                if (commandByteCount == 2) {
                    commandByteCount = 0;
                    isCommand = true;
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] found command\n", TAG);
                }
            } else if (!isFrameData && frameDataByteCount < frameLen) {
                seek[i++] = data;
                frameDataByteCount++;
                if (frameDataByteCount == frameLen) {
                    isFrameData = true;
                    frameDataByteCount = 0;
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] found frame data\n", TAG);
                }
            } else {
                // нашли контрольную сумму
                seek[i++] = data;
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] found FCS\n", TAG);

                // пакет вроде как разобран
                // нужно проверить контрольную сумму фрейма
                fcs = compute_fcs(seek, i);
                if (fcs == seek[i - 1]) {
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] frame good\n", TAG);
                    // складываем полученный пакет в список
                    zb_item = (struct zb_pkt_item *) malloc(sizeof(struct zb_pkt_item));
                    zb_item->len = i - 1;
                    zb_item->pkt = malloc(zb_item->len);
                    memcpy(zb_item->pkt, seek, zb_item->len);
                    SLIST_INSERT_HEAD(&zb_queue_head, zb_item, items);
                } else {
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] frame bad\n", TAG);
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
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] processing zb packet...\n", TAG);
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
                // TODO: отправить в лог дамп mtm пакета
//                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.\n", TAG, rc);
            }

            mtmZigbeeProcessOutPacket();

            run = mtmZigbeeGetRun();

            usleep(10000);
        }
    }

    mtmZigbeeDBase->disconnect();
}

void mtmZigbeeProcessOutPacket() {
    uint8_t query[1024];
    MYSQL_RES *res;
    MYSQL_ROW row;
    MYSQL_FIELD *field;
    int32_t fieldAddressIdx = -1;
    const char *fieldAddress = "address";
    int32_t fieldDataIdx = -1;
    const char *fieldData = "data";
    uint32_t nFields;
    u_long nRows;
    unsigned long *lengths;
    long flen;
    uint8_t tmpAddr[1024];
    uint8_t tmpData[1024];
    uint8_t mtmPkt[512];
    uint64_t dstAddr;
    ssize_t rc;

    sprintf((char *) query, "SELECT * FROM light_message WHERE dateOut IS NULL;");
//    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s\n", TAG,  query);
    res = mtmZigbeeDBase->sqlexec((char *) query);
    if (res) {
        nRows = mysql_num_rows(res);
        nFields = mysql_num_fields(res);
        char *headers[nFields];

        for (uint32_t i = 0; (field = mysql_fetch_field(res)); i++) {
            headers[i] = field->name;
            if (strcmp(fieldAddress, headers[i]) == 0) {
                fieldAddressIdx = i;
            } else if (strcmp(fieldData, headers[i]) == 0) {
                fieldDataIdx = i;
            }
        }

        for (uint32_t i = 0; i < nRows; i++) {
            row = mysql_fetch_row(res);
            lengths = mysql_fetch_lengths(res);
            if (row) {
                flen = lengths[fieldAddressIdx];
                memset(tmpAddr, 0, 1024);
                strncpy((char *) tmpAddr, row[fieldAddressIdx], flen);
//                printf("Addr: %s, ", tmpAddr);
                dstAddr = strtoull((char *) tmpAddr, nullptr, 16);

                flen = lengths[fieldDataIdx];
                memset(tmpData, 0, 1024);
                strncpy((char *) tmpData, row[fieldDataIdx], flen);
//                printf("Data: %s\n", tmpData);

                struct base64_decode_ctx b64_ctx = {};
                size_t decoded = 512;
                base64_decode_init(&b64_ctx);
                if (base64_decode_update(&b64_ctx, &decoded, mtmPkt, flen, tmpData)) {
                    if (base64_decode_final(&b64_ctx)) {
                        uint8_t pktType = mtmPkt[0];
                        switch (pktType) {
                            case MTM_CMD_TYPE_CONFIG:
                                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_CONFIG\n", TAG);
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

                                log_buffer_hex(mtmPkt, decoded);
                                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.\n", TAG, rc);
                                break;
                            case MTM_CMD_TYPE_CONFIG_LIGHT:
                                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_CONFIG_LIGHT\n", TAG);
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

                                log_buffer_hex(mtmPkt, decoded);
                                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.\n", TAG, rc);
                                break;
                            case MTM_CMD_TYPE_CURRENT_TIME:
                                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_CURRENT_TIME\n", TAG);
                                mtm_cmd_current_time current_time;
                                current_time.header.type = mtmPkt[0];
                                current_time.header.protoVersion = mtmPkt[1];
                                current_time.time = *(uint16_t *) &mtmPkt[2];

                                if (dstAddr == 0xFFFF) {
                                    rc = send_mtm_cmd(coordinatorFd, dstAddr, &current_time);
                                } else {
                                    rc = send_mtm_cmd_ext(coordinatorFd, dstAddr, &current_time);
                                }

                                log_buffer_hex(mtmPkt, decoded);
                                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.\n", TAG, rc);
                                break;
                            case MTM_CMD_TYPE_ACTION:
                                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_ACTION\n", TAG);
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

                                log_buffer_hex(mtmPkt, decoded);
                                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.\n", TAG, rc);
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
    uint8_t dstEndPoint = pktBuff[11];
    uint16_t cluster = *(uint16_t *) (&pktBuff[6]);

    uint8_t deviceUuid[64];
    uint8_t sChannelUuid[64];
    uuid_t newUuid;
    char newMeasureUuid[37] = {0};

//    printf("%s\n", resultBuff);
    uint8_t query[1024];
    uint8_t addr[32];
    MYSQL_RES *res;
    time_t createTime = time(nullptr);

    switch (cmd) {
        case AF_INCOMING_MSG:
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] AF_INCOMING_MSG\n", TAG);

            if (dstEndPoint != MTM_API_END_POINT || cluster != MTM_API_CLUSTER) {
                break;
            }

            pktType = pktBuff[21];
            switch (pktType) {
                case MTM_CMD_TYPE_STATUS:
                    int32_t sensorIndex;
                    len = len - 33;
                    // выводим содержимое пакета в лог
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] MTM_CMD_TYPE_STATUS\n", TAG);
                    log_buffer_hex(&pktBuff[21], len + 12);
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

                    sprintf((char *) query,
                            "INSERT INTO light_answer (address, data, createdAt, changedAt, dateIn) value('%s', '%s', FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld))",
                            addr, resultBuff, createTime, createTime, createTime);
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s\n", TAG, query);

                    res = mtmZigbeeDBase->sqlexec((const char *) query);
                    if (res) {
                        mysql_free_result(res);
                    }

                    for (uint8_t statusIdx = 0; statusIdx < len / 2; statusIdx++) {
                        switch (statusIdx) {
                            case 0 :
                                memset(deviceUuid, 0, 64);
                                if (findDevice(addr, deviceUuid)) {
                                    // найти по устройству sensor_channel и regIdx=0 (Температура светильника)
                                    sensorIndex = 0;
                                    memset(sChannelUuid, 0, 64);
                                    if (!findSChannel(deviceUuid, sensorIndex, sChannelUuid)) {
                                        // если нет, создать
                                        uuid_generate(newUuid);
                                        uuid_unparse_upper(newUuid, (char *) sChannelUuid);
                                        sprintf((char *) query,
                                                "INSERT INTO sensor_channel (uuid, title, register, deviceUuid, measureTypeUuid, createdAt) value('%s', 'Температура', '%d', '%s', '%s', FROM_UNIXTIME(%ld))",
                                                sChannelUuid, sensorIndex, deviceUuid, CHANNEL_T, createTime);
                                        kernel->log.ulogw(LOG_LEVEL_INFO, (char *) query, TAG);
                                        res = mtmZigbeeDBase->sqlexec((const char *) query);
                                        if (res) {
                                            mysql_free_result(res);
                                        }
                                    }

                                    // создать новое измерение
                                    uuid_generate(newUuid);
                                    memset(newMeasureUuid, 0, 37);
                                    uuid_unparse_upper(newUuid, newMeasureUuid);
                                    sprintf((char *) query,
                                            "INSERT INTO data (uuid, sensorChannelUuid, value, date, createdAt) value('%s', '%s', %d, FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld))",
                                            newMeasureUuid, sChannelUuid, pktBuff[34], createTime, createTime);
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s\n", TAG, query);
                                    res = mtmZigbeeDBase->sqlexec((const char *) query);
                                    if (res) {
                                        mysql_free_result(res);
                                    }

                                    // найти по устройству sensor_channel и regIdx=1 (Ток светильника)
                                    sensorIndex = 1;
                                    memset(sChannelUuid, 0, 64);
                                    if (!findSChannel(deviceUuid, sensorIndex, sChannelUuid)) {
                                        // если нет, создать
                                        uuid_generate(newUuid);
                                        uuid_unparse_upper(newUuid, (char *) sChannelUuid);
                                        sprintf((char *) query,
                                                "INSERT INTO sensor_channel (uuid, title, register, deviceUuid, measureTypeUuid, createdAt) value('%s', 'Мощность', '%d', '%s', '%s', FROM_UNIXTIME(%ld))",
                                                sChannelUuid, sensorIndex, deviceUuid, CHANNEL_W, createTime);
                                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s\n", TAG, query);
                                        res = mtmZigbeeDBase->sqlexec((const char *) query);
                                        if (res) {
                                            mysql_free_result(res);
                                        }
                                    }

                                    // создать новое измерение
                                    uuid_generate(newUuid);
                                    memset(newMeasureUuid, 0, 37);
                                    uuid_unparse_upper(newUuid, newMeasureUuid);
                                    sprintf((char *) query,
                                            "INSERT INTO data (uuid, sensorChannelUuid, value, date, createdAt) value('%s', '%s', %d, FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld))",
                                            newMeasureUuid, sChannelUuid, pktBuff[33], createTime, createTime);
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s\n", TAG, query);
                                    res = mtmZigbeeDBase->sqlexec((const char *) query);
                                    if (res) {
                                        mysql_free_result(res);
                                    }

                                    // найти по устройству sensor_channel и regIdx=2 (Состояние светильника)
                                    sensorIndex = 2;
                                    memset(sChannelUuid, 0, 64);
                                    if (!findSChannel(deviceUuid, sensorIndex, sChannelUuid)) {
                                        // если нет, создать
                                        uuid_generate(newUuid);
                                        uuid_unparse_upper(newUuid, (char *) sChannelUuid);
                                        sprintf((char *) query,
                                                "INSERT INTO sensor_channel (uuid, title, register, deviceUuid, measureTypeUuid, createdAt) value('%s', 'Авария', '%d', '%s', '%s', FROM_UNIXTIME(%ld))",
                                                sChannelUuid, sensorIndex, deviceUuid, CHANNEL_STATUS, createTime);
                                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s\n", TAG, query);
                                        res = mtmZigbeeDBase->sqlexec((const char *) query);
                                        if (res) {
                                            mysql_free_result(res);
                                        }
                                    }

                                    // создать новое измерение
                                    uuid_generate(newUuid);
                                    memset(newMeasureUuid, 0, 37);
                                    uuid_unparse_upper(newUuid, newMeasureUuid);
                                    uint16_t alerts = *(uint16_t *) &pktBuff[31];
                                    uint8_t alert = alerts & 0x0001u;
                                    sprintf((char *) query,
                                            "INSERT INTO data (uuid, sensorChannelUuid, value, date, createdAt) value('%s', '%s', %d, FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld))",
                                            newMeasureUuid, sChannelUuid, alert, createTime, createTime);
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s\n", TAG, query);
                                    res = mtmZigbeeDBase->sqlexec((const char *) query);
                                    if (res) {
                                        mysql_free_result(res);
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
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] AF_INCOMING_MSG_EXT\n", TAG);
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
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] FIFO can not make!!!\n", TAG);
                return -1;
            } else {
                close(coordinatorFd);
                coordinatorFd = open((char *) fifo, O_NONBLOCK | O_RDWR | O_NOCTTY); // NOLINT(hicpp-signed-bitwise)
                if (coordinatorFd == -1) {
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] FIFO can not open!!!\n", TAG);
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
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] can not open file\n", TAG);
            return -3;
        }

        // инициализируем
        tcgetattr(coordinatorFd, &serialPortSettings);

        /* Setting the Baud rate */
        cfsetispeed(&serialPortSettings, mtmZigbeeGetSpeed(speed));
        cfsetospeed(&serialPortSettings, mtmZigbeeGetSpeed(speed));

        /* 8N1 Mode */
        serialPortSettings.c_cflag &= ~PARENB;   /* Disables the Parity Enable bit(PARENB),So No Parity   */ // NOLINT(hicpp-signed-bitwise)
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
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] ERROR ! in Setting attributes\n", TAG);
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
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] rc=%ld\n", TAG, rc);

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
            } else {
                return false;
            }
        } else {
            return false;
        }
    } else {
        return false;
    }

    return true;
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
            } else {
                return false;
            }
        } else {
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

