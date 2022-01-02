//
// Created by koputo on 02.01.22.
//

#ifndef ESCADA2_CORE_E18_E18_H
#define ESCADA2_CORE_E18_E18_H

#include <stdint.h>
#include <kernel.h>

#define E18_HEX_CMD_END_CMD                 0xFF

// read commands
#define E18_HEX_CMD_GET                     0xFE
#define E18_HEX_CMD_GET_DEVICE_TYPE         0x01
#define E18_HEX_CMD_GET_NETWORK_STATE       0x02
#define E18_HEX_CMD_GET_PANID               0x03
#define E18_HEX_CMD_GET_NETWORK_KEY         0x04
#define E18_HEX_CMD_GET_LOCAL_SHORT_ADDR    0x05
#define E18_HEX_CMD_GET_LOCAL_MAC           0x06
#define E18_HEX_CMD_GET_FATHER_SHORT_ADDR   0x07
#define E18_HEX_CMD_GET_FATHER_MAC          0x08
#define E18_HEX_CMD_GET_NETWORK_GROUP       0x09
#define E18_HEX_CMD_GET_NETWORK_CHANNEL     0x0A
#define E18_HEX_CMD_GET_TX_POWER            0x0B
#define E18_HEX_CMD_GET_UART_BAUD_RATE      0x0C
#define E18_HEX_CMD_GET_SLEEP_STATE         0x0D
#define E18_HEX_CMD_GET_RETENTION_TIME      0x0E
#define E18_HEX_CMD_GET_JOIN_PERIOD         0x0F
#define E18_HEX_CMD_GET_ALL_DEVICE_INFO     0xFE
#define E18_HEX_CMD_GET_REMOTE_SHORT_ADDR   0x10
#define E18_HEX_CMD_GET_GPIO_IO_STATUS      0x20
#define E18_HEX_CMD_GET_GPIO_LEVEL          0x21
#define E18_HEX_CMD_GET_PWM_STATUS          0x22
#define E18_HEX_CMD_GET_ADC_STATE           0x23

// set commands
#define E18_HEX_CMD_SET                     0xFD
#define E18_HEX_CMD_SET_DEVICE_TYPE         E18_HEX_CMD_GET_DEVICE_TYPE
#define E18_HEX_CMD_SET_PANID               E18_HEX_CMD_GET_PANID
#define E18_HEX_CMD_SET_NETWORK_KEY         E18_HEX_CMD_GET_NETWORK_KEY
#define E18_HEX_CMD_SET_NETWORK_GROUP       E18_HEX_CMD_GET_NETWORK_GROUP
#define E18_HEX_CMD_SET_NETWORK_CHANNEL     E18_HEX_CMD_GET_NETWORK_CHANNEL
#define E18_HEX_CMD_SET_TX_POWER            E18_HEX_CMD_GET_TX_POWER
#define E18_HEX_CMD_SET_UART_BAUD_RATE      E18_HEX_CMD_GET_UART_BAUD_RATE
#define E18_HEX_CMD_SET_SLEEP_STATE         E18_HEX_CMD_GET_SLEEP_STATE
#define E18_HEX_CMD_SET_RETENTION_TIME      E18_HEX_CMD_GET_RETENTION_TIME
#define E18_HEX_CMD_SET_JOIN_PERIOD         E18_HEX_CMD_GET_JOIN_PERIOD
#define E18_HEX_CMD_SET_ALL_DEVICE_INFO     E18_HEX_CMD_GET_ALL_DEVICE_INFO
#define E18_HEX_CMD_SET_GPIO_IO_STATUS      E18_HEX_CMD_GET_GPIO_IO_STATUS
#define E18_HEX_CMD_SET_GPIO_LEVEL          E18_HEX_CMD_GET_GPIO_LEVEL
#define E18_HEX_CMD_SET_PWM_STATUS          E18_HEX_CMD_GET_PWM_STATUS

// other commands
#define E18_HEX_CMD_DEVICE_RESTART 0x12
#define E18_HEX_CMD_RECOVER_FACTORY 0x13
#define E18_HEX_CMD_OFF_NETWORK_AND_RESTART 0x14

#define E18_HEX_CMD 0xFC

#define E18_HEX_CMD_BROADCAST           0x01
#define E18_HEX_CMD_BROADCAST_MODE_1    0x01 // The message is broadcast to all devices in the entire network
#define E18_HEX_CMD_BROADCAST_MODE_2    0x02 // The message is broadcast to only devices that have enabled reception (except sleep mode)
#define E18_HEX_CMD_BROADCAST_MODE_3    0x03 // The message is broadcast to all fully functional devices (routers and coordinators)

#define E18_HEX_CMD_MULTICAST 0x02

#define E18_HEX_CMD_UNICAST             0x03
#define E18_HEX_CMD_UNICAST_TRANSPARENT 0x01 // Transparent transmission mode (without carrying information)
#define E18_HEX_CMD_UNICAST_SHORT_ADDR  0x02 // Short address mode (carrying information is short address)
#define E18_HEX_CMD_UNICAST_MAC         0x03 // MAC address mode (carrying information is MAC address)

#define E18_SOF 0xFE // start of frame
#define E18_EOF 0xFC // end of frame
#define E18_ESC 0x7D // escape symbol

#define E18_ERROR           0xF7
#define E18_ERROR_SYNTAX    0xFF

#define E18_NETWORK_STATE       0xFF
#define E18_NETWORK_STATE_UP    0xFF
#define E18_NETWORK_STATE_JOIN  0xAA
#define E18_NETWORK_STATE_LOST  0x00

#define E18_GET_ANSWER 0xFB
#define E18_SET_ANSWER 0xFA


#define E18_PIN_LED         0x08 // индикатор
#define E18_PIN_RELAY       0x07 // реле
#define E18_PIN_DOOR        0x06 // дверь
#define E18_PIN_CONTACTOR   0x05 // контактор
#define E18_PIN_ADC1        0x01 // АЦП 1
#define E18_PIN_ADC0        0x00 // АЦП 0

#define E18_PIN_INPUT       0x01
#define E18_PIN_OUTPUT      0x00


ssize_t send_e18_cmd(int fd, uint16_t short_addr, void *mtm_cmd, Kernel *kernel);

ssize_t e18_cmd_init_gpio(int fd, uint16_t short_addr, uint8_t line, uint8_t mode, Kernel *kernel);


#endif //ESCADA2_CORE_E18_E18_H
