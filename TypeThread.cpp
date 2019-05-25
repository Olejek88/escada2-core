//
// Created by Mac on 27/04/2019.
//

#include <string>
#include <mysql/mysql.h>
#include <mysql.h>
#include "TypeThread.h"
#include "dbase.h"
#include "errors.h"

TypeThread *TypeThread::getAllThreads() {
    DBase dBase;
    static MYSQL_RES *res;
    static MYSQL_ROW row;
    static char query[500];
    struct tm tms={0};
    if (dBase.openConnection()==OK) {
        sprintf(query, "SELECT * FROM threads");
        res = dBase.sqlexec(query);
        if (res) {
            u_long nRow = mysql_num_rows(res);
            auto *tThread = new TypeThread[nRow];
            for (u_long r = 0; r < nRow; r++) {
                row = mysql_fetch_row(res);
                if (row) {
                    tThread[r].id = atoi(row[0]);
                    strncpy(tThread[r].port, row[1], 20);
                    strncpy(tThread[r].title, row[3], 20);
                    tThread[r].speed = static_cast<uint16_t>(atoi(row[2]));
                    tThread[r].status = atoi(row[4]);
                    tThread[r].work = atoi(row[5]);
                    strncpy(tThread[r].deviceType, row[6], 15);
                    if (row[7]) {
                        strptime(row[7], "%Y-%m-%d %H:%M:%S", &tms);
                        tThread[r].lastDate = mktime(&tms);
                    } else
                        tThread[r].lastDate = mktime(nullptr);
                }
            }
            return tThread;
        }
        dBase.disconnect();
    }
    return nullptr;
}

