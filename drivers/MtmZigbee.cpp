#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <nettle/base64.h>
#include <zigbeemtm.h>
#include "dbase.h"
#include "TypeThread.h"
#include "MtmZigbee.h"
#include "kernel.h"
#include "function.h"
#include <uuid/uuid.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include "suninfo.h"
#include <stdlib.h>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/value.h>
#include <main.h>
#include "lightUtils.h"
#include "ce102.h"
#include "e18.h"

int coordinatorFd;
bool mtmZigbeeStarted = false;
uint8_t *TAG = (uint8_t *) "mtmzigbee";
pthread_mutex_t mtmZigbeeStopMutex;
bool mtmZigbeeStopIssued;
DBase *mtmZigbeeDBase;
int32_t mtmZigBeeThreadId;
Kernel *kernel;
bool isSunInit;
bool isSunSet, isTwilightEnd, isTwilightStart, isSunRise;
std::string coordinatorUuid;
bool isCheckCoordinatorRespond;
e18_cmd_queue e18_cmd_queue_head = {nullptr};

void *mtmZigbeeDeviceThread(void *pth) { // NOLINT
    uint64_t speed;
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
        isCheckCoordinatorRespond = true;

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
    uint8_t data;
    uint8_t seek[1024];
    bool isNetwork = false;
    bool isCmdRun = false;
    e18_cmd_item currentCmd = {nullptr};
    currentCmd.data = malloc(128);
    time_t currentTime, heartBeatTime = 0, syncTimeTime = 0, checkDoorSensorTime = 0, checkContactorSensorTime = 0,
            checkRelaySensorTime = 0, checkAstroTime = 0, checkOutPacket = 0, checkCoordinatorTime = 0,
            checkLinkState = 0, checkShortAddresses = 0;
    struct tm *localTime;
    uint16_t addr;
    uint8_t inState, outState;

    struct zb_pkt_item {
        void *pkt;
        uint32_t len;
        SLIST_ENTRY(zb_pkt_item) items;
    };

    SLIST_HEAD(zb_queue, zb_pkt_item)
            zb_queue_head = SLIST_HEAD_INITIALIZER(zb_queue_head);
    SLIST_INIT(&zb_queue_head);

    struct zb_pkt_item *zb_item;

    mtmZigbeeSetRun(true);

    while (run) {
        count = read(coordinatorFd, &data, 1);
        if (count > 0) {
            printf("data: %02X\n", data);

            if (isCmdRun && data == E18_GET_ANSWER) {
                // ответ на команду получения данных
                printf("get read data of %02X command\n", currentCmd.cmd);
                uint8_t readDataLen;

                switch (currentCmd.cmd) {
                    case E18_HEX_CMD_GET_NETWORK_STATE :
                    case E18_HEX_CMD_GET_UART_BAUD_RATE :
                        readDataLen = 1;
                        break;
                    case E18_HEX_CMD_GET_GPIO_LEVEL:
                        readDataLen = 5;
                        break;
                    case E18_HEX_CMD_GET_REMOTE_SHORT_ADDR:
                        readDataLen = 2;
                        break;
                    default:
                        readDataLen = 0;
                        break;
                }

                // читаем данные ответа
                if (e18_read_fixed_data(coordinatorFd, seek, readDataLen) < 0) {
                    // ошибка чтения данных, нужно остановить поток
                    lostZBCoordinator(dBase, threadId, &coordinatorUuid);
                    return;
                }

                // обрабатываем при необходимости полученные данные
                switch (currentCmd.cmd) {
                    case E18_HEX_CMD_GET_NETWORK_STATE :
                        isNetwork = seek[0] == 1;
                        printf("get network state is %s\n", isNetwork ? "TRUE" : "FALSE");
                        break;
                    case E18_HEX_CMD_GET_UART_BAUD_RATE :
                        isCheckCoordinatorRespond = true;
                        break;
                    case E18_HEX_CMD_GET_GPIO_LEVEL :
                        addr = *(uint16_t *) &seek[1];
                        inState = seek[3];
                        outState = seek[4];
                        printf("Addr: 0x%04X, In: %d, Out: %d\n", addr, inState, outState);
                        switch (currentCmd.extra[0]) {
                            case E18_PIN_DOOR :
                                printf("door status: in=%d, out=%d\n", inState, outState);
                                storeCoordinatorDoorStatus(dBase, &coordinatorUuid, inState == 1, outState == 1);
                                break;
                            case E18_PIN_CONTACTOR :
                                printf("contactor status: in=%d, out=%d\n", inState, outState);
                                storeCoordinatorContactorStatus(dBase, &coordinatorUuid, inState == 1, outState == 1);
                                break;
                            case E18_PIN_RELAY :
                                printf("relay status: in=%d, out=%d\n", inState, outState);
                                storeCoordinatorRelayStatus(dBase, &coordinatorUuid, inState == 1, outState == 1);
                                break;
                            default:
                                break;
                        }

                        break;
                    case E18_HEX_CMD_GET_REMOTE_SHORT_ADDR :
                        // сохраняем/обновляем запись с коротким адресом
                        e18_store_short_address(dBase, currentCmd.extra, *(uint16_t *) seek, kernel);
                        break;
                    default:
                        break;
                }

                isCmdRun = false;
            } else if (isCmdRun && data == E18_SET_ANSWER) {
                // ответ на команду установки данных
                printf("get write data of %02X command\n", currentCmd.cmd);
                uint8_t readDataLen;
                switch (currentCmd.cmd) {
                    case E18_HEX_CMD_SET_DEVICE_TYPE:
                    case E18_HEX_CMD_SET_PANID:
                    case E18_HEX_CMD_SET_NETWORK_KEY:
                    case E18_HEX_CMD_SET_NETWORK_GROUP:
                    case E18_HEX_CMD_SET_NETWORK_CHANNEL:
                    case E18_HEX_CMD_SET_TX_POWER:
                    case E18_HEX_CMD_SET_UART_BAUD_RATE:
                    case E18_HEX_CMD_SET_SLEEP_STATE:
                    case E18_HEX_CMD_SET_RETENTION_TIME:
                    case E18_HEX_CMD_SET_JOIN_PERIOD:
                    case E18_HEX_CMD_SET_ALL_DEVICE_INFO:
                    case E18_HEX_CMD_DEVICE_RESTART:
                    case E18_HEX_CMD_RECOVER_FACTORY:
                    case E18_HEX_CMD_OFF_NETWORK_AND_RESTART:
                        readDataLen = 1;
                        break;
                    case E18_HEX_CMD_SET_GPIO_IO_STATUS:
                    case E18_HEX_CMD_SET_GPIO_LEVEL:
                    case E18_HEX_CMD_SET_PWM_STATUS:
                        readDataLen = 3;
                        break;
                    default:
                        readDataLen = 0;
                        break;
                }

                // читаем данные ответа
                if (e18_read_fixed_data(coordinatorFd, seek, readDataLen) < 0) {
                    // ошибка чтения данных, нужно остановить поток
                    lostZBCoordinator(dBase, threadId, &coordinatorUuid);
                    return;
                }

                if (seek[0] != currentCmd.cmd) {
                    printf("write answer do not match! received %02X\n", seek[1]);
                }

                switch (currentCmd.cmd) {
                    case E18_HEX_CMD_SET_GPIO_LEVEL :
                        if (currentCmd.extra[0] == E18_PIN_RELAY) {
                            // даём задержку для того чтоб стартанули модули в светильниках
                            // т.к. неизвестно, питаются они через контактор или всё время под напряжением
                            // задержка будет и при выключении реле
                            sleep(5);
                        }
                        break;
                    default:
                        break;
                }

                isCmdRun = false;
            } else if (isCmdRun && data == E18_ERROR) {
                // ошибка
                // читаем данные ответа
                if (e18_read_fixed_data(coordinatorFd, seek, 1) < 0) {
                    // ошибка чтения данных, нужно остановить поток
                    lostZBCoordinator(dBase, threadId, &coordinatorUuid);
                    return;
                }

                // возможно нужно выставить флаг ошибки команды,
                // не снимать флаг выполнения команды, чтобы можно было обработать данную ситуацию
                if (seek[0] == E18_ERROR_SYNTAX) {
                    if (currentCmd.cmd == E18_HEX_CMD_GET_REMOTE_SHORT_ADDR) {
                        // это значит что светильник с указанным MAC сейчас либо выключен, либо не доступен
                        printf("Unable get short address. Maybe zigbee module offline.\n");
                    } else {
                        printf("error syntax of %02X command\n", currentCmd.cmd);
                    }
                } else {
                    printf("unknown error of %02X command\n", currentCmd.cmd);
                }

                isCmdRun = false;
            } else if (data == E18_NETWORK_STATE) {
                // состояние сети
                // читаем данные ответа
                if (e18_read_fixed_data(coordinatorFd, seek, 1) < 0) {
                    // ошибка чтения данных, нужно остановить поток
                    lostZBCoordinator(dBase, threadId, &coordinatorUuid);
                    return;
                }

                if (seek[0] == E18_NETWORK_STATE_UP) {
                    // координатор запустил сеть
                    printf("network up\n");
                    isNetwork = true;
                } else if (seek[0] == E18_NETWORK_STATE_JOIN) {
                    // устройство подключилось к сети
                    printf("join to network\n");
                    isNetwork = true;
                } else if (seek[0] == E18_NETWORK_STATE_LOST) {
                    // нет сети
                    printf("network lost\n");
                    isNetwork = false;
                } else {
                    printf("unknown network state\n");
                }
            } else if (data == E18_SOF) {
                printf("SOF\n");
                // начало фрейма
                // читаем данные до символа конца фрейма или максимум 255(?) байт.
                bool isComplete = false;
                bool isEsc = false;
                uint8_t i = 0;
                time_t startReadTime = time(nullptr);

                while (true) {
                    count = read(coordinatorFd, &data, 1);
                    if (count > 0) {
                        printf("frame data: %02X\n", data);

                        if (data == E18_EOF) {
                            printf("EOF\n");
                            isComplete = true;
                            break;
                        }

                        if (data == E18_ESC) {
                            printf("ESC\n");
                            isEsc = true;
                            continue;
                        }

                        if (isEsc) {
                            data = data ^ 0x20u;
                            printf("un ESC %02X\n", data);
                            isEsc = false;
                        }

                        seek[i++] = data;
                    }

                    if (i == 255) {
                        // что-то пошло не так
                        printf("Error: size frame limit reached!!!\n");
                        break;
                    }

                    if (time(nullptr) - startReadTime > 5) {
                        // в течении 5 секунд не смогли прочитать все данные, останавливаем поток
                        lostZBCoordinator(dBase, threadId, &coordinatorUuid);
                        return;
                    }

                    usleep(1000);
                }

                // чтение данных закончили, либо штатно, либо с ошибкой
                if (isComplete) {
                    printf("frame received, len = %i\n", i);

                    // пакет вроде как разобран
                    // складываем полученный пакет в список
                    zb_item = (struct zb_pkt_item *) malloc(sizeof(struct zb_pkt_item));
                    zb_item->len = i;
                    zb_item->pkt = malloc(zb_item->len);
                    memcpy(zb_item->pkt, (const void *) seek, zb_item->len);
                    SLIST_INSERT_HEAD(&zb_queue_head, zb_item, items);
                } else {
                    printf("bad frame, not found EOF\n");
                }
            }
        } else {
            // есть свободное время, разбираем список полученных пакетов
            while (!SLIST_EMPTY(&zb_queue_head)) {
                if (kernel->isDebug) {
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] processing zb packet...", TAG);
                }

                zb_item = SLIST_FIRST(&zb_queue_head);
                mtmZigbeeProcessInPacket((uint8_t *) zb_item->pkt, zb_item->len);
                SLIST_REMOVE_HEAD(&zb_queue_head, items);
                free(zb_item->pkt);
                free(zb_item);
            }

            // если в очереди есть команда, отправляем её
            if (!isCmdRun) {
                if (!SLIST_EMPTY(&e18_cmd_queue_head)) {
                    e18_cmd_item *cmdItem = SLIST_FIRST(&e18_cmd_queue_head);
                    isCmdRun = true;

                    currentCmd.cmd = cmdItem->cmd;
                    memcpy(currentCmd.extra, cmdItem->extra, 32);
                    memcpy(currentCmd.data, cmdItem->data, cmdItem->len);
                    currentCmd.len = cmdItem->len;

                    size_t rc = send_cmd(coordinatorFd, (uint8_t *) cmdItem->data, cmdItem->len, kernel);
                    usleep(100000);
                    SLIST_REMOVE_HEAD(&e18_cmd_queue_head, cmds);
                    free(cmdItem->data);
                    free(cmdItem);
                    // TODO: проверка rc
                }
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


            // TODO: Следить за наличием сети!!! Отправка данных только если сеть есть. Если сеть не поднимается дольше чем 30 секунд, сообщение на сервер!
            // при старте координатора можно "пропустить" сообщение о наличии сети, если сети нет,
            // проверяем отдельной командой её наличие
            if (!isCmdRun && !isNetwork) {
                printf("check network state\n");
                e18_cmd_get_network_state(coordinatorFd, kernel);
            }

            // рассылаем пакет с текущим "временем" раз в 10 секунд
            currentTime = time(nullptr) + kernel->timeOffset;
            if (isNetwork && !isCmdRun && currentTime - syncTimeTime >= 10) {
                // В "ручном" режиме пакет со времменем не рассылаем, т.к. в нём передаётся уровень диммирования для
                // каждой группы. При этом какое бы значение мы не установили по умолчанию, оно "затрёт" установленное
                // вручную оператором, что для демонстрационного режима неприемлемо.
                syncTimeTime = currentTime;

                if (!manualMode(dBase)) {
                    mtm_cmd_current_time currentTimeCmd;
                    currentTimeCmd.header.type = MTM_CMD_TYPE_CURRENT_TIME;
                    currentTimeCmd.header.protoVersion = MTM_VERSION_0;
                    localTime = localtime(&currentTime);
                    currentTimeCmd.time = localTime->tm_hour * 60 + localTime->tm_min;
                    for (int idx = 0; idx < 16; idx++) {
                        currentTimeCmd.brightLevel[idx] = lightGroupBright[idx];
                    }

                    printf("Send packets with time\n");
                    ssize_t rc = send_e18_hex_cmd(coordinatorFd, E18_BROADCAST_ADDRESS, &currentTimeCmd, kernel);

                    if (kernel->isDebug) {
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.", TAG, rc);
                    }

                    if (rc == -1) {
                        lostZBCoordinator(dBase, threadId, &coordinatorUuid);
                        return;
                    }

                }
            }

            // опрашиваем датчик двери шкафа
            currentTime = time(nullptr);
            if (!isCmdRun && currentTime - checkDoorSensorTime >= 10) {
                checkDoorSensorTime = currentTime;
                printf("Check door sensor\n");
                e18_cmd_read_gpio_level(coordinatorFd, E18_LOCAL_DATA_ADDRESS, E18_PIN_DOOR, kernel);
            }

            // опрашиваем датчик контактора
            currentTime = time(nullptr);
            if (!isCmdRun && currentTime - checkContactorSensorTime >= 10) {
                checkContactorSensorTime = currentTime;
                printf("Check contactor sensor\n");
                e18_cmd_read_gpio_level(coordinatorFd, E18_LOCAL_DATA_ADDRESS, E18_PIN_CONTACTOR, kernel);
            }

            // опрашиваем датчик реле
            currentTime = time(nullptr);
            if (!isCmdRun && currentTime - checkRelaySensorTime >= 10) {
                checkRelaySensorTime = currentTime;
                printf("Check relay sensor\n");
                e18_cmd_read_gpio_level(coordinatorFd, E18_LOCAL_DATA_ADDRESS, E18_PIN_RELAY, kernel);
            }

            // опрашиваем датчик температуры на координаторе
            //

            // получаем скорость порта, по полученному ответу понимаем что модуль работает
            currentTime = time(nullptr);
            if (!isCmdRun && currentTime - checkCoordinatorTime >= 15) {
                if (!isCheckCoordinatorRespond) {
                    // координатор не ответил
                    kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR Coordinator not answer for request module version",
                                      TAG);
                    // останавливаем поток с целью его последующего автоматического запуска и инициализации
                    mtmZigbeeStopThread(mtmZigbeeDBase, threadId);
                    AddDeviceRegister(mtmZigbeeDBase, (char *) coordinatorUuid.data(),
                                      (char *) "Координатор не ответил на запрос");
                    return;
                }

                // сбрасываем флаг полученного ответа от координатора
                isCheckCoordinatorRespond = false;
//                uint8_t buff[] = {
//                        0xFB, 0x09
//                };
//                send_cmd(coordinatorFd, buff, sizeof(buff), kernel);

                checkCoordinatorTime = currentTime;
                printf("Check baud rate\n");
                e18_cmd_get_baud_rate(coordinatorFd, kernel);
            }

            // проверка на наступление астрономических событий
            currentTime = time(nullptr) + kernel->timeOffset;
            if (isNetwork && !isCmdRun && currentTime - checkAstroTime > 60) {
                // костыль для демонстрационных целей, т.е. когда флаг установлен, ни какого автоматического
                // управления светильниками не происходит. только ручной режим.
                if (!manualMode(dBase)) {
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
                    checkLightProgram(mtmZigbeeDBase, currentTime, lon, lat);
                }
            }

            currentTime = time(nullptr);
            if (currentTime - checkLinkState > 10) {
                checkLinkState = currentTime;
                mtmCheckLinkState(mtmZigbeeDBase);
            }

            currentTime = time(nullptr);
            if (isNetwork && currentTime - checkOutPacket > 2) {
                checkOutPacket = currentTime;
                mtmZigbeeProcessOutPacket(threadId);
            }

            // получаем короткие адреса для светильников
            currentTime = time(nullptr);
            if (isNetwork && !isCmdRun && currentTime - checkShortAddresses > 300) {
                checkShortAddresses = currentTime;
                printf("check short addresses\n");

                // получаем все устройства типа управляемый светильник и координатор
                std::string query;
                query.append("SELECT * FROM device WHERE deviceTypeUuid IN ")
                        .append("('" + std::string(DEVICE_TYPE_ZB_LIGHT) + "', ")
                        .append("'" + std::string(DEVICE_TYPE_ZB_COORDINATOR) + "')");
                MYSQL_RES *res = dBase->sqlexec(query.data());
                if (res) {
                    dBase->makeFieldsList(res);
                    int nRows = mysql_num_rows(res);
                    if (nRows > 0) {
                        for (uint32_t i = 0; i < nRows; i++) {
                            MYSQL_ROW row = mysql_fetch_row(res);
                            if (row) {
                                std::string address = dBase->getFieldValue(row, "address");
                                e18_cmd_get_remote_short_address(coordinatorFd, (uint8_t *) address.data(), kernel);
                            }
                        }
                    }

                    mysql_free_result(res);
                }
            }


            run = mtmZigbeeGetRun();

            usleep(10000);
        }
    }
}

