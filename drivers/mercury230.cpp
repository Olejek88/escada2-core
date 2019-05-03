//-----------------------------------------------------------------------------
#include "errors.h"
#include "main.h"
#include "mercury230.h"
#include "db.h"
#include "dbase.h"
#include <fcntl.h>
#include <sys/termios.h>
#include <pthread.h>
#include <cstdio>
#include <TypeThread.h>

//-----------------------------------------------------------------------------
static bool LoadConfig();

static MYSQL_RES *res;
static MYSQL_ROW row;
static char query[500];

//-----------------------------------------------------------------------------
void *mekDeviceThread(void *pth) {
    TypeThread thread=*((TypeThread*) pth);                      // DK identificator
    LoadConfig(thread);
    while (true) {

    }
    pthread_exit(nullptr);
}

bool LoadConfig(TypeThread pth)
{
    DBase dBase;
    if (dBase.openConnection()) {
        sprintf(query, "SELECT * FROM device WHERE thread=%d",pth.id);
        res = dBase.sqlexec(query);
        u_long nRow = mysql_num_rows(res);
/*
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
*/
        dBase.disconnect();
        return tThread;
    }
    return nullptr;
}