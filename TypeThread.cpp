//
// Created by Mac on 27/04/2019.
//

#include <string>
#include "TypeThread.h"
#include "dbase.h"

static MYSQL_RES *res;
static MYSQL_ROW row;
static char query[500];

static TypeThread *TypeThread::getAllThreads() {
    DBase dBase;
    if (dBase.openConnection()) {
        sprintf(query, "SELECT * FROM dev_thread");
        res = dBase.sqlexec(query);
        u_long nRow = mysql_num_rows(res);
        auto *tThread =  new TypeThread[nRow];
        for (u_long r = 0; r < nRow; r++) {
            row = mysql_fetch_row(res);
            if (row) {
                tThread[r].id = atoi(row[0]);
                tThread[r].title = row[3];
                tThread[r].port = row[1];
                tThread[r].speed = atoi(row[2]);
                tThread[r].deviceType = atoi(row[6]);
                tThread[r].lastDate = (std::time_t)atoi(row[7]);
            }
        }
        dBase.disconnect();
        return tThread;
    }
    return nullptr;
}

