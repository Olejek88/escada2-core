//
// Created by koputo on 08.01.22.
//

#include <TypeThread.h>
#include <sys/queue.h>
#include "E18Module.h"
// TODO: избавиться от этого заголовка
#include "MtmZigbee.h"
#include "ce102.h"
#include "EntityParameter.h"
#include <sstream>
#include <main.h>
#include <function.h>
#include <drivers/lightUtils.h>
#include <nettle/base64.h>
#include <nettle/version.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <uuid/uuid.h>
#include <jsoncpp/json/json.h>
#include <suninfo.h>
#include "zigbeemtm.h"
#include "Device.h"
#include "SensorChannel.h"

//E18Module::E18Module() {
//}

E18Module::E18Module(Kernel *kernel, TypeThread *tth) {
    this->kernel = kernel;
    this->pth = tth;
}

E18Module::~E18Module() {
    int a = 0;
}

void *E18Module::getModuleThread(void *context) {
    return ((E18Module *) context)->moduleThread(nullptr);
}

void *E18Module::moduleThread(TypeThread *tth) {
    uint64_t speed;
    uint8_t *port;
    auto *tInfo = this->pth;

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
            mtmZigbeePktListener();
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

    dBase->disconnect();
    delete dBase;
    free(port);

    return nullptr;
}

