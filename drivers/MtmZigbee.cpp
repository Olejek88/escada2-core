#include <unistd.h>
#include "TypeThread.h"
#include "MtmZigbee.h"
#include "kernel.h"

bool mtmZigbeeStarted = false;
uint8_t TAG[] = "mtmzigbee";

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
    int16_t id = -1;
    uint16_t speed = -1;
    uint8_t *port = nullptr;
    auto &currentKernelInstance = Kernel::Instance();
    auto *tInfo = (TypeThread *) pth;

    id = tInfo->id;
    speed = tInfo->speed;
    port = (uint8_t *) tInfo->port;

    if (!mtmZigbeeStarted) {
        mtmZigbeeStarted = true;
        currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[%s] device thread started", TAG);
        sleep(15);
    } else {
        currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "[%s] thread already started", TAG);
        return nullptr;
    }

    mtmZigbeeStarted = false;
    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "[%s] device thread ended", TAG);

    return nullptr;
}