bool manualMode(DBase *dBase) {
    std::string query;
    MYSQL_RES *res;
    MYSQL_ROW row;
    my_ulonglong nRows;
    int mode = 0;
    std::string coordUuid;

    // ищем координатор
    query.append(
            "SELECT * FROM device WHERE deviceTypeUuid = '" + std::string(DEVICE_TYPE_ZB_COORDINATOR) + "' LIMIT 1");
    res = dBase->sqlexec(query.data());
    if (res) {
        nRows = mysql_num_rows(res);
        if (nRows == 1) {
            dBase->makeFieldsList(res);
            row = mysql_fetch_row(res);
            if (row != nullptr) {
                coordUuid = std::string(row[dBase->getFieldIndex("uuid")]);
            }
        }
    }

    mysql_free_result(res);

    if (!coordUuid.empty()) {
        // ищем настроку координатора
        query = "SELECT * FROM device_config WHERE deviceUuid = '" + coordUuid
                + "' AND parameter='" + std::string(DEVICE_PARAMETER_ZB_COORD_MODE) + "' LIMIT 1";
        res = dBase->sqlexec(query.data());
        if (res) {
            nRows = mysql_num_rows(res);
            if (nRows == 1) {
                dBase->makeFieldsList(res);
                row = mysql_fetch_row(res);
                if (row != nullptr) {
                    mode = std::stoi(row[dBase->getFieldIndex("value")]);
                }
            }
        }
    }

    mysql_free_result(res);

    return mode == 1;
}