void E18Module::mtmZigbeePktListener() {
    bool run = true;
    int64_t count;
    uint8_t data;
    uint8_t seek[1024];
    // при старте счтаем что сети нет и начинаем отсчёт
    time_t lostNetworkTime = time(nullptr);
    bool isLostNetworkAlarmSent = false;
    // по умолчанию проверяем наличие сети
    bool isNetworkCheck = true;
    bool isNetwork = false;
    bool isCmdRun = false;
    E18CmdItem currentCmd;
    time_t currentTime, heartBeatTime = 0, syncTimeTime = 0, checkDoorSensorTime = 0, checkContactorSensorTime = 0,
            checkRelaySensorTime = 0, checkAstroTime = 0, checkOutPacket = 0, checkCoordinatorTime = 0,
            checkLinkState = 0, checkShortAddresses = 0, checkNetworkTime = 0, checkGetStatus = 0;
    struct tm *localTime;
    uint16_t addr;
    uint8_t inState, outState;
    std::stringstream sstream;

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

//-------------------
    // складываем тестовый пакет в список
//    uint8_t testPkt[] = {
//            0x01, 0x01,
////            0x34, 0x25, 0x43, 0x24, 0x00, 0x4B, 0x12, 0x00,
////            0x58, 0xC8, 0x90, 0x11, 0x00, 0x4B, 0x12, 0x00,
//            0x58, 0xC8, 0x90, 0x11, 0x00, 0x4B, 0x12, 0x00, // фонарь
//            0x17, 0x29, 0x9D, 0x20, 0x00, 0x4B, 0x12, 0x00, // родитель
//            0x00, 0x00,
//            0x00, 0x1B, 0xFF, 0x01, 0x02, 0x00
//    };
//    zb_item = (struct zb_pkt_item *) malloc(sizeof(struct zb_pkt_item));
//    zb_item->len = sizeof(testPkt);
//    zb_item->pkt = malloc(zb_item->len);
//    memcpy(zb_item->pkt, testPkt, zb_item->len);
//
//    ((uint8_t *)zb_item->pkt)[0] = 0x0A;
//    ((uint8_t *)zb_item->pkt)[1] = 0x00;
//
//    ((uint8_t *)zb_item->pkt)[2] = 0x66;
//    ((uint8_t *)zb_item->pkt)[3] = 0x55;
//    ((uint8_t *)zb_item->pkt)[4] = 0x44;
//    ((uint8_t *)zb_item->pkt)[5] = 0x33;
//    ((uint8_t *)zb_item->pkt)[6] = 0x22;
//    ((uint8_t *)zb_item->pkt)[7] = 0x11;
//    ((uint8_t *)zb_item->pkt)[8] = 0x12;
//    ((uint8_t *)zb_item->pkt)[9] = 0x00;
//
//    ((uint8_t *)zb_item->pkt)[10] = 0x66;
//    ((uint8_t *)zb_item->pkt)[11] = 0x55;
//    ((uint8_t *)zb_item->pkt)[12] = 0x44;
//    ((uint8_t *)zb_item->pkt)[13] = 0x33;
//    ((uint8_t *)zb_item->pkt)[14] = 0x22;
//    ((uint8_t *)zb_item->pkt)[15] = 0x11;
//    ((uint8_t *)zb_item->pkt)[16] = 0x12;
//    ((uint8_t *)zb_item->pkt)[17] = 0x00;
//
//    ((uint8_t *)zb_item->pkt)[18] = 0xBB;
//    ((uint8_t *)zb_item->pkt)[19] = 0xAA;
//    SLIST_INSERT_HEAD(&zb_queue_head, zb_item, items);
//-------------------
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
                    case E18_HEX_CMD_OFF_NETWORK_AND_RESTART:
                    case E18_HEX_CMD_DEVICE_RESTART:
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
                if (e18_read_fixed_data(seek, readDataLen) < 0) {
                    // ошибка чтения данных, нужно остановить поток
                    lostZBCoordinator();
                    return;
                }

                // обрабатываем при необходимости полученные данные
                switch (currentCmd.cmd) {
                    case E18_HEX_CMD_GET_NETWORK_STATE :
                        isNetwork = seek[0] == 1;
                        printf("get network state is %s\n", isNetwork ? "TRUE" : "FALSE");
                        if (!isNetwork) {
                            if (!isNetworkCheck) {
                                isLostNetworkAlarmSent = false;
                                lostNetworkTime = time(nullptr);
                                isNetworkCheck = true;
                            }
                        } else {
                            isNetworkCheck = false;
                        }

                        break;
                    case E18_HEX_CMD_GET_UART_BAUD_RATE :
                        isCheckCoordinatorRespond = true;
                        break;
                    case E18_HEX_CMD_GET_GPIO_LEVEL :
                        addr = *(uint16_t *) &seek[1];
                        inState = seek[3];
                        outState = seek[4];
                        printf("Addr: 0x%04X, In: %d, Out: %d\n", addr, inState, outState);
                        switch (currentCmd.pin) {
                            case E18_PIN_DOOR :
                                printf("door status: in=%d, out=%d\n", inState, outState);
                                storeCoordinatorDoorStatus(inState == 1, outState == 1);
                                break;
                            case E18_PIN_CONTACTOR :
                                printf("contactor status: in=%d, out=%d\n", inState, outState);
                                storeCoordinatorContactorStatus(inState == 1, outState == 1);
                                break;
                            case E18_PIN_RELAY :
                                printf("relay status: in=%d, out=%d\n", inState, outState);
                                storeCoordinatorRelayStatus(inState == 1, outState == 1);
                                break;
                            default:
                                break;
                        }

                        break;
                    case E18_HEX_CMD_GET_REMOTE_SHORT_ADDR :
                        // сохраняем/обновляем запись с коротким адресом
                        sstream.str("");
                        sstream << std::hex << *(uint16_t *) seek;
                        e18_store_parameter(currentCmd.mac, std::string("shortAddr"), std::string(sstream.str()));
                    case E18_HEX_CMD_OFF_NETWORK_AND_RESTART :
                    case E18_HEX_CMD_DEVICE_RESTART :
                        // ни чего не делаем
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
                if (e18_read_fixed_data(seek, readDataLen) < 0) {
                    // ошибка чтения данных, нужно остановить поток
                    lostZBCoordinator();
                    return;
                }

                if (seek[0] != currentCmd.cmd) {
                    printf("write answer do not match! received %02X\n", seek[1]);
                }

                switch (currentCmd.cmd) {
                    case E18_HEX_CMD_SET_GPIO_LEVEL :
                        if (currentCmd.pin == E18_PIN_RELAY) {
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
                if (e18_read_fixed_data(seek, 1) < 0) {
                    // ошибка чтения данных, нужно остановить поток
                    lostZBCoordinator();
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
                if (e18_read_fixed_data(seek, 1) < 0) {
                    // ошибка чтения данных, нужно остановить поток
                    lostZBCoordinator();
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
                    lostNetworkTime = time(nullptr);
                    isLostNetworkAlarmSent = false;
                    isNetworkCheck = true;
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
                        lostZBCoordinator();
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
                if (!e18_cmd_queue.empty()) {
                    E18CmdItem cmdItem = e18_cmd_queue.front();
                    isCmdRun = true;

                    currentCmd = cmdItem;
                    size_t rc = send_cmd((uint8_t *) cmdItem.data, cmdItem.dataLen);
                    usleep(100000);
                    e18_cmd_queue.pop();

                    if (kernel->isDebug) {
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.", TAG, rc);
                    }

                    if (rc == -1) {
                        lostZBCoordinator();
                        return;
                    }
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
                sprintf(query, "SELECT * FROM threads WHERE _id = %d", mtmZigBeeThreadId);
                res = dBase->sqlexec(query);
                if (res) {
                    nRows = mysql_num_rows(res);
                    if (nRows == 1) {
                        dBase->makeFieldsList(res);
                        row = mysql_fetch_row(res);
                        if (row != nullptr) {
                            isWork = std::stoi(row[dBase->getFieldIndex("work")]);
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
                        UpdateThreads(*dBase, mtmZigBeeThreadId, 0, 1, nullptr);
                    } else {
                        // поток "остановили"
                        sprintf(query, "UPDATE threads SET status=%d, changedAt=FROM_UNIXTIME(%lu) WHERE _id=%d", 0,
                                currentTime, mtmZigBeeThreadId);
                        res = dBase->sqlexec(query);
                        mysql_free_result(res);
                        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] Thread stopped from GUI", TAG);
                        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] Stopping thread", TAG);
                        return;
                    }
                }
            }

            //  Если сеть не поднимается дольше чем 30 секунд, сообщение на сервер
            if (!isNetwork && !isLostNetworkAlarmSent) {
                if (time(nullptr) - lostNetworkTime > 30) {
                    AddDeviceRegister(dBase, (char *) coordinatorUuid.c_str(), (char *) "Нет ZigBee сети!");
                    isLostNetworkAlarmSent = true;
                }
            }

            // при старте координатора можно "пропустить" сообщение о наличии сети, если сети нет,
            // проверяем отдельной командой её наличие
            currentTime = time(nullptr);
            if (!isCmdRun && !isNetwork && currentTime - checkNetworkTime > 3) {
                printf("check network state\n");
                checkNetworkTime = currentTime;
                if (!isNetworkCheck) {
                    lostNetworkTime = time(nullptr);
                    isNetworkCheck = true;
                }

                e18_cmd_get_network_state();
            }

            // рассылаем пакет с текущим "временем" раз в 10 секунд
            currentTime = time(nullptr) + kernel->timeOffset;
            if (isNetwork && !isCmdRun && currentTime - syncTimeTime >= 10) {
                // В "ручном" режиме пакет со времменем не рассылаем, т.к. в нём передаётся уровень диммирования для
                // каждой группы. При этом какое бы значение мы не установили по умолчанию, оно "затрёт" установленное
                // вручную оператором, что для демонстрационного режима неприемлемо.
                syncTimeTime = currentTime;

                if (!this->manualMode()) {
                    mtm_cmd_current_time currentTimeCmd;
                    currentTimeCmd.header.type = MTM_CMD_TYPE_CURRENT_TIME;
                    currentTimeCmd.header.protoVersion = MTM_VERSION_0;
                    localTime = localtime(&currentTime);
                    currentTimeCmd.time = localTime->tm_hour * 60 + localTime->tm_min;
                    for (int idx = 0; idx < 16; idx++) {
                        currentTimeCmd.brightLevel[idx] = lightGroupBright[idx];
                    }

                    printf("Send packets with time\n");
                    ssize_t rc = send_e18_hex_cmd(E18_BROADCAST_ADDRESS, &currentTimeCmd);

                    if (kernel->isDebug) {
                        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] Written %ld bytes.", TAG, rc);
                    }

                    if (rc == -1) {
                        lostZBCoordinator();
                        return;
                    }
                }
            }

            // опрашиваем датчик двери шкафа
            currentTime = time(nullptr);
            if (!isCmdRun && currentTime - checkDoorSensorTime >= 10) {
                checkDoorSensorTime = currentTime;
                printf("Check door sensor\n");
                e18_cmd_read_gpio_level(E18_LOCAL_DATA_ADDRESS, E18_PIN_DOOR);
            }

            // опрашиваем датчик контактора
            currentTime = time(nullptr);
            if (!isCmdRun && currentTime - checkContactorSensorTime >= 10) {
                checkContactorSensorTime = currentTime;
                printf("Check contactor sensor\n");
                e18_cmd_read_gpio_level(E18_LOCAL_DATA_ADDRESS, E18_PIN_CONTACTOR);
            }

            // опрашиваем датчик реле
            currentTime = time(nullptr);
            if (!isCmdRun && currentTime - checkRelaySensorTime >= 10) {
                checkRelaySensorTime = currentTime;
                printf("Check relay sensor\n");
                e18_cmd_read_gpio_level(E18_LOCAL_DATA_ADDRESS, E18_PIN_RELAY);
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
                    mtmZigbeeStopThread();
                    AddDeviceRegister(dBase, (char *) coordinatorUuid.data(),
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
                e18_cmd_get_baud_rate();
            }

            // проверка на наступление астрономических событий
            currentTime = time(nullptr) + kernel->timeOffset;
            if (isNetwork && !isCmdRun && currentTime - checkAstroTime > 60) {
                checkAstroTime = currentTime;
                if (kernel->isDebug) {
                    char currentTimeString[64] = {0};
                    std::tm tmpTm = {0};
                    localtime_r(&currentTime, &tmpTm);
                    std::strftime(currentTimeString, 20, "%F %T", &tmpTm);
                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s", TAG, currentTimeString);
                }

                // костыль для демонстрационных целей, т.е. когда флаг установлен, ни какого автоматического
                // управления светильниками не происходит. только ручной режим.
                if (!manualMode()) {
                    double lon = 0, lat = 0;
                    MYSQL_RES *res = dBase->sqlexec("SELECT * FROM node LIMIT 1");
                    if (res) {
                        MYSQL_ROW row = mysql_fetch_row(res);
                        dBase->makeFieldsList(res);
                        if (row) {
                            lon = strtod(row[dBase->getFieldIndex("longitude")], nullptr);
                            lat = strtod(row[dBase->getFieldIndex("latitude")], nullptr);
                        }

                        mysql_free_result(res);
                    }

                    // управление контактором, рассылка пакетов светильникам
                    checkAstroEvents(currentTime, lon, lat);

                    // рассылка пакетов светильникам по параметрам заданным в программах
                    checkLightProgram(dBase, currentTime, lon, lat);
                }
            }

            currentTime = time(nullptr);
            if (currentTime - checkLinkState > 10) {
                checkLinkState = currentTime;
                mtmCheckLinkState();
            }

            currentTime = time(nullptr);
            if (isNetwork && currentTime - checkOutPacket > 2) {
                checkOutPacket = currentTime;
                mtmZigbeeProcessOutPacket();
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
                        .append("'" + std::string(DEVICE_TYPE_ZB_COORDINATOR_E18) + "') ")
                        .append("AND deleted=0");
                MYSQL_RES *res = dBase->sqlexec(query.data());
                if (res) {
                    dBase->makeFieldsList(res);
                    int nRows = mysql_num_rows(res);
                    if (nRows > 0) {
                        for (uint32_t i = 0; i < nRows; i++) {
                            MYSQL_ROW row = mysql_fetch_row(res);
                            if (row) {
                                std::string address = dBase->getFieldValue(row, "address");
                                e18_cmd_get_remote_short_address((uint8_t *) address.data());
                            }
                        }
                    }

                    mysql_free_result(res);
                }
            }

            // опрашиваем светильники на предмет их статуса
            currentTime = time(nullptr);
            if (isNetwork && !isCmdRun && currentTime - checkGetStatus > 300) {
                checkGetStatus = currentTime;
                printf("get light module status\n");

                // получаем все устройства типа управляемый светильник
                std::string query;
                query.append("SELECT ept.parameter, ept.value FROM device dt ")
                        .append("LEFT JOIN entity_parameter ept ON ept.entityUuid=dt.uuid ")
                        .append("WHERE deviceTypeUuid='" + std::string(DEVICE_TYPE_ZB_LIGHT) + "' ")
                        .append("AND ept.parameter='shortAddr' AND dt.deleted=0");
                MYSQL_RES *res = dBase->sqlexec(query.data());
                if (res) {
                    dBase->makeFieldsList(res);
                    int nRows = mysql_num_rows(res);
                    if (nRows > 0) {
                        for (uint32_t i = 0; i < nRows; i++) {
                            MYSQL_ROW row = mysql_fetch_row(res);
                            if (row) {
                                std::string address = dBase->getFieldValue(row, "value");
                                uint16_t short_addr = std::stoi(address, 0, 16);
                                mtm_cmd_get_status mtm_cmd;
                                mtm_cmd.header.protoVersion = MTM_VERSION_0;
                                mtm_cmd.header.type = MTM_CMD_TYPE_GET_STATUS;
                                send_e18_hex_cmd(short_addr, &mtm_cmd);
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

bool E18Module::manualMode() {
    std::string query;
    MYSQL_RES *res;
    MYSQL_ROW row;
    my_ulonglong nRows;
    int mode = 0;
    std::string coordUuid;

    // ищем координатор
    query.append(
            "SELECT * FROM device WHERE deviceTypeUuid = '" + std::string(DEVICE_TYPE_ZB_COORDINATOR_E18) +
            "' AND deleted=0 LIMIT 1");
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

        mysql_free_result(res);
    }


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

        mysql_free_result(res);
    }


    return mode == 1;
}

ssize_t E18Module::switchAllLight(uint16_t level) {
    mtm_cmd_action action = {0};
    action.header.type = MTM_CMD_TYPE_ACTION;
    action.header.protoVersion = MTM_VERSION_0;
    action.device = MTM_DEVICE_LIGHT;
    action.data = level;
    ssize_t rc = send_e18_hex_cmd(E18_BROADCAST_ADDRESS, &action);
    return rc;
}

void E18Module::switchContactor(bool enable, uint8_t line) {
    e18_cmd_set_gpio_level(E18_LOCAL_DATA_ADDRESS, line, enable ? E18_LEVEL_HI : E18_LEVEL_LOW);
}

ssize_t E18Module::resetCoordinator() {
    int dtrFlag = TIOCM_DTR;
    ioctl(coordinatorFd, TIOCMBIS, &dtrFlag);
    ioctl(coordinatorFd, TIOCMBIC, &dtrFlag);
    sleep(2);
    return 1;
}

void E18Module::mtmZigbeeProcessOutPacket() {
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

    res = dBase->sqlexec((char *) query);
    if (res) {
        dBase->makeFieldsList(res);
        nRows = mysql_num_rows(res);
        if (nRows == 0) {
            mysql_free_result(res);
            return;
        }

        for (uint32_t i = 0; i < nRows; i++) {
            row = mysql_fetch_row(res);
            if (row) {
                lengths = mysql_fetch_lengths(res);
                fieldIdx = dBase->getFieldIndex("address");
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

                fieldIdx = dBase->getFieldIndex("data");
                flen = lengths[fieldIdx];
                memset(tmpData, 0, 1024);
                strncpy((char *) tmpData, row[fieldIdx], flen);
                if (kernel->isDebug) {
                    kernel->log.ulogw(LOG_LEVEL_INFO, "Data: %s", tmpData);
                }

                struct base64_decode_ctx b64_ctx = {};
                size_t decoded = 512;
                base64_decode_init(&b64_ctx);
#if (NETTLE_VERSION_MAJOR == 3) && (NETTLE_VERSION_MINOR > 2)
                if (base64_decode_update(&b64_ctx, &decoded, mtmPkt, flen, (const char *) tmpData)) {
#else
                    if (base64_decode_update(&b64_ctx, &decoded, mtmPkt, flen, tmpData)) {
#endif
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

                                rc = send_e18_hex_cmd(dstAddr, &config);

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

                                rc = send_e18_hex_cmd(dstAddr, &config_light);

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

                                rc = send_e18_hex_cmd(dstAddr, &current_time);

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

                                rc = send_e18_hex_cmd(dstAddr, &action);

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
                                AddDeviceRegister(dBase, (char *) coordinatorUuid.data(), message);
                                rc = 1; // костыль от старой реализации
                                break;
                            case MTM_CMD_TYPE_RESET_COORDINATOR:
                                if (kernel->isDebug) {
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_RESET_COORDINATOR", TAG);
                                    log_buffer_hex(mtmPkt, decoded);
                                }

                                rc = resetCoordinator();
                                // останавливаем поток с целью его последующего автоматического запуска и инициализации
                                mtmZigbeeStopThread();
                                AddDeviceRegister(dBase, (char *) coordinatorUuid.data(),
                                                  (char *) "Получена команда сброса координатора");
                                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] Thread stopped by reset coordinator command",
                                                  TAG);
                                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] Stopping thread", TAG);
                                break;
                            case MTM_CMD_TYPE_CLEAR_NETWORK:
                                if (kernel->isDebug) {
                                    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] send MTM_CMD_TYPE_CLEAR_NETWORK", TAG);
                                    log_buffer_hex(mtmPkt, decoded);
                                }

                                e18_cmd_set_network_off();
                                e18_cmd_device_restart();
                                break;
                            default:
                                rc = 0;
                                break;
                        }

                        if (rc > 0) {
                            sprintf((char *) query, "UPDATE light_message SET dateOut=FROM_UNIXTIME(%ld) WHERE _id=%ld",
                                    time(nullptr), strtoul(row[0], nullptr, 10));
                            MYSQL_RES *updRes = dBase->sqlexec((char *) query);
                            if (updRes) {
                                mysql_free_result(updRes);
                            }
                        } else if (rc == -1) {
                            // ошибка записи в порт, останавливаем поток
                            lostZBCoordinator();
                            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] Stopping thread", TAG);
                        }
                    }
                }
            }
        }

        mysql_free_result(res);
    }
}

void E18Module::mtmZigbeeProcessInPacket(uint8_t *pktBuff, uint32_t length) {
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

    MYSQL_RES *res;
    char query[2048] = {0};
    mtm_cmd_beacon *beacon;
    time_t cTime;

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
                // сохраняем/обновляем запись с адресом родителя
                e18_store_parameter(std::string((char *) address), std::string("parentAddr"), *parentAddressStr);
            } else {
                // неизвестная версия протокола
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] Не известная версия протокола: %d", TAG,
                                  pktHeader->protoVersion);
                return;
            }

            addressStr->assign((char *) address);

            base64_encode_init(&b64_ctx);
