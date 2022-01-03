//
// Created by koputo on 02.01.22.
//

#include "e18.h"
#include "zigbeemtm.h"


// отправляем mtm команду
ssize_t send_e18_hex_cmd(int fd, uint16_t short_addr, void *mtm_cmd, Kernel *kernel) {
    uint8_t buffer[1024];
    uint8_t sendBuffer[1024];
    uint8_t bufferSize;
    uint8_t sendBufferSize = 0;
    ssize_t rc;
    uint8_t cmdType = ((mtm_cmd_header *) mtm_cmd)->type;

    bufferSize = get_mtm_command_size(cmdType);
    // формируем данные для отправки
    sendBuffer[sendBufferSize++] = E18_HEX_CMD;
    sendBuffer[sendBufferSize++] = 0; // размер передаваемых данных нам пока не известен
    if (short_addr == 0xFFFF) {
        sendBuffer[sendBufferSize++] = E18_HEX_CMD_BROADCAST;
        sendBuffer[sendBufferSize++] = E18_HEX_CMD_BROADCAST_MODE_1;
    } else {
        sendBuffer[sendBufferSize++] = E18_HEX_CMD_UNICAST;
        sendBuffer[sendBufferSize++] = E18_HEX_CMD_UNICAST_TRANSPARENT;
        sendBuffer[sendBufferSize++] = short_addr >> 8; // HI byte of addr // NOLINT(hicpp-signed-bitwise)
        sendBuffer[sendBufferSize++] = short_addr & 0xFF; // LO byte of addr // NOLINT(hicpp-signed-bitwise)
    }

    sendBuffer[sendBufferSize++] = E18_SOF;

    zigbeemtm_get_mtm_cmd_data(cmdType, mtm_cmd, buffer);
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


ssize_t e18_cmd_init_gpio(int fd, uint16_t short_addr, uint8_t line, uint8_t mode, Kernel *kernel) {
    ssize_t rc;
    uint8_t hiAddr = short_addr >> 8; // HI byte of addr // NOLINT(hicpp-signed-bitwise)
    uint8_t loAddr = short_addr & 0xFF; // LO byte of addr // NOLINT(hicpp-signed-bitwise)
    uint8_t gpioInitCmd[] = {
            E18_HEX_CMD_SET,
            0x05,
            E18_HEX_CMD_SET_GPIO_IO_STATUS,
            hiAddr,
            loAddr,
            line,
            mode,
            E18_HEX_CMD_END_CMD
    };

    rc = send_cmd(fd, gpioInitCmd, sizeof(gpioInitCmd), kernel);
    usleep(100000);

    return rc;
}

ssize_t e18_cmd_get_baud_rate(int fd, Kernel *kernel) {
    ssize_t rc;
    uint8_t getBaudCmd[] = {
            E18_HEX_CMD_GET,
            0x01,
            E18_HEX_CMD_GET_UART_BAUD_RATE,
            E18_HEX_CMD_END_CMD
    };

    rc = send_cmd(fd, getBaudCmd, sizeof(getBaudCmd), kernel);
    usleep(100000);

    return rc;
}

ssize_t e18_read_fixed_data(int coordinatorFd, uint8_t *buffer, ssize_t size) {
    int64_t count = 0;
    ssize_t readed;
    time_t currentTime = time(nullptr);

    while (count < size) {
        readed = read(coordinatorFd, &buffer[count], size - count);
        if (readed >= 0) {
            count += readed;
        } else {
            return readed;
        }

        if (time(nullptr) - currentTime > 5) {
            // в течении 5 секунд не смогли прочитать все данные, чтото случилось с координатором/портом
            return -1;
        }

        usleep(1000);
    }

    return count;
}
