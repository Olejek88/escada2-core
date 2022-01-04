#ifndef ESCADA_CORE_MTMZIGBEE_H
#define ESCADA_CORE_MTMZIGBEE_H

#include <cstdint>
#include <zigbeemtm.h>
#include <termios.h>
#include <iostream>
#include "drivers/Device.h"
#include "drivers/SensorChannel.h"

#define MTM_ZIGBEE_FIFO 0
#define MTM_ZIGBEE_COM_PORT 1

#define CHANNEL_STATUS "E45EA488-DB97-4D38-9067-6B4E29B965F8"
#define CHANNEL_IN1 "5D8A3557-6DB1-401B-9326-388F03714E48"
#define CHANNEL_DOOR_STATE CHANNEL_IN1
#define CHANNEL_IN2 "066C4553-EA7A-4DB9-8E25-98192EF659A3"
#define CHANNEL_CONTACTOR_STATE CHANNEL_IN2
#define CHANNEL_DIGI1 "3D597483-F547-438C-A284-85E0F2C5C480"
#define CHANNEL_RELAY_STATE CHANNEL_DIGI1
#define CHANNEL_RSSI "06F2D619-CB5A-4561-82DF-4C87DF06C6FE"
#define CHANNEL_HOP_COUNT "74656AFD-F536-49AE-A71A-83F0EEE9C912"
#define CHANNEL_CO2 "A2E80AB5-952D-4428-8044-28F55BC104C7"

#define MTM_ZB_CHANNEL_COORD_IN1_IDX 0
#define MTM_ZB_CHANNEL_COORD_DOOR_IDX MTM_ZB_CHANNEL_COORD_IN1_IDX
#define MTM_ZB_CHANNEL_COORD_IN1_TITLE "DOOR"
#define MTM_ZB_CHANNEL_COORD_DOOR_TITLE MTM_ZB_CHANNEL_COORD_IN1_TITLE
#define MTM_ZB_CHANNEL_COORD_IN2_IDX 0
#define MTM_ZB_CHANNEL_COORD_CONTACTOR_IDX MTM_ZB_CHANNEL_COORD_IN2_IDX
#define MTM_ZB_CHANNEL_COORD_IN2_TITLE "CONTACTOR"
#define MTM_ZB_CHANNEL_COORD_CONTACTOR_TITLE MTM_ZB_CHANNEL_COORD_IN2_TITLE
#define MTM_ZB_CHANNEL_COORD_DIGI1_IDX 0
#define MTM_ZB_CHANNEL_COORD_RELAY_IDX MTM_ZB_CHANNEL_COORD_DIGI1_IDX
#define MTM_ZB_CHANNEL_COORD_DIGI1_TITLE "RELAY"
#define MTM_ZB_CHANNEL_COORD_RELAY_TITLE MTM_ZB_CHANNEL_COORD_DIGI1_TITLE

#define MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_IDX 0
#define MTM_ZB_CHANNEL_LIGHT_TEMPERATURE_TITLE "Температура"

void *mtmZigbeeDeviceThread(void *device);

int32_t mtmZigbeeInit(int32_t mode, uint8_t *path, uint64_t speed);

void mtmZigbeePktListener(DBase *dBase, int32_t threadId);

speed_t mtmZigbeeGetSpeed(uint64_t speed);

bool mtmZigbeeGetRun();

void mtmZigbeeSetRun(bool val);

void mtmZigbeeProcessInPacket(uint8_t *pktBuff, uint32_t length);

void mtmZigbeeProcessOutPacket(int32_t threadId);

bool findDevice(DBase *dBase, uint8_t *addr, uint8_t *uuid);

std::string findSChannel(DBase *dBase, uint8_t *deviceUuid, uint8_t regIdx, const char *measureType);

void log_buffer_hex(uint8_t *buffer, size_t buffer_size);

ssize_t switchContactor(bool enable, uint8_t line);

ssize_t switchAllLight(uint16_t level);

bool
createSChannel(DBase *dBase, uint8_t *uuid, const char *channelTitle, uint8_t sensorIndex, uint8_t *deviceUuid,
               const char *channelTypeUuid, time_t createTime);

bool storeMeasureValue(DBase *dBase, uint8_t *uuid, std::string *channelUuid, double value, time_t createTime,
                       time_t changedTime);

ssize_t resetCoordinator();

void makeCoordinatorStatus(DBase *dBase, uint8_t *address, const uint8_t *packetBuffer);

void storeCoordinatorDoorStatus(DBase *dBase, std::string *address, bool in, bool out);

void storeCoordinatorContactorStatus(DBase *dBase, std::string *address, bool in, bool out);

void storeCoordinatorRelayStatus(DBase *dBase, std::string *address, bool in, bool out);

std::string findMeasure(DBase *dBase, std::string *sChannelUuid, uint8_t regIdx);

bool updateMeasureValue(DBase *dBase, uint8_t *uuid, double value, time_t changedTime);

std::string getSChannelConfig(DBase *dBase, std::string *sChannelUuid);

void checkAstroEvents(time_t currentTime, double lon, double lat, DBase *dBase, int32_t threadId);

void checkLightProgram(DBase *dBase, time_t currentTime, double lon, double lat, int32_t threadId);

ssize_t sendLightLevel(char *addrString, char *level);

void mtmZigbeeStopThread(DBase *dBase, int32_t threadId);

void mtmCheckLinkState(DBase *dBase);

void makeCoordinatorTemperature(DBase *dBase, uint8_t *address, const uint8_t *packetBuffer);

void fillTimeStruct(double time, struct tm *dtm);

Device *findDeviceByAddress(DBase *dBase, std::string *address);

SensorChannel *findSensorChannelsByDevice(DBase *dBase, std::string *deviceUuid, uint16_t *size);

void storeMeasureValueExt(DBase *dBase, SensorChannel *sc, int16_t value, bool instant);

bool updateMeasureValueExt(DBase *dBase, uint8_t *uuid, int32_t regIdx, double value, time_t changedTime);

bool insertMeasureValue(DBase *dBase, uint8_t *uuid, std::string *channelUuid, int32_t regIdx, double value,
                        time_t createTime,
                        time_t changedTime);

bool manualMode(DBase *dBase);

void lostZBCoordinator(DBase *dBase, int32_t threadId, std::string *coordUuid);

#endif //ESCADA_CORE_MTMZIGBEE_H