#if (NETTLE_VERSION_MAJOR == 3) && (NETTLE_VERSION_MINOR > 2)
#ifdef __APPLE__
        encoded_bytes = base64_encode_update(&b64_ctx, (char *) resultBuff, length, pktBuff);
        base64_encode_final(&b64_ctx, reinterpret_cast<char *>(resultBuff + encoded_bytes));
#elif __USE_GNU
            encoded_bytes = base64_encode_update(&b64_ctx, (char *) resultBuff, length, pktBuff);
            base64_encode_final(&b64_ctx, (char *) (resultBuff + encoded_bytes));
#endif
#else
#ifdef __APPLE__
        encoded_bytes = base64_encode_update(&b64_ctx, resultBuff, length, pktBuff);
        base64_encode_final(&b64_ctx, reinterpret_cast<char *>(resultBuff + encoded_bytes));
#elif __USE_GNU
            encoded_bytes = base64_encode_update(&b64_ctx, resultBuff, length, pktBuff);
            base64_encode_final(&b64_ctx, (resultBuff + encoded_bytes));
#endif
#endif

            if (kernel->isDebug) {
                // для отладочных целей, сохраняем пакет в базу
                time_t createTime = time(nullptr);

                sprintf((char *) query,
                        "INSERT INTO light_answer (address, data, createdAt, changedAt, dateIn) value('%s', '%s', FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld), FROM_UNIXTIME(%ld))",
                        address, resultBuff, createTime, createTime, createTime);
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s", TAG, query);
                res = dBase->sqlexec((const char *) query);
                if (res) {
                    mysql_free_result(res);
                }
            }

            // получаем устройство
            device = findDeviceByAddress(dBase, addressStr);
            if (device != nullptr) {
                uint16_t listLength;
                SensorChannel *list = findSensorChannelsByDevice(dBase, &device->uuid, &listLength);
                if (list != nullptr) {
                    for (uint16_t i = 0; i < listLength; i++) {
                        uint16_t reg = list[i].reg;
                        if ((int16_t) reg == -1) {
                            continue;
                        }

                        uint8_t sensorDataStart = get_mtm_status_data_start(pktHeader->protoVersion);
                        if (reg < sensorDataCount) {
                            // добавляем измерение
                            if (list[i].measureTypeUuid == CHANNEL_STATUS) {
                                uint16_t alerts = *(uint16_t *) &pktBuff[sensorDataStart - 2];
                                int8_t value = alerts & 0x0001u;
                                storeMeasureValueExt(dBase, &list[i], value, true);
                                if (value == 1) {
                                    AddDeviceRegister(dBase, (char *) list[i].deviceUuid.c_str(),
                                                      (char *) "Аварийный статус!");
                                    setDeviceStatus(dBase, list[i].deviceUuid, std::string(DEVICE_STATUS_NOT_WORK));
                                }
                            } else if (list[i].measureTypeUuid == CHANNEL_W) {
                                // данные по потребляемой мощности хранятся в младшем байте датчика 0 (reg = 0)
                                int8_t value = pktBuff[sensorDataStart + reg * 2];
                                storeMeasureValueExt(dBase, &list[i], value, true);
                            } else if (list[i].measureTypeUuid == CHANNEL_T) {
                                // данные по температуре хранятся в старшем байте датчика 0 (reg = 0)
                                int8_t value = pktBuff[sensorDataStart + reg * 2 + 1];
                                storeMeasureValueExt(dBase, &list[i], value, true);
                            } else if (list[i].measureTypeUuid == CHANNEL_RSSI) {
                                // данные по rssi хранятся в младшем байте датчика 1 (reg = 1)
                                int8_t value = pktBuff[sensorDataStart + reg * 2];
                                storeMeasureValueExt(dBase, &list[i], value, true);
                            } else if (list[i].measureTypeUuid == CHANNEL_HOP_COUNT) {
                                // данные по количеству хопов хранятся в старшем байте датчика 0 (reg = 1)
                                int8_t value = pktBuff[sensorDataStart + reg * 2 + 1];
                                storeMeasureValueExt(dBase, &list[i], value, true);
                            } else if (list[i].measureTypeUuid == CHANNEL_CO2) {
                                // данные по CO2 хранятся в виде 16 бит значения датчика 2 (reg = 2)
                                uint16_t value = *(uint16_t *) &pktBuff[sensorDataStart + reg * 2];
                                storeMeasureValueExt(dBase, &list[i], value, false);
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
        case MTM_CMD_TYPE_BEACON:
            if (kernel->isDebug) {
                kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] MTM_CMD_TYPE_BEACON", TAG);
                log_buffer_hex(pktBuff, length);
            }

            beacon = (mtm_cmd_beacon *) pktBuff;
            sprintf(query, "SELECT * FROM available_device WHERE macAddress='%016lX'", *(uint64_t *) beacon->mac);
            res = dBase->sqlexec(query);
            cTime = time(nullptr);
            if (res != nullptr) {
                if (mysql_num_rows(res) > 0) {
                    sprintf(query,
                            "UPDATE available_device SET parentMacAddress='%016lX', shortAddress='%04X', changedAt=current_timestamp() " \
                            "WHERE macAddress='%016lX'",
                            *(uint64_t *) beacon->parentMac, beacon->shortAddr, *(uint64_t *) beacon->mac);
                    dBase->sqlexec(query);
                } else {
                    uuid_t newUuid;
                    uint8_t newUuidString[37] = {0};
                    uuid_generate(newUuid);
                    uuid_unparse_upper(newUuid, (char *) newUuidString);
                    sprintf(query,
                            "INSERT INTO available_device (uuid, macAddress, parentMacAddress, shortAddress, createdAt, changedAt) " \
                        "VALUE ('%s', '%016lX', '%016lX', '%04X', FROM_UNIXTIME(%lu), FROM_UNIXTIME(%lu))",
                            newUuidString, *(uint64_t *) beacon->mac, *(uint64_t *) beacon->parentMac,
                            beacon->shortAddr, cTime,
                            cTime);
                    dBase->sqlexec(query);
                }
            }

            mysql_free_result(res);
            break;
        default:
            printf("skip mtm packet\n");
            break;
    }
}

int32_t E18Module::mtmZigbeeInit(int32_t mode, uint8_t *path, uint64_t speed) {
    struct termios serialPortSettings{};

    dBase = new DBase();

    if (dBase->openConnection() != 0) {
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
        // индикатор
        e18_cmd_init_gpio(E18_LOCAL_DATA_ADDRESS, E18_PIN_LED, E18_PIN_OUTPUT);
        // реле
        e18_cmd_init_gpio(E18_LOCAL_DATA_ADDRESS, E18_PIN_RELAY, E18_PIN_OUTPUT);
        // дверь
        e18_cmd_init_gpio(E18_LOCAL_DATA_ADDRESS, E18_PIN_DOOR, E18_PIN_INPUT);
        // контактор
        e18_cmd_init_gpio(E18_LOCAL_DATA_ADDRESS, E18_PIN_CONTACTOR, E18_PIN_INPUT);
        // отключаем реле
        e18_cmd_set_gpio_level(E18_LOCAL_DATA_ADDRESS, E18_PIN_RELAY, E18_LEVEL_LOW);

        tcflush(coordinatorFd, TCIFLUSH);   /* Discards old data in the rx buffer            */
    }

    return 0;
}

speed_t E18Module::mtmZigbeeGetSpeed(uint64_t speed) {
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

bool E18Module::mtmZigbeeGetRun() {
    bool ret;
//    pthread_mutex_lock(&mtmZigbeeStopMutex);
    ret = mtmZigbeeStopIssued;
//    pthread_mutex_unlock(&mtmZigbeeStopMutex);
    return ret;
}

void E18Module::mtmZigbeeSetRun(bool val) {
//    pthread_mutex_lock(&mtmZigbeeStopMutex);
    mtmZigbeeStopIssued = val;
//    pthread_mutex_unlock(&mtmZigbeeStopMutex);
}

// ---------
// отправляем mtm команду
ssize_t E18Module::send_e18_hex_cmd(uint16_t short_addr, void *mtm_cmd) {
    uint8_t buffer[1024];
    uint8_t sendBuffer[1024];
    uint8_t bufferSize;
    uint8_t sendBufferSize = 0;
    ssize_t rc;
    auto *pktHeader = (mtm_cmd_header *) mtm_cmd;

    bufferSize = get_mtm_command_size(pktHeader->type, pktHeader->protoVersion);
    // формируем данные для отправки
    sendBuffer[sendBufferSize++] = E18_HEX_CMD;
    sendBuffer[sendBufferSize++] = 0; // размер передаваемых данных нам пока не известен
    if (short_addr == E18_BROADCAST_ADDRESS) {
        sendBuffer[sendBufferSize++] = E18_HEX_CMD_BROADCAST;
        sendBuffer[sendBufferSize++] = E18_HEX_CMD_BROADCAST_MODE_1;
    } else {
        sendBuffer[sendBufferSize++] = E18_HEX_CMD_UNICAST;
        sendBuffer[sendBufferSize++] = E18_HEX_CMD_UNICAST_TRANSPARENT;
        sendBuffer[sendBufferSize++] = short_addr & 0xFF; // HI byte of addr // NOLINT(hicpp-signed-bitwise)
        sendBuffer[sendBufferSize++] = short_addr >> 8; // LO byte of addr // NOLINT(hicpp-signed-bitwise)
    }

    sendBuffer[sendBufferSize++] = E18_SOF;

    zigbeemtm_get_mtm_cmd_data(pktHeader->type, mtm_cmd, buffer);
    // переносим данные команды в буфер отправки, при необходимости экранируем данные
    for (uint16_t i = 0; i < bufferSize; i++) {
        switch (buffer[i]) {
            case E18_ESC:
            case E18_SOF:
            case E18_EOF:
            case E18_ERROR:
            case E18_NETWORK_STATE:
            case E18_GET_ANSWER:
            case E18_SET_ANSWER:
                sendBuffer[sendBufferSize++] = E18_ESC;
                sendBuffer[sendBufferSize++] = buffer[i] ^ 0x20; // NOLINT(hicpp-signed-bitwise)
                break;
            default:
                sendBuffer[sendBufferSize++] = buffer[i];
                break;
        }
    }

    sendBuffer[sendBufferSize++] = E18_EOF;
    sendBuffer[1] = sendBufferSize - 2; // указываем размер передаваемых данных минус два "служебных" байта

    rc = send_cmd(sendBuffer, sendBufferSize);
    usleep(100000);

    return rc;
}


void E18Module::e18_cmd_init_gpio(uint16_t short_addr, uint8_t line, uint8_t mode) {
    uint8_t hiAddr = short_addr >> 8; // HI byte of addr // NOLINT(hicpp-signed-bitwise)
    uint8_t loAddr = short_addr & 0xFF; // LO byte of addr // NOLINT(hicpp-signed-bitwise)
    uint8_t cmd[] = {
            E18_HEX_CMD_SET,
            0x05,
            E18_HEX_CMD_SET_GPIO_IO_STATUS,
            hiAddr,
            loAddr,
            line,
            mode,
            E18_HEX_CMD_END_CMD
    };

    // складываем команду в список
    E18CmdItem cmdItem;
    cmdItem.dataLen = sizeof(cmd);
    cmdItem.data = new uint8_t(cmdItem.dataLen);
    memcpy(cmdItem.data, cmd, cmdItem.dataLen);
    cmdItem.cmd = E18_HEX_CMD_SET_GPIO_IO_STATUS;
    e18_cmd_queue.push(cmdItem);
}

void E18Module::e18_cmd_set_gpio_level(uint16_t short_addr, uint8_t gpio, uint8_t level) {
    uint8_t hiAddr = short_addr >> 8; // HI byte of addr // NOLINT(hicpp-signed-bitwise)
    uint8_t loAddr = short_addr & 0xFF; // LO byte of addr // NOLINT(hicpp-signed-bitwise)
    uint8_t cmd[] = {
            E18_HEX_CMD_SET,
            0x05,
            E18_HEX_CMD_SET_GPIO_LEVEL,
            hiAddr,
            loAddr,
            gpio,
            level,
            E18_HEX_CMD_END_CMD
    };

    // складываем команду в список
    E18CmdItem cmdItem;
    cmdItem.dataLen = sizeof(cmd);
    cmdItem.data = new uint8_t(cmdItem.dataLen);
    memcpy(cmdItem.data, cmd, cmdItem.dataLen);
    cmdItem.cmd = E18_HEX_CMD_SET_GPIO_LEVEL;
    cmdItem.pin = gpio;
    e18_cmd_queue.push(cmdItem);
}

void E18Module::e18_cmd_get_baud_rate() {
    uint8_t cmd[] = {
            E18_HEX_CMD_GET,
            0x01,
            E18_HEX_CMD_GET_UART_BAUD_RATE,
            E18_HEX_CMD_END_CMD
    };

    // складываем команду в список
    E18CmdItem cmdItem;
    cmdItem.dataLen = sizeof(cmd);
    cmdItem.data = new uint8_t(cmdItem.dataLen);
    memcpy(cmdItem.data, cmd, cmdItem.dataLen);
    cmdItem.cmd = E18_HEX_CMD_GET_UART_BAUD_RATE;
    e18_cmd_queue.push(cmdItem);
}

void E18Module::e18_cmd_read_gpio_level(uint16_t short_addr, uint8_t gpio) {
    uint8_t hiAddr = short_addr >> 8; // HI byte of addr // NOLINT(hicpp-signed-bitwise)
    uint8_t loAddr = short_addr & 0xFF; // LO byte of addr // NOLINT(hicpp-signed-bitwise)

    uint8_t cmd[] = {
            E18_HEX_CMD_GET,
            0x04,
            E18_HEX_CMD_GET_GPIO_LEVEL,
            hiAddr,
            loAddr,
            gpio,
            E18_HEX_CMD_END_CMD
    };

    // складываем команду в список
    E18CmdItem cmdItem;
    cmdItem.dataLen = sizeof(cmd);
    cmdItem.data = new uint8_t(cmdItem.dataLen);
    memcpy(cmdItem.data, cmd, cmdItem.dataLen);
    cmdItem.cmd = E18_HEX_CMD_GET_GPIO_LEVEL;
    cmdItem.pin = gpio;
    e18_cmd_queue.push(cmdItem);
}

ssize_t E18Module::e18_read_fixed_data(uint8_t *buffer, ssize_t size) {
    int64_t count = 0;
    ssize_t readed;
    time_t currentTime = time(nullptr);

    while (count < size) {
        readed = read(coordinatorFd, &buffer[count], size - count);
        if (readed >= 0) {
            count += readed;
        }

        if (time(nullptr) - currentTime > 5) {
            // в течении 5 секунд не смогли прочитать все данные, чтото случилось с координатором/портом
            return -1;
        }

        usleep(1000);
    }

    return count;
}

void E18Module::e18_cmd_get_network_state() {
    uint8_t cmd[] = {
            E18_HEX_CMD_GET,
            0x01,
            E18_HEX_CMD_GET_NETWORK_STATE,
            E18_HEX_CMD_END_CMD
    };

    // складываем команду в список
    E18CmdItem cmdItem;
    cmdItem.dataLen = sizeof(cmd);
    cmdItem.data = new uint8_t(cmdItem.dataLen);
    memcpy(cmdItem.data, cmd, cmdItem.dataLen);
    cmdItem.cmd = E18_HEX_CMD_GET_NETWORK_STATE;
    e18_cmd_queue.push(cmdItem);
}

void E18Module::e18_cmd_get_remote_short_address(uint8_t *mac) {
    uint64_t dstAddr = strtoull((char *) mac, nullptr, 16);

    uint8_t cmd[] = {
            E18_HEX_CMD_GET,
            0x09,
            E18_HEX_CMD_GET_REMOTE_SHORT_ADDR,
            (uint8_t) (dstAddr & 0xFF), // NOLINT(hicpp-signed-bitwise)
            (uint8_t) (dstAddr >> 8 & 0xFF), // NOLINT(hicpp-signed-bitwise)
            (uint8_t) (dstAddr >> 16 & 0xFF), // NOLINT(hicpp-signed-bitwise)
            (uint8_t) (dstAddr >> 24 & 0xFF), // NOLINT(hicpp-signed-bitwise)
            (uint8_t) (dstAddr >> 32 & 0xFF), // NOLINT(hicpp-signed-bitwise)
            (uint8_t) (dstAddr >> 40 & 0xFF), // NOLINT(hicpp-signed-bitwise)
            (uint8_t) (dstAddr >> 48 & 0xFF), // NOLINT(hicpp-signed-bitwise)
            (uint8_t) (dstAddr >> 56 & 0xFF), // NOLINT(hicpp-signed-bitwise)
            E18_HEX_CMD_END_CMD
    };

    // складываем команду в список
    E18CmdItem cmdItem;
    cmdItem.dataLen = sizeof(cmd);
    cmdItem.data = new uint8_t(cmdItem.dataLen);
    memcpy(cmdItem.data, cmd, cmdItem.dataLen);
    cmdItem.cmd = E18_HEX_CMD_GET_REMOTE_SHORT_ADDR;
    cmdItem.mac = std::string((char *) mac);
    e18_cmd_queue.push(cmdItem);
}

bool E18Module::e18_store_parameter(std::string deviceMac, std::string parameterName, std::string value) {
    Device *device = findDeviceByAddress(dBase, &deviceMac);
    if (device != nullptr) {
        EntityParameter parameter(dBase);
        if (!parameter.loadParameter(device->uuid, parameterName)) {
            // параметра в базе нет, заполним поля
            uuid_t newUuid;
            uint8_t newUuidString[37] = {0};
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            parameter.uuid = (char *) newUuidString;
            parameter.entityUuid = device->uuid;
            parameter.parameter = parameterName;
        }

        parameter.value = value;
        // сохраняем в базу
        if (!parameter.save()) {
            // сообщение? о том что не смогли записать данные в базу
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s]", TAG, "Не удалось сохранить параметр!!!");
            return false;
        } else {
            return true;
        }
    }

    return false;
}

void E18Module::e18_cmd_set_network_off() {
    uint8_t cmd[] = {
            E18_HEX_CMD_SET,
            0x01,
            E18_HEX_CMD_OFF_NETWORK_AND_RESTART,
            E18_HEX_CMD_END_CMD
    };

    // складываем команду в список
    E18CmdItem cmdItem;
    cmdItem.dataLen = sizeof(cmd);
    cmdItem.data = new uint8_t(cmdItem.dataLen);
    memcpy(cmdItem.data, cmd, cmdItem.dataLen);
    cmdItem.cmd = E18_HEX_CMD_OFF_NETWORK_AND_RESTART;
    e18_cmd_queue.push(cmdItem);
}

void E18Module::e18_cmd_device_restart() {
    uint8_t cmd[] = {
            E18_HEX_CMD_SET,
            0x01,
            E18_HEX_CMD_DEVICE_RESTART,
            E18_HEX_CMD_END_CMD
    };

    // складываем команду в список
    E18CmdItem cmdItem;
    cmdItem.dataLen = sizeof(cmd);
    cmdItem.data = new uint8_t(cmdItem.dataLen);
    memcpy(cmdItem.data, cmd, cmdItem.dataLen);
    cmdItem.cmd = E18_HEX_CMD_OFF_NETWORK_AND_RESTART;
    e18_cmd_queue.push(cmdItem);
}

//----

void E18Module::storeCoordinatorDoorStatus(bool in, bool out) {
    uint8_t deviceUuid[37];
    uuid_t newUuid;
    uint8_t newUuidString[37];
    std::string measureUuid;
    time_t createTime = time(nullptr);
    Json::Reader reader;
    Json::Value obj;
    char message[1024];
    uint16_t oldValue;
    char query[1024];
    MYSQL_RES *res;
    MYSQL_ROW row;

    memset(deviceUuid, 0, 37);
    if (!findDevice(dBase, &coordinatorUuid, deviceUuid)) {
        sprintf(message, "Не удалось найти устройство с адресом %s", coordinatorUuid.data());
        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, message);
        AddDeviceRegister(dBase, (char *) coordinatorUuid.data(), message);
        return;
    }

    // найти канал по устройству sensor_channel и regIdx
    std::string channelUuid = findSChannel(dBase, deviceUuid, MTM_ZB_CHANNEL_COORD_DOOR_IDX, CHANNEL_DOOR_STATE);
    if (!channelUuid.empty()) {
        oldValue = 0;
        measureUuid = findMeasure(dBase, &channelUuid, MTM_ZB_CHANNEL_COORD_DOOR_IDX);
        if (!measureUuid.empty()) {
            sprintf(query, "SELECT * FROM data WHERE uuid='%s'", measureUuid.data());
            res = dBase->sqlexec(query);
            dBase->makeFieldsList(res);
            row = mysql_fetch_row(res);
            oldValue = (uint16_t) std::stoi(row[dBase->getFieldIndex("value")]);
            mysql_free_result(res);
            if (updateMeasureValue(dBase, (uint8_t *) measureUuid.data(), out, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG,
                                  "Не удалось обновить измерение", MTM_ZB_CHANNEL_COORD_DOOR_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            memset(newUuidString, 0, 37);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (storeMeasureValue(dBase, newUuidString, &channelUuid, out, createTime, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG,
                                  "Не удалось сохранить измерение", MTM_ZB_CHANNEL_COORD_DOOR_TITLE);
            }
        }

        if (oldValue != out) {
            sprintf(message, "Дверь %s.", out == 0 ? "закрыта" : "открыта");
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s", TAG, message);
            AddDeviceRegister(dBase, (char *) coordinatorUuid.data(), message);
        }
    }
}

void E18Module::storeCoordinatorContactorStatus(bool in, bool out) {
    uint8_t deviceUuid[37];
    uuid_t newUuid;
    uint8_t newUuidString[37];
    std::string measureUuid;
    time_t createTime = time(nullptr);
    Json::Reader reader;
    Json::Value obj;
    char message[1024];
    uint16_t oldValue;
    char query[1024];
    MYSQL_RES *res;
    MYSQL_ROW row;

    memset(deviceUuid, 0, 37);
    if (!findDevice(dBase, &coordinatorUuid, deviceUuid)) {
        sprintf(message, "Не удалось найти устройство с адресом %s", coordinatorUuid.data());
        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, message);
        AddDeviceRegister(dBase, (char *) coordinatorUuid.data(), message);
        return;
    }

    // найти канал по устройству sensor_channel и regIdx
    std::string channelUuid = findSChannel(dBase, deviceUuid, MTM_ZB_CHANNEL_COORD_CONTACTOR_IDX,
                                           CHANNEL_CONTACTOR_STATE);
    if (!channelUuid.empty()) {
        oldValue = 0;
        measureUuid = findMeasure(dBase, &channelUuid, MTM_ZB_CHANNEL_COORD_CONTACTOR_IDX);
        if (!measureUuid.empty()) {
            sprintf(query, "SELECT * FROM data WHERE uuid='%s'", measureUuid.data());
            res = dBase->sqlexec(query);
            dBase->makeFieldsList(res);
            row = mysql_fetch_row(res);
            oldValue = (uint16_t) std::stoi(row[dBase->getFieldIndex("value")]);
            mysql_free_result(res);
            if (updateMeasureValue(dBase, (uint8_t *) measureUuid.data(), out, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG,
                                  "Не удалось обновить измерение", MTM_ZB_CHANNEL_COORD_CONTACTOR_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            memset(newUuidString, 0, 37);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (storeMeasureValue(dBase, newUuidString, &channelUuid, out, createTime, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG,
                                  "Не удалось сохранить измерение", MTM_ZB_CHANNEL_COORD_CONTACTOR_TITLE);
            }
        }

        if (oldValue != out) {
            sprintf(message, "Контактор %s.", out == 0 ? "включен" : "отключен");
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s", TAG, message);
            AddDeviceRegister(dBase, (char *) coordinatorUuid.data(), message);
        }
    }
}

void E18Module::storeCoordinatorRelayStatus(bool in, bool out) {
    uint8_t deviceUuid[37];
    uuid_t newUuid;
    uint8_t newUuidString[37];
    std::string measureUuid;
    time_t createTime = time(nullptr);
    Json::Reader reader;
    Json::Value obj;
    char message[1024];
    uint16_t oldValue;
    char query[1024];
    MYSQL_RES *res;
    MYSQL_ROW row;

    memset(deviceUuid, 0, 37);
    if (!findDevice(dBase, &coordinatorUuid, deviceUuid)) {
        sprintf(message, "Не удалось найти устройство с адресом %s", coordinatorUuid.data());
        kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG, message);
        AddDeviceRegister(dBase, (char *) coordinatorUuid.data(), message);
        return;
    }

    // найти канал по устройству sensor_channel и regIdx
    std::string in1ChannelUuid = findSChannel(dBase, deviceUuid, MTM_ZB_CHANNEL_COORD_RELAY_IDX, CHANNEL_RELAY_STATE);
    if (!in1ChannelUuid.empty()) {
        oldValue = 0;
        measureUuid = findMeasure(dBase, &in1ChannelUuid, MTM_ZB_CHANNEL_COORD_RELAY_IDX);
        if (!measureUuid.empty()) {
            sprintf(query, "SELECT * FROM data WHERE uuid='%s'", measureUuid.data());
            res = dBase->sqlexec(query);
            dBase->makeFieldsList(res);
            row = mysql_fetch_row(res);
            oldValue = (uint16_t) std::stoi(row[dBase->getFieldIndex("value")]);
            mysql_free_result(res);
            if (updateMeasureValue(dBase, (uint8_t *) measureUuid.data(), out, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG,
                                  "Не удалось обновить измерение", MTM_ZB_CHANNEL_COORD_RELAY_TITLE);
            }
        } else {
            // создать новое измерение для канала
            uuid_generate(newUuid);
            memset(newUuidString, 0, 37);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            if (storeMeasureValue(dBase, newUuidString, &in1ChannelUuid, out, createTime, createTime)) {
                kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s %s", TAG,
                                  "Не удалось сохранить измерение", MTM_ZB_CHANNEL_COORD_RELAY_TITLE);
            }
        }

        if (oldValue != out) {
            sprintf(message, "Реле контактора %s.", out == 0 ? "отключено" : "включено");
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s", TAG, message);
            AddDeviceRegister(dBase, (char *) coordinatorUuid.data(), message);
        }
    }
}

void E18Module::mtmCheckLinkState() {
    std::string query;
    MYSQL_RES *res;
    MYSQL_ROW row;
    int nRows;
    int contactorState = 0; // считаем что контактор всегда включен, даже если его нет
    bool firstItem;
    std::string inParamList;
    char message[1024];
    std::string devType;

    // проверяем состояние контактора, если он включен, тогда следим за состоянием светильников
    query = "SELECT mt.* FROM device AS dt ";
    query.append("LEFT JOIN sensor_channel AS sct ON sct.deviceUuid=dt.uuid ");
    query.append("LEFT JOIN data AS mt ON mt.sensorChannelUuid=sct.uuid ");
    query.append("WHERE dt.deviceTypeUuid='" + std::string(DEVICE_TYPE_ZB_COORDINATOR_E18) + "' ");
    query.append("AND sct.measureTypeUuid='" + std::string(CHANNEL_CONTACTOR_STATE) + "' ");
    query.append("AND dt.deleted=0");
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

    // для всех светильников от которых не было пакетов с коротким адресом более linkTimeOut секунд,
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
    query.append("(dt.deviceTypeUuid='" + std::string(DEVICE_TYPE_ZB_COORDINATOR_E18)
                 + "' AND sct.measureTypeUuid='" + std::string(CHANNEL_RELAY_STATE) + "')");
    query.append(") ");
    query.append("AND dt.deviceStatusUuid='" + std::string(DEVICE_STATUS_WORK) + "' ");
    query.append("AND dt.deleted=0 ");
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
                            devType == std::string(DEVICE_TYPE_ZB_COORDINATOR_E18) ? row[dBase->getFieldIndex(
                                    "nodeAddr")] : row[dBase->getFieldIndex("devAddr")]);
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

    // для всех светильников от которых были получены пакеты с коротким адресом менее linkTimeout секунд назад,
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
    query.append("(dt.deviceTypeUuid='" + std::string(DEVICE_TYPE_ZB_COORDINATOR_E18)
                 + "' AND sct.measureTypeUuid='" + std::string(CHANNEL_RELAY_STATE) + "')");
    query.append(") ");
    query.append("AND dt.deviceStatusUuid='" + std::string(DEVICE_STATUS_NO_CONNECT) + "' ");
    query.append("AND dt.deleted=0 ");
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
                            devType == std::string(DEVICE_TYPE_ZB_COORDINATOR_E18) ? row[dBase->getFieldIndex(
                                    "nodeAddr")] : row[dBase->getFieldIndex("devAddr")]);
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
                 std::string(DEVICE_TYPE_ZB_COORDINATOR_E18) + "') ");
    query.append("AND device.deviceStatusUuid='" + std::string(DEVICE_STATUS_WORK) + "' ");
    query.append("AND dt.deleted=0");
    if (kernel->isDebug) {
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] update without measure to not link query: %s", TAG, query.data());
    }

    res = dBase->sqlexec(query.data());
    mysql_free_result(res);
}

