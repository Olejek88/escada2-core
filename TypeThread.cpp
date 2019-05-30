//
// Created by Mac on 27/04/2019.
//

#include <string>
#include <mysql/mysql.h>
#include <cstring>
#include <stdlib.h>
#include "TypeThread.h"
#include "dbase.h"
#include "errors.h"

uint32_t TypeThread::getAllThreads(TypeThread **dstPtr) {
    DBase dBase;
    static MYSQL_RES *res;
    static MYSQL_ROW row;
    static char query[500];
    struct tm tms = {0};
    if (dBase.openConnection() == OK) {
        sprintf(query, "SELECT * FROM threads");
        res = dBase.sqlexec(query);
        if (res) {
            u_long nRow = mysql_num_rows(res);
            auto *tThread = new TypeThread[nRow];
            *dstPtr = tThread;
            for (u_long r = 0; r < nRow; r++) {
                row = mysql_fetch_row(res);
                uint32_t *lengths;
                int32_t flen;
                lengths = (uint32_t *) mysql_fetch_lengths(res);
                if (row) {
                    tThread[r].id = strtol(row[0], nullptr, 10);
                    flen = lengths[1];
                    memset(tThread[r].port, 0, 15);
                    strncpy(tThread[r].port, row[1], flen);
                    flen = lengths[3];
                    memset(tThread[r].title, 0, 100);
                    strncpy(tThread[r].title, row[3], flen);
                    tThread[r].speed = static_cast<uint16_t>(strtol(row[2], nullptr, 10));
                    tThread[r].status = strtol(row[4], nullptr, 10);
                    tThread[r].work = strtol(row[5], nullptr, 10);
                    tThread[r].deviceType = strtol(row[6], nullptr, 10);
                    if (row[7]) {
                        strptime(row[7], "%Y-%m-%d %H:%M:%S", &tms);
                        tThread[r].lastDate = mktime(&tms);
                    } else
                        tThread[r].lastDate = mktime(nullptr);
                }
            }
            return nRow;
        }
        dBase.disconnect();
    }
    return 0;
}

