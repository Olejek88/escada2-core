#include <stdio.h>
#include <termios.h>
#include <time.h>
#include "function.h"

uint32_t baudrate(uint32_t baud) {
    switch (baud) {
        case 300:
            return B300;
        case 600:
            return B600;
        case 1200:
            return B1200;
        case 2400:
            return B2400;
        case 4800:
            return B4800;
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
            return B9600;
    }
}

//---------------------------------------------------------------------------------------------------
bool UpdateThreads(DBase dBase, int thread_id, uint8_t type, uint8_t status) {
    MYSQL_RES *res;
    MYSQL_ROW row;
    char query[500], types[40];
    time_t current_time = time(nullptr);

    switch (type) {
        case 0:
            sprintf(types, "read currents");
            break;
        case 1:
            sprintf(types, "read hours");
            break;
        case 2:
            sprintf(types, "read days");
            break;
        case 4:
            sprintf(types, "read month");
            break;
        case 7:
            sprintf(types, "read increments");
            break;
        default:
            sprintf(types, "none");
            break;
    }

    sprintf(query, "SELECT * FROM threads WHERE _id=%d", thread_id);
    printf("%s\n", query);
    res = dBase.sqlexec(query);
    if (res && (row = mysql_fetch_row(res))) {
        sprintf(query,
                "UPDATE threads SET status=%d, message='%s', c_time=FROM_UNIXTIME(%lu) WHERE _id=%d", status, types, current_time, thread_id);
        res = dBase.sqlexec(query);
        printf("%s\n", query);
    }

    if (res) {
        mysql_free_result(res);
    }

    res = dBase.sqlexec(query);
    if (res) {
        mysql_free_result(res);
    }

    return true;
}

uint8_t BCD(uint8_t dat) {
    uint8_t data = 0;
    data = ((dat & 0xf0) >> 4) * 10 + (dat & 0xf);
    return data;
}

