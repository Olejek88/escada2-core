//----------------------------------------------------------------------------
#include "errors.h"
#include "logs.h"
#include <ctime>
#include <sys/time.h>
#include <sys/times.h>
#include <cstdio>
#include <cstdarg>
#include <pthread.h>

#include "dbase.h"
#include "kernel.h"
#include "drivers/mercury230.h"
#include "tinyxml2.h"

#include <libgtop-2.0/glibtop.h>
#include <libgtop-2.0/glibtop/cpu.h>

#include "version/version.h"
#include "TypeThread.h"

DBase dBase;
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
    if (currentKernelInstance.init()==OK) {
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
int Kernel::init() {
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile("config/escada.conf") == tinyxml2::XML_SUCCESS) {
        if (dBase.openConnection()) {
            log.ulogw(LOG_LEVEL_INFO, "database initialisation success");
        }
    } else {
        this->log.ulogw(LOG_LEVEL_ERROR, "load configuration file failed");
        return ERROR;
    }
    return OK;
}

// create thread read variable from channal
// create thread evaluate
void * dispatcher (void * thread_arg)
{
    Kernel &currentKernelInstance = Kernel::Instance();
    pthread_t thr;
    while (true) {
        // читаем конфигурацию
        TypeThread *typeThreads;
        typeThreads = TypeThread::getAllThreads();

        // запускаем активные потоки, те сами вызывают функции сохранения
        // периодически пишем в базу загрузку CPU
        for (int th = 0; th < (sizeof(typeThreads) / sizeof(*typeThreads)); th++) {
            time_t now = time(nullptr);
            // поток похожу протух
            if (difftime(typeThreads[th].lastDate,now)>60) {
                if (pthread_create(&thr, nullptr, mekDeviceThread, nullptr) != 0)
                    currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "error create %s thread", typeThreads[th].title);
            }

        }
/*
        glibtop_init();
        glibtop_get_cpu (&cpu1);
        sleep (1);
        glibtop_get_cpu (&cpu2);
        ct=100*(((unsigned long)cpu2.user-(unsigned long)cpu1.user)+((unsigned long)cpu2.nice-(unsigned long)cpu1.nice)+((unsigned long)cpu2.sys-(unsigned long)cpu1.sys));
        ct/=((unsigned long)cpu2.total-(unsigned long)cpu1.total);
        getrusage(who,&usage);
        ULOGW ("%ld %ld %ld %ld %ld %ld %ld %ld",(unsigned long)cpu2.user,(unsigned long)cpu1.user,(unsigned long)cpu2.nice,(unsigned long)cpu1.nice,(unsigned long)cpu2.sys,(unsigned long)cpu1.sys,(unsigned long)cpu2.total,(unsigned long)cpu1.total);
        sprintf (querys,"INSERT INTO stat(type,cpu,mem) VALUES('1','%f','%d')",ct,usage.ru_maxrss);
        ULOGW ("%s",querys);
        dbase.sqlexec(querys);
        sprintf(querys, "UPDATE info SET date=NULL");
        res_ = dbase.sqlexec(querys);
*/
        break;
    }
    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "dispatcher finished");
}