void E18Module::lostZBCoordinator() {
    std::string query;
    MYSQL_RES *res;

    kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] ERROR write to port", TAG);
    // меняем статус координатора на DEVICE_STATUS_NO_CONNECT
    query = "UPDATE device SET deviceStatusUuid='" + std::string(DEVICE_STATUS_NO_CONNECT) +
            "', changedAt=current_timestamp() WHERE uuid='" + coordinatorUuid + "'";
    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s", TAG, query.data());
    res = dBase->sqlexec(query.data());
    mysql_free_result(res);

    // останавливаем поток с целью его последующего автоматического запуска и инициализации
    mtmZigbeeStopThread();
    AddDeviceRegister(dBase, (char *) coordinatorUuid.data(), (char *) "Ошибка записи в порт координатора");
}

void E18Module::mtmZigbeeStopThread() {
    char query[1024] = {0};
    MYSQL_RES *res;
    mtmZigbeeSetRun(false);
    // поток "остановили"
    sprintf(query, "UPDATE threads SET status=%d, changedAt=FROM_UNIXTIME(%lu) WHERE _id=%d", 0, time(nullptr),
            mtmZigBeeThreadId);
    res = dBase->sqlexec((char *) query);
    mysql_free_result(res);
}

// отправляем команду
ssize_t E18Module::send_cmd(uint8_t *buffer, size_t size) {
    ssize_t count = 0;
    ssize_t writen;

    if (kernel->isDebug) {
        char pktStr[2048] = {0};
        for (int i = 0; i < size; i++) {
            sprintf(&pktStr[i * 2], "%02X", buffer[i]);
        }

        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] RAW out packet", TAG);
        kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] %s", TAG, pktStr);
    }

    while (count < size) {
        writen = write(coordinatorFd, &buffer[count], size - count);
        if (writen >= 0) {
            count += writen;
        } else {
            // ошибка записи в порт
            return writen;
        }
    }

    return count;
}

