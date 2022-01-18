//
// Created by koputo on 08.01.22.
//

#ifndef ESCADA2_CORE_E18_E18MODULE_H
#define ESCADA2_CORE_E18_E18MODULE_H


#include <cstdint>
#include <zconf.h>
#include <dbase.h>
#include <kernel.h>
#include <string>
#include <map>
#include <termios.h>
#include <queue>
#include "IZigbeeModule.h"
#include "E18CmdItem.h"

class E18Module : public IZigbeeModule {

public:
// from MtmZigbee.h
    static const uint8_t MTM_ZIGBEE_FIFO = 0;
    static const uint8_t MTM_ZIGBEE_COM_PORT = 1;


// E18 defines
    const uint8_t E18_HEX_CMD_END_CMD = 0xFF;

// read commands
    static const uint8_t E18_HEX_CMD_GET = 0xFE;
    static const uint8_t E18_HEX_CMD_GET_DEVICE_TYPE = 0x01;
    static const uint8_t E18_HEX_CMD_GET_NETWORK_STATE = 0x02;
    static const uint8_t E18_HEX_CMD_GET_PANID = 0x03;
    static const uint8_t E18_HEX_CMD_GET_NETWORK_KEY = 0x04;
    static const uint8_t E18_HEX_CMD_GET_LOCAL_SHORT_ADDR = 0x05;
    static const uint8_t E18_HEX_CMD_GET_LOCAL_MAC = 0x06;
    static const uint8_t E18_HEX_CMD_GET_FATHER_SHORT_ADDR = 0x07;
    static const uint8_t E18_HEX_CMD_GET_FATHER_MAC = 0x08;
    static const uint8_t E18_HEX_CMD_GET_NETWORK_GROUP = 0x09;
    static const uint8_t E18_HEX_CMD_GET_NETWORK_CHANNEL = 0x0A;
    static const uint8_t E18_HEX_CMD_GET_TX_POWER = 0x0B;
    static const uint8_t E18_HEX_CMD_GET_UART_BAUD_RATE = 0x0C;
    static const uint8_t E18_HEX_CMD_GET_SLEEP_STATE = 0x0D;
    static const uint8_t E18_HEX_CMD_GET_RETENTION_TIME = 0x0E;
    static const uint8_t E18_HEX_CMD_GET_JOIN_PERIOD = 0x0F;
    static const uint8_t E18_HEX_CMD_GET_ALL_DEVICE_INFO = 0xFE;
    static const uint8_t E18_HEX_CMD_GET_REMOTE_SHORT_ADDR = 0x10;
    static const uint8_t E18_HEX_CMD_GET_GPIO_IO_STATUS = 0x20;
    static const uint8_t E18_HEX_CMD_GET_GPIO_LEVEL = 0x21;
    static const uint8_t E18_HEX_CMD_GET_PWM_STATUS = 0x22;
    static const uint8_t E18_HEX_CMD_GET_ADC_STATE = 0x23;

// set commands
    static const uint8_t E18_HEX_CMD_SET = 0xFD;
    static const uint8_t E18_HEX_CMD_SET_DEVICE_TYPE = E18_HEX_CMD_GET_DEVICE_TYPE;
    static const uint8_t E18_HEX_CMD_SET_PANID = E18_HEX_CMD_GET_PANID;
    static const uint8_t E18_HEX_CMD_SET_NETWORK_KEY = E18_HEX_CMD_GET_NETWORK_KEY;
    static const uint8_t E18_HEX_CMD_SET_NETWORK_GROUP = E18_HEX_CMD_GET_NETWORK_GROUP;
    static const uint8_t E18_HEX_CMD_SET_NETWORK_CHANNEL = E18_HEX_CMD_GET_NETWORK_CHANNEL;
    static const uint8_t E18_HEX_CMD_SET_TX_POWER = E18_HEX_CMD_GET_TX_POWER;
    static const uint8_t E18_HEX_CMD_SET_UART_BAUD_RATE = E18_HEX_CMD_GET_UART_BAUD_RATE;
    static const uint8_t E18_HEX_CMD_SET_SLEEP_STATE = E18_HEX_CMD_GET_SLEEP_STATE;
    static const uint8_t E18_HEX_CMD_SET_RETENTION_TIME = E18_HEX_CMD_GET_RETENTION_TIME;
    static const uint8_t E18_HEX_CMD_SET_JOIN_PERIOD = E18_HEX_CMD_GET_JOIN_PERIOD;
    static const uint8_t E18_HEX_CMD_SET_ALL_DEVICE_INFO = E18_HEX_CMD_GET_ALL_DEVICE_INFO;
    static const uint8_t E18_HEX_CMD_SET_GPIO_IO_STATUS = E18_HEX_CMD_GET_GPIO_IO_STATUS;
    static const uint8_t E18_HEX_CMD_SET_GPIO_LEVEL = E18_HEX_CMD_GET_GPIO_LEVEL;
    static const uint8_t E18_HEX_CMD_SET_PWM_STATUS = E18_HEX_CMD_GET_PWM_STATUS;

// other commands
    static const uint8_t E18_HEX_CMD_DEVICE_RESTART = 0x12;
    static const uint8_t E18_HEX_CMD_RECOVER_FACTORY = 0x13;
    static const uint8_t E18_HEX_CMD_OFF_NETWORK_AND_RESTART = 0x14;

