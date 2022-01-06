//
// Created by koputo on 02.01.22.
//

#include <dbase.h>
#include "e18.h"
#include "zigbeemtm.h"
#include <sys/queue.h>
#include <drivers/Device.h>
#include <drivers/MtmZigbee.h>
#include <drivers/EntityParameter.h>
#include <uuid/uuid.h>


// отправляем mtm команду
ssize_t send_e18_hex_cmd(int fd, uint16_t short_addr, void *mtm_cmd, Kernel *kernel) {
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

    rc = send_cmd(fd, sendBuffer, sendBufferSize, kernel);
    usleep(100000);

    return rc;
}


void e18_cmd_init_gpio(uint16_t short_addr, uint8_t line, uint8_t mode) {
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
    auto *cmdItem = (struct e18_cmd_item *) malloc(sizeof(struct e18_cmd_item));
    cmdItem->len = sizeof(cmd);
    cmdItem->data = malloc(cmdItem->len);
    cmdItem->cmd = E18_HEX_CMD_SET_GPIO_IO_STATUS;
    memcpy(cmdItem->data, (const void *) cmd, cmdItem->len);
    SLIST_INSERT_HEAD(&e18_cmd_queue_head, cmdItem, cmds);
}

void e18_cmd_set_gpio_level(uint16_t short_addr, uint8_t gpio, uint8_t level) {
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
    auto *cmdItem = (struct e18_cmd_item *) malloc(sizeof(struct e18_cmd_item));
    cmdItem->len = sizeof(cmd);
    cmdItem->data = malloc(cmdItem->len);
    cmdItem->cmd = E18_HEX_CMD_SET_GPIO_LEVEL;
    cmdItem->extra[0] = gpio;
    memcpy(cmdItem->data, (const void *) cmd, cmdItem->len);
    SLIST_INSERT_HEAD(&e18_cmd_queue_head, cmdItem, cmds);
}

void e18_cmd_get_baud_rate() {
    uint8_t cmd[] = {
            E18_HEX_CMD_GET,
            0x01,
            E18_HEX_CMD_GET_UART_BAUD_RATE,
            E18_HEX_CMD_END_CMD
    };

    // складываем команду в список
    auto *cmdItem = (struct e18_cmd_item *) malloc(sizeof(struct e18_cmd_item));
    cmdItem->len = sizeof(cmd);
    cmdItem->data = malloc(cmdItem->len);
    cmdItem->cmd = E18_HEX_CMD_GET_UART_BAUD_RATE;
    memcpy(cmdItem->data, (const void *) cmd, cmdItem->len);
    SLIST_INSERT_HEAD(&e18_cmd_queue_head, cmdItem, cmds);

}

void e18_cmd_read_gpio_level(uint16_t short_addr, uint8_t gpio) {
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
    auto *cmdItem = (struct e18_cmd_item *) malloc(sizeof(struct e18_cmd_item));
    cmdItem->len = sizeof(cmd);
    cmdItem->data = malloc(cmdItem->len);
    cmdItem->cmd = E18_HEX_CMD_GET_GPIO_LEVEL;
    cmdItem->extra[0] = gpio;
    memcpy(cmdItem->data, (const void *) cmd, cmdItem->len);
    SLIST_INSERT_HEAD(&e18_cmd_queue_head, cmdItem, cmds);
}

ssize_t e18_read_fixed_data(int coordinatorFd, uint8_t *buffer, ssize_t size) {
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

void e18_cmd_get_network_state() {
    uint8_t cmd[] = {
            E18_HEX_CMD_GET,
            0x01,
            E18_HEX_CMD_GET_NETWORK_STATE,
            E18_HEX_CMD_END_CMD
    };

    // складываем команду в список
    auto *cmdItem = (struct e18_cmd_item *) malloc(sizeof(struct e18_cmd_item));
    cmdItem->len = sizeof(cmd);
    cmdItem->data = malloc(cmdItem->len);
    cmdItem->cmd = E18_HEX_CMD_GET_NETWORK_STATE;
    memcpy(cmdItem->data, (const void *) cmd, cmdItem->len);
    SLIST_INSERT_HEAD(&e18_cmd_queue_head, cmdItem, cmds);
}

void e18_cmd_get_remote_short_address(uint8_t *mac) {
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
    auto *cmdItem = (struct e18_cmd_item *) malloc(sizeof(struct e18_cmd_item));
    cmdItem->len = sizeof(cmd);
    cmdItem->data = malloc(cmdItem->len);
    cmdItem->cmd = E18_HEX_CMD_GET_REMOTE_SHORT_ADDR;
    memset(cmdItem->extra, 0, 32);
    memcpy(cmdItem->extra, mac, 16);
    memcpy(cmdItem->data, (const void *) cmd, cmdItem->len);
    SLIST_INSERT_HEAD(&e18_cmd_queue_head, cmdItem, cmds);
}

bool e18_store_short_address(DBase *dBase, uint8_t *currentMac, uint16_t shortAddr, Kernel *kernel) {
    std::string macString((const char *) currentMac);
    char shortAddrString[5] = {0};

    sprintf(shortAddrString, "%04X", shortAddr);

    Device *device = findDeviceByAddress(dBase, &macString);
    if (device != nullptr) {
        EntityParameter parameter(dBase);
        if (!parameter.loadParameter(device->uuid, std::string("shortAddr"))) {
            // параметра в базе нет, заполним поля
            uuid_t newUuid;
            uint8_t newUuidString[37] = {0};
            uuid_generate(newUuid);
            uuid_unparse_upper(newUuid, (char *) newUuidString);
            parameter.uuid = (char *) newUuidString;
            parameter.entityUuid = device->uuid;
            parameter.parameter = "shortAddr";
        }

        parameter.value = shortAddrString;
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

void e18_cmd_set_network_off() {
    uint8_t cmd[] = {
            E18_HEX_CMD_SET,
            0x01,
            E18_HEX_CMD_OFF_NETWORK_AND_RESTART,
            E18_HEX_CMD_END_CMD
    };

    // складываем команду в список
    auto *cmdItem = (struct e18_cmd_item *) malloc(sizeof(struct e18_cmd_item));
    cmdItem->len = sizeof(cmd);
    cmdItem->data = malloc(cmdItem->len);
    cmdItem->cmd = E18_HEX_CMD_OFF_NETWORK_AND_RESTART;
    memcpy(cmdItem->data, (const void *) cmd, cmdItem->len);
    SLIST_INSERT_HEAD(&e18_cmd_queue_head, cmdItem, cmds);
}