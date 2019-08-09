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

#define MTM_ZB_CHANNEL_COORD_IN1_IDX 0
#define MTM_ZB_CHANNEL_COORD_IN1_TITLE "IN1"
#define MTM_ZB_CHANNEL_COORD_IN2_IDX 1
#define MTM_ZB_CHANNEL_COORD_IN2_TITLE "IN2"
#define MTM_ZB_CHANNEL_COORD_DIGI1_IDX 2
#define MTM_ZB_CHANNEL_COORD_DIGI7_TITLE "DIGI7"

#define MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_IDX 0
#define MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_TITLE "Температура"
#define MTM_ZB_CHANNEL_LIGHT_CURRENT_IDX 1
#define MTM_ZB_CHANNEL_LIGHT_CURRENT_TITLE "Мощность"
#define MTM_ZB_CHANNEL_LIGHT_STATUS_IDX 2
#define MTM_ZB_CHANNEL_LIGHT_STATUS_TITLE "Статус"

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

bool createSChannel(uint8_t *uuid, const char *channelTitle, uint8_t sensorIndex, uint8_t *deviceUuid,
                    const char *channelTypeUuid, time_t createTime);

bool storeMeasureValue(uint8_t *uuid, uint8_t *channelUuid, double value, time_t createTime, time_t changedTime);

#endif //ESCADA_CORE_MTMZIGBEE_H