ssize_t switchAllLight(uint16_t level) {
    mtm_cmd_action action = {0};
    action.header.type = MTM_CMD_TYPE_ACTION;
    action.header.protoVersion = MTM_VERSION_0;
    action.device = MTM_DEVICE_LIGHT;
    action.data = level;
    ssize_t rc = send_e18_hex_cmd(coordinatorFd, E18_BROADCAST_ADDRESS, &action, kernel);
    return rc;
}

void switchContactor(bool enable, uint8_t line) {
    e18_cmd_set_gpio_level(coordinatorFd, E18_LOCAL_DATA_ADDRESS, line,
                           enable ? E18_LEVEL_HI : E18_LEVEL_LOW, kernel);
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

    sprintf((char *) query,
            "SELECT lmt._id, lmt.data, ept.value AS address FROM light_message lmt LEFT JOIN device dt ON lmt.address=dt.address LEFT JOIN entity_parameter ept ON ept.entityUuid=dt.uuid AND ept.parameter='shortAddr' WHERE dateOut IS NULL;");
    if (kernel->isDebug) {
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s", TAG, query);
    }

    res = mtmZigbeeDBase->sqlexec((char *) query);
    if (res) {
        mtmZigbeeDBase->makeFieldsList(res);
        nRows = mysql_num_rows(res);
        if (nRows == 0) {
            mysql_free_result(res);
            return;
        }

        for (uint32_t i = 0; i < nRows; i++) {
            row = mysql_fetch_row(res);
            if (row) {
                lengths = mysql_fetch_lengths(res);
                fieldIdx = mtmZigbeeDBase->getFieldIndex("address");
                flen = lengths[fieldIdx];
                if (flen == 0) {
                    // короткого адреса нет, ни чего не отправляем
                    continue;
                }

                memset(tmpAddr, 0, 1024);
                strncpy((char *) tmpAddr, row[fieldIdx], flen);
                if (kernel->isDebug) {
                    kernel->log.ulogw(LOG_LEVEL_INFO, "Addr: %s, ", tmpAddr);
                }

                dstAddr = strtoull((char *) tmpAddr, nullptr, 16);

                fieldIdx = mtmZigbeeDBase->getFieldIndex("data");
                flen = lengths[fieldIdx];
                memset(tmpData, 0, 1024);
                strncpy((char *) tmpData, row[fieldIdx], flen);
                if (kernel->isDebug) {
                    kernel->log.ulogw(LOG_LEVEL_INFO, "Data: %s", tmpData);
                }

                struct base64_decode_ctx b64_ctx = {};
                size_t decoded = 512;
                base64_decode_init(&b64_ctx);
                if (base64_decode_update(&b64_ctx, &decoded, mtmPkt, flen, (const char *) tmpData)) {
                    if (base64_decode_final(&b64_ctx)) {
                        uint8_t pktType = mtmPkt[0];
                        switch (pktType) {
                            case MTM_CMD_TYPE_CONFIG:
                                if (kernel->isDebug) {
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_CONFIG", TAG);
                                }

                                mtm_cmd_config config;
                                config.header.type = mtmPkt[0];
                                config.header.protoVersion = mtmPkt[1];
                                config.device = mtmPkt[2];
                                config.min = *(uint16_t *) &mtmPkt[3];
                                config.max = *(uint16_t *) &mtmPkt[5];

                                rc = send_e18_hex_cmd(coordinatorFd, dstAddr, &config, kernel);

                                if (kernel->isDebug) {
                                    log_buffer_hex(mtmPkt, decoded);
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.", TAG, rc);
                                }

                                break;
                            case MTM_CMD_TYPE_CONFIG_LIGHT:
                                if (kernel->isDebug) {
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_CONFIG_LIGHT", TAG);
                                }

                                mtm_cmd_config_light config_light;
                                config_light.header.type = mtmPkt[0];
                                config_light.header.protoVersion = mtmPkt[1];
                                config_light.device = mtmPkt[2];
                                for (int nCfg = 0; nCfg < MAX_LIGHT_CONFIG; nCfg++) {
                                    config_light.config[nCfg].time = *(uint16_t *) &mtmPkt[3 + nCfg * 4];
                                    config_light.config[nCfg].value = *(uint16_t *) &mtmPkt[3 + nCfg * 4 + 2];
                                }

                                rc = send_e18_hex_cmd(coordinatorFd, dstAddr, &config_light, kernel);

                                if (kernel->isDebug) {
                                    log_buffer_hex(mtmPkt, decoded);
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.", TAG, rc);
                                }

                                break;
                            case MTM_CMD_TYPE_CURRENT_TIME:
                                if (kernel->isDebug) {
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_CURRENT_TIME", TAG);
                                }

                                mtm_cmd_current_time current_time;
                                current_time.header.type = mtmPkt[0];
                                current_time.header.protoVersion = mtmPkt[1];
                                current_time.time = *(uint16_t *) &mtmPkt[2];

                                rc = send_e18_hex_cmd(coordinatorFd, dstAddr, &current_time, kernel);

                                if (kernel->isDebug) {
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.", TAG, rc);
                                    log_buffer_hex(mtmPkt, decoded);
                                }

                                break;
                            case MTM_CMD_TYPE_ACTION:
                                if (kernel->isDebug) {
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_ACTION", TAG);
                                }

                                mtm_cmd_action action;
                                action.header.type = mtmPkt[0];
                                action.header.protoVersion = mtmPkt[1];
                                action.device = mtmPkt[2];
                                action.data = *(uint16_t *) &mtmPkt[3];

                                rc = send_e18_hex_cmd(coordinatorFd, dstAddr, &action, kernel);

                                if (kernel->isDebug) {
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.", TAG, rc);
                                    log_buffer_hex(mtmPkt, decoded);
                                }

                                break;
                            case MTM_CMD_TYPE_CONTACTOR:
                                if (kernel->isDebug) {
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_CONTACTOR", TAG);
                                    log_buffer_hex(mtmPkt, decoded);
                                }

                                char message[1024];
                                sprintf(message, "Получена команда %s реле контактора.",
                                        mtmPkt[3] == 0 ? "выключения" : "включения");
                                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s", TAG, message);
                                switchContactor(mtmPkt[3], E18_PIN_RELAY);
                                AddDeviceRegister(mtmZigbeeDBase, (char *) coordinatorUuid.data(), message);
                                rc = 1; // костыль от старой реализации
                                break;
                            case MTM_CMD_TYPE_RESET_COORDINATOR:
                                if (kernel->isDebug) {
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_RESET_COORDINATOR", TAG);
                                    log_buffer_hex(mtmPkt, decoded);
                                }

                                rc = resetCoordinator();
                                // останавливаем поток с целью его последующего автоматического запуска и инициализации
                                mtmZigbeeStopThread(mtmZigbeeDBase, threadId);
                                AddDeviceRegister(mtmZigbeeDBase, (char *) coordinatorUuid.data(),
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
                            lostZBCoordinator(mtmZigbeeDBase, threadId, &coordinatorUuid);
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
    uint8_t address[32];
    uint8_t parentAddress[32];
    auto *addressStr = new std::string();
    auto *parentAddressStr = new std::string();
    uint16_t sensorDataCount;
    Device *device;

    auto *pktHeader = (mtm_cmd_header *) pktBuff;
    if (kernel->isDebug) {
        char pktStr[2048] = {0};
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] RAW in packet", TAG);
        for (int i = 0; i < length; i++) {
            sprintf(&pktStr[i * 2], "%02X", pktBuff[i]);
        }

        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s", TAG, pktStr);
    }

    uint8_t resultBuff[1024] = {};
    struct base64_encode_ctx b64_ctx = {};
    int encoded_bytes;

    switch (pktHeader->type) {
        case MTM_CMD_TYPE_STATUS :
            if (kernel->isDebug) {
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] MTM_CMD_TYPE_STATUS", TAG);
                log_buffer_hex(pktBuff, length);
            }

            // из размера пакета вычитаем два байта заголовка, восемь байт адреса, два байта флагов аварии
            sensorDataCount = (length - 12);
            if (pktHeader->protoVersion == MTM_VERSION_1) {
                // вычитаем ещё 8 байт родительского MAC
                sensorDataCount -= 8;
            }

            sensorDataCount /= 2;

            memset(address, 0, 32);
            if (pktHeader->protoVersion == MTM_VERSION_0) {
                auto *pkt = (mtm_cmd_status *) pktBuff;
                sprintf((char *) address, "%016lX", *(uint64_t *) pkt->mac);
            } else if (pktHeader->protoVersion == MTM_VERSION_1) {
                auto *pkt = (mtm_cmd_status_v1 *) pktBuff;
                sprintf((char *) address, "%016lX", *(uint64_t *) pkt->mac);
                memset(parentAddress, 0, 32);
                sprintf((char *) parentAddress, "%016lX", *(uint64_t *) pkt->parentMac);
                parentAddressStr->assign((char *) parentAddress);
            } else {
                // неизвестная версия протокола
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] Не известная версия протокола: %d", TAG,
                                  pktHeader->protoVersion);
                return;
            }

            addressStr->assign((char *) address);

            base64_encode_init(&b64_ctx);
#ifdef __APPLE__
        encoded_bytes = base64_encode_update(&b64_ctx, (char *) resultBuff, mtmLightStatusPktSize, pktBuff);
        base64_encode_final(&b64_ctx, reinterpret_cast<char *>(resultBuff + encoded_bytes));
#elif __USE_GNU
            encoded_bytes = base64_encode_update(&b64_ctx, (char *) resultBuff, length, pktBuff);
            base64_encode_final(&b64_ctx, (char *) (resultBuff + encoded_bytes));
#endif

            if (kernel->isDebug) {
                // для отладочных целей, сохраняем пакет в базу
                uint8_t query[1024];
                MYSQL_RES *res;
                time_t createTime = time(nullptr);

                sprintf((char *) query,
                        "INSERT INTO light_answer (address, data, createdAt, changedAt, dateIn) value('%s', '%s', FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld))",
                        address, resultBuff, createTime, createTime, createTime);
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s", TAG, query);
                res = mtmZigbeeDBase->sqlexec((const char *) query);
                if (res) {
                    mysql_free_result(res);
                }
            }

            // получаем устройство
            device = findDeviceByAddress(mtmZigbeeDBase, addressStr);
            if (device != nullptr) {
                uint16_t listLength;
                SensorChannel *list = findSensorChannelsByDevice(mtmZigbeeDBase, &device->uuid, &listLength);
                if (list != nullptr) {
                    for (uint16_t i = 0; i < listLength; i++) {
                        uint16_t reg = list[i].reg;
                        uint8_t sensorDataStart = get_mtm_status_data_start(pktHeader->protoVersion);
                        if (reg < sensorDataCount) {
                            // добавляем измерение
                            if (list[i].measureTypeUuid == CHANNEL_STATUS) {
                                uint16_t alerts = *(uint16_t *) &pktBuff[sensorDataStart - 2];
                                int8_t value = alerts & 0x0001u;
                                storeMeasureValueExt(mtmZigbeeDBase, &list[i], value, true);
                            } else if (list[i].measureTypeUuid == CHANNEL_W) {
                                // данные по потребляемой мощности хранятся в младшем байте датчика 0 (reg = 0)
                                int8_t value = pktBuff[sensorDataStart + reg * 2];
                                storeMeasureValueExt(mtmZigbeeDBase, &list[i], value, true);
                            } else if (list[i].measureTypeUuid == CHANNEL_T) {
                                // данные по температуре хранятся в старшем байте датчика 0 (reg = 0)
                                int8_t value = pktBuff[sensorDataStart + reg * 2 + 1];
                                storeMeasureValueExt(mtmZigbeeDBase, &list[i], value, true);
                            } else if (list[i].measureTypeUuid == CHANNEL_RSSI) {
                                // данные по rssi хранятся в младшем байте датчика 1 (reg = 1)
                                int8_t value = pktBuff[sensorDataStart + reg * 2];
                                storeMeasureValueExt(mtmZigbeeDBase, &list[i], value, true);
                            } else if (list[i].measureTypeUuid == CHANNEL_HOP_COUNT) {
                                // данные по количеству хопов хранятся в старшем байте датчика 0 (reg = 1)
                                int8_t value = pktBuff[sensorDataStart + reg * 2 + 1];
                                storeMeasureValueExt(mtmZigbeeDBase, &list[i], value, true);
                            } else if (list[i].measureTypeUuid == CHANNEL_CO2) {
                                // данные по CO2 хранятся в виде 16 бит значения датчика 2 (reg = 2)
                                uint16_t value = *(uint16_t *) &pktBuff[sensorDataStart + reg * 2];
                                storeMeasureValueExt(mtmZigbeeDBase, &list[i], value, false);
                            }
                        }
                    }
                }
            }
            break;
        case MTM_CMD_TYPE_CONFIG:
        case MTM_CMD_TYPE_CONFIG_LIGHT:
        case MTM_CMD_TYPE_CURRENT_TIME:
        case MTM_CMD_TYPE_ACTION:
        default:
            printf("skip mtm packet\n");
            break;
    }
}

int32_t mtmZigbeeInit(int32_t mode, uint8_t *path, uint64_t speed) {
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
            kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] BaudRate = %i StopBits = 1 Parity = none\n", TAG, speed);
        }

        // Инициализируем внешние линии координатора e18
        // отключаем реле
        e18_cmd_set_gpio_level(coordinatorFd, E18_LOCAL_DATA_ADDRESS, E18_PIN_RELAY, E18_LEVEL_LOW, kernel);
        // индикатор
        e18_cmd_init_gpio(coordinatorFd, E18_LOCAL_DATA_ADDRESS, E18_PIN_LED, E18_PIN_OUTPUT, kernel);
        // реле
        e18_cmd_init_gpio(coordinatorFd, E18_LOCAL_DATA_ADDRESS, E18_PIN_RELAY, E18_PIN_OUTPUT, kernel);
        // дверь
        e18_cmd_init_gpio(coordinatorFd, E18_LOCAL_DATA_ADDRESS, E18_PIN_DOOR, E18_PIN_INPUT, kernel);
        // контактор
        e18_cmd_init_gpio(coordinatorFd, E18_LOCAL_DATA_ADDRESS, E18_PIN_CONTACTOR, E18_PIN_INPUT, kernel);

        tcflush(coordinatorFd, TCIFLUSH);   /* Discards old data in the rx buffer            */
    }

    return 0;
}

speed_t mtmZigbeeGetSpeed(uint64_t speed) {
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

