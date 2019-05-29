uint32_t baudrate(uint32_t  baud) {
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
    switch (type) {
        case 0:
            sprintf(types, "read currents");
        case 1:
            sprintf(types, "read hours");
        case 2:
            sprintf(types, "read days");
        case 4:
            sprintf(types, "read month");
        case 7:
            sprintf(types, "read increments");
    }
    sprintf(query, "SELECT * FROM threads WHERE id=%d", thread_id);
    printf("%s\n",query);
    res = dBase.sqlexec(query);
    if (res && (row = mysql_fetch_row(res))) {
        sprintf(query,
                "UPDATE threads SET status=%d, message='%s' WHERE id=%d", status, types, thread_id);
        res = dBase.sqlexec(query);
        printf("%s\n",query);
    }
    if (res) mysql_free_result(res);
    res = dBase.sqlexec(query);
    if (res) mysql_free_result(res);
    return true;
}

uint8_t BCD (uint8_t dat)
{
    uint8_t data=0;
    data=((dat&0xf0)>>4)*10+(dat&0xf);
    return	data;
}
