UINT baudrate(UINT baud) {
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
    }
}

//---------------------------------------------------------------------------------------------------
BOOL UpdateThreads(DBase dbase, int thread_id, uint8_t global, uint8_t start, uint8_t curr,
                   char *curr_adr, uint8_t status, uint8_t type, char *data_time) {
    MYSQL_RES *res;
    MYSQL_ROW row;
    CHAR query[500], types[40];
    switch (type) {
        case 0:
            sprintf(types, "currents");
        case 1:
            sprintf(types, "hours");
        case 2:
            sprintf(types, "days");
        case 4:
            sprintf(types, "month");
        case 7:
            sprintf(types, "increments");
    }
    sprintf(query, "SELECT * FROM threads WHERE thread=%d", thread_id);
    res = dbase.sqlexec(query);
    if (row = mysql_fetch_row(res)) {
        if (strlen(datatime) < 5)
            sprintf(query,
                    "UPDATE threads SET global=%d, start=%d, curr=%d, curr_adr=%d, status=%d, type='%s' WHERE thread=%d",
                    global, start, curr, curr_adr, status, types, thread_id);
        else
            sprintf(query,
                    "UPDATE threads SET global=%d, start=%d, curr=%d, curr_adr=%d, status=%d, type='%s', ctime='%s' WHERE thread=%d",
                    thread_id, global, start, curr, curr_adr, status, types, ctime);
    } else {
        sprintf(query,
                "INSERT INTO threads SET global=%d, start=%d, curr=%d, curr_adr=%d, status=%d, type='%s',thread=%d",
                global, start, curr, curr_adr, status, types, thread_id);
    }
    if (debug > 4) ULOGW("[db] %s", query);
    if (res) mysql_free_result(res);
    res = dbase.sqlexec(query);
    if (res) mysql_free_result(res);
    return true;
}

uint8_t BCD (uint8_t dat)
{
    uint8_t data=0;
    data=((dat&0xf0)>>4)*10+(dat&0xf);
    return	data;
}
