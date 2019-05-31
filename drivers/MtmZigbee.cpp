#include <unistd.h>
#include <cstdio>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <cstring>
#include "TypeThread.h"
#include "MtmZigbee.h"
#include "kernel.h"

int coordinatorFd;
bool mtmZigbeeStarted = false;
uint8_t TAG[] = "mtmzigbee";
pthread_mutex_t mtmZigbeeStopMutex;
bool mtmZigbeeStopIssued;

/*
Алгоритм работы zigbee модуля для escada2 core:

escada запускает основную функцию модуля в отдельном потоке, передаёт параметры.
модуль по параметрам открывает порт, если успешно, начинает бесконечный процесс прослушивания порта для получения пакетов.
Если пакетов нет, проверяет список разобранных пакетов, обрабатывает их согласно их типу.
Если есть пакет AF_INCOMING_MESSAGE, разбирает его, сохраняет в базу данных со статусом "не отправлено".
Если пакетов нет, проверяет наличие в таблице базы данных поступивших команд для светильников.
Если команды есть, рассылает их.

Получением/отправкой пакетов полученных с/на сервер занимается отдельный сервис, который полчает/отправляет сообщения по MQTT.

 */

void *mtmZigbeeDeviceThread(void *pth) {
//    int16_t id = -1;
    uint16_t speed;
    uint8_t *port;
    auto &currentKernelInstance = Kernel::Instance();
    auto *tInfo = (TypeThread *) pth;

//    id = tInfo->id;
    speed = tInfo->speed;
    port = (uint8_t *) tInfo->port;

    if (!mtmZigbeeStarted) {
        mtmZigbeeStarted = true;
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[%s] device thread started", TAG);
        // TODO: как будет всё отлажено вместо MTM_ZIGBEE_FIFO использовать MTM_ZIGBEE_COM_PORT
        if (mtmZigbeeInit(MTM_ZIGBEE_FIFO, port, speed) != 0) {
            // завершаем поток
            return nullptr;
        } else {
            // запускаем цикл разбора паетов
            mtmZigbeePktListener();
        }
    } else {
        currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "[%s] thread already started", TAG);
        return nullptr;
    }

    mtmZigbeeStarted = false;
    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[%s] device thread ended", TAG);

    return nullptr;
}


void mtmZigbeePktListener() {
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

    struct zb_pkt_item {
//        zigbee_frame frame;
        void *pkt;
        SLIST_ENTRY(zb_pkt_item)
                items;
    };
    struct zb_queue *zb_queue_ptr;
    SLIST_HEAD(zb_queue, zb_pkt_item)
            zb_queue_head = SLIST_HEAD_INITIALIZER(zb_queue_head);
    SLIST_INIT(&zb_queue_head);
    zb_queue_ptr = (struct zb_queue *) (&zb_queue_head);
    struct zb_pkt_item *zb_item;

    mtmZigbeeSetRun(true);

    while (run) {
        count = read(coordinatorFd, &data, 1);
        if (count > 0) {
            printf("data: %02X\n", data);

            // начинаем разбор
            if (!isSof && data == SOF) {
                i = 0;
                isSof = true;
                seek[i++] = data;
                printf("found SOF\n");
            } else if (!isFrameLen) {
                isFrameLen = true;
                seek[i++] = frameLen = data;
                printf("found frame len\n");
            } else if (!isCommand) {
                commandByteCount++;
                seek[i++] = data;
                if (commandByteCount == 2) {
                    commandByteCount = 0;
                    isCommand = true;
                    printf("found command\n");
                }
            } else if (!isFrameData && frameDataByteCount < frameLen) {
                seek[i++] = data;
                frameDataByteCount++;
                if (frameDataByteCount == frameLen) {
                    isFrameData = true;
                    frameDataByteCount = 0;
                    printf("found frame data\n");
                }
            } else {
                // нашли контрольную сумму
                seek[i++] = data;
                printf("found FCS\n");

                // пакет вроде как разобран
                // нужно проверить контрольную сумму фрейма
                fcs = compute_fcs(seek, i);
                if (fcs == seek[i - 1]) {
                    printf("frame good\n");
                    // складываем полученный пакет в список
                    zb_item = (struct zb_pkt_item *) malloc(sizeof(struct zb_pkt_item));
                    zb_item->pkt = malloc(i - 1);
                    memcpy(zb_item->pkt, seek, i - 1);
                    SLIST_INSERT_HEAD(zb_queue_ptr, zb_item, items);
                } else {
                    printf("frame bad\n");
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
//            printf("sleep...\n");
            // есть свободное время, разобираем список полученных пакетов
            while (!SLIST_EMPTY(zb_queue_ptr)) {
                printf("processing zb packet...\n");
                zb_item = SLIST_FIRST(zb_queue_ptr);
                // TODO: реализовать обработку пакета
                // process(zb_item);
                SLIST_REMOVE_HEAD(zb_queue_ptr, items);
                free(zb_item->pkt);
                free(zb_item);
            }

            run = mtmZigbeeGetRun();

            usleep(10000);
        }
    }
}

int32_t mtmZigbeeInit(int32_t mode, uint8_t *path, uint32_t speed) {
    struct termios serialPortSettings{};

    if (mode == MTM_ZIGBEE_FIFO) {
        // создаём fifo для тестов
        const char *fifo = "/tmp/zbfifo";
        coordinatorFd = open(fifo, O_NONBLOCK | O_RDWR | O_NOCTTY); // NOLINT(hicpp-signed-bitwise)
        if (coordinatorFd == -1) {
            // пробуем создать
            coordinatorFd = mkfifo((char *) path, 0666);
            if (coordinatorFd == -1) {
                printf("FIFO can not make!!!\n");
                return -1;
            } else {
                close(coordinatorFd);
                coordinatorFd = open((char *) path, O_NONBLOCK | O_RDWR | O_NOCTTY); // NOLINT(hicpp-signed-bitwise)
                if (coordinatorFd == -1) {
                    printf("FIFO can not open!!!\n");
                    return -1;
                }
            }
        }
    } else {
        if (access((char *) path, F_OK) != -1) {
            printf("init %s\n", path);
        } else {
            printf("%s path not exists\n", path);
            exit(-2);
        }

        // открываем порт
        coordinatorFd = open((char *) path, O_NONBLOCK | O_RDWR | O_NOCTTY); // NOLINT(hicpp-signed-bitwise)
        if (coordinatorFd == -1) {
            printf("can not open file\n");
            exit(-3);
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
            printf("ERROR ! in Setting attributes\n");
            exit(-4);
        } else {
            printf("BaudRate = 38400\nStopBits = 1\nParity = none\n");
        }

        tcflush(coordinatorFd, TCIFLUSH);   /* Discards old data in the rx buffer            */
    }


    send_zb_cmd(coordinatorFd, ZB_SYSTEM_RESET, nullptr);

    sleep(1);

    // регистрируем свою конечную точку
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
    printf("rc=%i\n", rc);

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