    static const uint8_t E18_HEX_CMD = 0xFC;

    static const uint8_t E18_HEX_CMD_BROADCAST = 0x01;
    static const uint8_t E18_HEX_CMD_BROADCAST_MODE_1 = 0x01; // The message is broadcast to all devices in the entire network
    static const uint8_t E18_HEX_CMD_BROADCAST_MODE_2 = 0x02; // The message is broadcast to only devices that have enabled reception (except sleep mode)
    static const uint8_t E18_HEX_CMD_BROADCAST_MODE_3 = 0x03; // The message is broadcast to all fully functional devices (routers and coordinators)

    static const uint8_t E18_HEX_CMD_MULTICAST = 0x02;

    static const uint8_t E18_HEX_CMD_UNICAST = 0x03;
    static const uint8_t E18_HEX_CMD_UNICAST_TRANSPARENT = 0x01; // Transparent transmission mode (without carrying information)
    static const uint8_t E18_HEX_CMD_UNICAST_SHORT_ADDR = 0x02; // Short address mode (carrying information is short address)
    static const uint8_t E18_HEX_CMD_UNICAST_MAC = 0x03; // MAC address mode (carrying information is MAC address)

    static const uint8_t E18_SOF = 0xFE; // start of frame
    static const uint8_t E18_EOF = 0xFC; // end of frame
    static const uint8_t E18_ESC = 0x7D; // escape symbol

    static const uint8_t E18_ERROR = 0xF7;
    static const uint8_t E18_ERROR_SYNTAX = 0xFF;

    static const uint8_t E18_NETWORK_STATE = 0xFF;
    static const uint8_t E18_NETWORK_STATE_UP = 0xFF;
    static const uint8_t E18_NETWORK_STATE_JOIN = 0xAA;
    static const uint8_t E18_NETWORK_STATE_LOST = 0x00;

    static const uint8_t E18_GET_ANSWER = 0xFB;
    static const uint8_t E18_SET_ANSWER = 0xFA;


    static const uint8_t E18_PIN_LED = 0x08; // индикатор
    static const uint8_t E18_PIN_RELAY = 0x07; // реле
    static const uint8_t E18_PIN_DOOR = 0x06; // дверь
    static const uint8_t E18_PIN_CONTACTOR = 0x05; // контактор
    static const uint8_t E18_PIN_ADC1 = 0x01; // АЦП 1
    static const uint8_t E18_PIN_ADC0 = 0x00; // АЦП 0

    static const uint8_t E18_PIN_INPUT = 0x01;
    static const uint8_t E18_PIN_OUTPUT = 0x00;

    static const uint8_t E18_LEVEL_HI = 0x01;
    static const uint8_t E18_LEVEL_LOW = 0x00;

    static const uint16_t E18_LOCAL_DATA_ADDRESS = 0xFFFF;
    static const uint16_t E18_BROADCAST_ADDRESS = 0xFFFF;


