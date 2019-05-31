#ifndef ESCADA_CORE_MTMZIGBEE_H
#define ESCADA_CORE_MTMZIGBEE_H

#include <cstdint>
#include <zigbeemtm.h>
#include <termios.h>

#define MTM_ZIGBEE_FIFO 0
#define MTM_ZIGBEE_COM_PORT 1

void *mtmZigbeeDeviceThread(void *device);

int32_t mtmZigbeeInit(int32_t mode, uint8_t *path, uint32_t speed);

void mtmZigbeePktListener();

speed_t mtmZigbeeGetSpeed(uint32_t speed);

bool mtmZigbeeGetRun();

void mtmZigbeeSetRun(bool val);


#endif //ESCADA_CORE_MTMZIGBEE_H
