#ifndef ESCADA_CORE_MTMZIGBEE_H
#define ESCADA_CORE_MTMZIGBEE_H

#include <cstdint>
#include <zigbeemtm.h>
#include <termios.h>

#ifndef DEBUG
#define DEBUG false
#endif

#define MTM_ZIGBEE_FIFO 0
#define MTM_ZIGBEE_COM_PORT 1

void *mtmZigbeeDeviceThread(void *device);

int32_t mtmZigbeeInit(int32_t mode, uint8_t *path, uint32_t speed);

void mtmZigbeePktListener(int32_t threadId);

speed_t mtmZigbeeGetSpeed(uint32_t speed);

bool mtmZigbeeGetRun();

void mtmZigbeeSetRun(bool val);

void mtmZigbeeProcessInPacket(uint8_t *pktBuff, uint32_t len);

void mtmZigbeeProcessOutPacket();

bool findDevice(uint8_t *addr, uint8_t *uuid);

bool findSChannel(uint8_t *deviceUuid, uint8_t regIdx, uint8_t *sChannelUuid);

void log_buffer_hex(uint8_t *buffer, size_t buffer_size);

ssize_t switchContactor(bool enable, uint8_t line);

ssize_t switchAllLight(uint16_t level);

#endif //ESCADA_CORE_MTMZIGBEE_H