void E18Module::log_buffer_hex(uint8_t *buffer, size_t buffer_size) {
    uint8_t message[1024];
    for (int i = 0; i < buffer_size; i++) {
        sprintf((char *) &message[i * 2], "%02X", buffer[i]);
    }

    kernel->log.ulogw(LOG_LEVEL_INFO, "[%s] MTM packet: %s", TAG, message);
}

//---

void E18Module::checkAstroEvents(time_t currentTime, double lon, double lat) {
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

            char message[1024];
            sprintf(message, "Наступил закат, включаем реле контактора.");
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s", TAG, message);
            // включаем контактор
            switchContactor(true, E18_PIN_RELAY);
            AddDeviceRegister(dBase, (char *) coordinatorUuid.data(), message);

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
            rc = send_e18_hex_cmd(E18_BROADCAST_ADDRESS, &action);
            if (rc == -1) {
                lostZBCoordinator();
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

            char message[1024];
            sprintf(message, "Наступил конец сумерек, включаем реле контактора.");
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s", TAG, message);
            // включаем контактор
            switchContactor(true, E18_PIN_RELAY);
//            AddDeviceRegister(dBase, (char *) coordinatorUuid.data(), message);

            // передаём команду "астро событие" "конец сумерек"
            action.data = (0x01 << 8 | 0x00); // NOLINT(hicpp-signed-bitwise)
            ssize_t rc = send_e18_hex_cmd(E18_BROADCAST_ADDRESS, &action);
            if (rc == -1) {
                lostZBCoordinator();
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

            char message[1024];
            sprintf(message, "Наступило начало сумерек, включаем реле контактора.");
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s", TAG, message);
            // включаем контактор
            switchContactor(true, E18_PIN_RELAY);
//            AddDeviceRegister(dBase, (char *) coordinatorUuid.data(), message);

            // передаём команду "астро событие" "начало сумерек"
            action.data = (0x03 << 8 | 0x00); // NOLINT(hicpp-signed-bitwise)
            ssize_t rc = send_e18_hex_cmd(E18_BROADCAST_ADDRESS, &action);
            if (rc == -1) {
                lostZBCoordinator();
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

            char message[1024];
            sprintf(message, "Наступил восход, выключаем реле контактора.");
            kernel->log.ulogw(LOG_LEVEL_ERROR, "[%s] %s", TAG, message);
            // выключаем контактор, гасим светильники, отправляем команду "восход"
            switchContactor(false, E18_PIN_RELAY);
            AddDeviceRegister(dBase, (char *) coordinatorUuid.data(), message);

            // строим список неисправных светильников
            makeLostLightList(dBase, kernel);
            // на всякий случай, если светильники всегда под напряжением
            switchAllLight(0);
            // передаём команду "астро событие" "восход"
            action.data = (0x00 << 8 | 0x00); // NOLINT(hicpp-signed-bitwise)
            ssize_t rc = send_e18_hex_cmd(E18_BROADCAST_ADDRESS, &action);
            if (rc == -1) {
                lostZBCoordinator();
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

void E18Module::fillTimeStruct(double time, struct tm *dtm) {
    time = time + (double) dtm->tm_gmtoff / 3600;
    dtm->tm_hour = (int) time;
    dtm->tm_min = (int) ((time - dtm->tm_hour) * 60);
    dtm->tm_sec = (int) ((((time - dtm->tm_hour) * 60) - dtm->tm_min) * 60);
}

ssize_t E18Module::sendLightLevel(uint8_t shortAddress, char *level) {
    mtm_cmd_action action = {0};
    action.header.type = MTM_CMD_TYPE_ACTION;
    action.header.protoVersion = MTM_VERSION_0;
    action.device = MTM_DEVICE_LIGHT;
    action.data = std::stoi(level);
    return send_e18_hex_cmd(shortAddress, &action);
}