    // это так-то общие константы для всего проекта MTM. нужно будет удалить от сюда,
    // когда перенос/создание/востановление MTMZigbee и E18Module будут завершены. Подключить через MtmZigbee.h
    const char *CHANNEL_STATUS = "E45EA488-DB97-4D38-9067-6B4E29B965F8";
    const char *CHANNEL_IN1 = "5D8A3557-6DB1-401B-9326-388F03714E48";
    const char *CHANNEL_DOOR_STATE = CHANNEL_IN1;
    const char *CHANNEL_IN2 = "066C4553-EA7A-4DB9-8E25-98192EF659A3";
    const char *CHANNEL_CONTACTOR_STATE = CHANNEL_IN2;
    const char *CHANNEL_DIGI1 = "3D597483-F547-438C-A284-85E0F2C5C480";
    const char *CHANNEL_RELAY_STATE = CHANNEL_DIGI1;
    const char *CHANNEL_RSSI = "06F2D619-CB5A-4561-82DF-4C87DF06C6FE";
    const char *CHANNEL_HOP_COUNT = "74656AFD-F536-49AE-A71A-83F0EEE9C912";
    const char *CHANNEL_CO2 = "A2E80AB5-952D-4428-8044-28F55BC104C7";

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

    static void *getModuleThread(void *context);


public:
//    E18Module();

    E18Module(Kernel *kernel, TypeThread *tth);

    ~E18Module() override;

    void *moduleThread(TypeThread *pth) override;

private:
    int coordinatorFd;
    bool mtmZigbeeStarted = false;
    uint8_t *TAG = (uint8_t *) "mtmzigbee";
//    pthread_mutex_t mtmZigbeeStopMutex;
    bool mtmZigbeeStopIssued;
    DBase *dBase;
    int32_t mtmZigBeeThreadId;
    Kernel *kernel;
    bool isSunInit;
    bool isSunSet;
    bool isTwilightEnd;
    bool isTwilightStart;
    bool isSunRise;
    std::string coordinatorUuid;
    bool isCheckCoordinatorRespond;
    TypeThread *pth;

    std::queue<E18CmdItem> e18_cmd_queue;

    void mtmZigbeePktListener();

    bool manualMode();

    ssize_t switchAllLight(uint16_t level);

    void switchContactor(bool enable, uint8_t line);

    ssize_t resetCoordinator();

    void mtmZigbeeProcessOutPacket();

    void mtmZigbeeProcessInPacket(uint8_t *pktBuff, uint32_t length);

    int32_t mtmZigbeeInit(int32_t mode, uint8_t *path, uint64_t speed);

    speed_t mtmZigbeeGetSpeed(uint64_t speed);

    bool mtmZigbeeGetRun();

    void mtmZigbeeSetRun(bool val);

    //---
    ssize_t send_e18_hex_cmd(uint16_t short_addr, void *mtm_cmd);

    void e18_cmd_init_gpio(uint16_t short_addr, uint8_t line, uint8_t mode);

    void e18_cmd_get_baud_rate();

    ssize_t e18_read_fixed_data(uint8_t *buffer, ssize_t len);

    void e18_cmd_read_gpio_level(uint16_t short_addr, uint8_t gpio);

    void e18_cmd_set_gpio_level(uint16_t short_addr, uint8_t gpio, uint8_t level);

    void e18_cmd_get_network_state();

    void e18_cmd_get_remote_short_address(uint8_t *mac);

    bool e18_store_parameter(std::string deviceMac, std::string parameterName, std::string value);

    void e18_cmd_set_network_off();

    void e18_cmd_device_restart();

    //----
    void storeCoordinatorDoorStatus(bool in, bool out);

    void storeCoordinatorContactorStatus(bool in, bool out);

    void storeCoordinatorRelayStatus(bool in, bool out);

    void mtmCheckLinkState();

    void lostZBCoordinator();

    void mtmZigbeeStopThread();

    ssize_t send_cmd(uint8_t *buffer, size_t size);

    void log_buffer_hex(uint8_t *buffer, size_t buffer_size);

    void checkAstroEvents(time_t currentTime, double lon, double lat);

    void fillTimeStruct(double time, struct tm *dtm);

    ssize_t sendLightLevel(uint8_t shortAddress, char *level);
};


#endif //ESCADA2_CORE_E18_E18MODULE_H
