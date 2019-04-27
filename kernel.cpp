//----------------------------------------------------------------------------
#include "errors.h"
#include "logs.h"
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <stdio.h>
#include <stdarg.h>

#include "dbase.h"
#include "kernel.h"
#include "tinyxml2.h"

//#include <libgtop-2.0/glibtop.h>
//#include <libgtop-2.0/glibtop/cpu.h>

#include "version/version.h"

//----------------------------------------------------------------------------
int main() {
    int res = 0;
    Kernel &currentKernelInstance = Kernel::Instance();

    time_t tim;
    tim = time(&tim);

    currentKernelInstance.currenttime = localtime(&tim);
    sprintf(currentKernelInstance.logname, "logs/kernel-%04d%02d%02d_%02d%02d.log",
            currentKernelInstance.currenttime->tm_year + 1900, currentKernelInstance.currenttime->tm_mon + 1,
            currentKernelInstance.currenttime->tm_mday, currentKernelInstance.currenttime->tm_hour,
            currentKernelInstance.currenttime->tm_min);

    res = currentKernelInstance.log.init(currentKernelInstance.logname);

    if (res < 0) {
        printf("%sfatal error: log file cannot be created%s", kernel_color, nc);
        return ERROR;
    }

    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "escada kernel v.%s started", version);
    if (currentKernelInstance.init()) {
        //if(pthread_create(&thr2,NULL,dispatcher,NULL) != 0) ULOGW ("error create thread");
    } else {
        currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "%skernel finished, because initialization failed%s",
                                        kernel_color, nc);
        return OK;
    }
    //while (WorkRegim) sleep(1);
    //sleep (3600*1572);
    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "kernel finished");
    return OK;
}

//----------------------------------------------------------------------------
int Kernel::init(void) {
    DBase dbase;

    tinyxml2::XMLDocument doc;
    if (doc.LoadFile("config/escada.conf") == tinyxml2::XML_SUCCESS) {
        // TODO recode to get|set
        snprintf(dbase.driver, MAX_STR, "%s",
                 doc.FirstChildElement("database")->FirstChildElement("driver")->GetText());
        snprintf(dbase.user, MAX_STR, "%s", doc.FirstChildElement("database")->FirstChildElement("user")->GetText());
        snprintf(dbase.host, MAX_STR, "%s", doc.FirstChildElement("database")->FirstChildElement("host")->GetText());
        snprintf(dbase.pass, MAX_STR, "%s", doc.FirstChildElement("database")->FirstChildElement("pass")->GetText());
        snprintf(dbase.table, MAX_STR, "%s", doc.FirstChildElement("database")->FirstChildElement("table")->GetText());

        if (dbase.openConnection(dbase.driver, dbase.host, dbase.user, dbase.pass, dbase.table)) {
            log.ulogw(LOG_LEVEL_INFO, "database initialisation success");
        }
    } else {
        this->log.ulogw(LOG_LEVEL_ERROR, "load configuration file failed");
        return ERROR;
    }
    return OK;
}
