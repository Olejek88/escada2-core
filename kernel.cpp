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
#include <mhash.h>
#include <tidyplatform.h>

#include "version/version.h"
#include "TypeThread.h"

DBase dBase;
void * dispatcher (void * thread_arg);
//----------------------------------------------------------------------------
int main() {
    int res = 0;
    Kernel &currentKernelInstance = Kernel::Instance();
    pthread_t dispatcher_thread;
    time_t tim;
    tim = time(&tim);

    currentKernelInstance.current_time = localtime(&tim);
    sprintf(currentKernelInstance.log_name, "logs/kernel-%04d%02d%02d_%02d%02d.log",
            currentKernelInstance.current_time->tm_year + 1900, currentKernelInstance.current_time->tm_mon + 1,
            currentKernelInstance.current_time->tm_mday, currentKernelInstance.current_time->tm_hour,
            currentKernelInstance.current_time->tm_min);

    res = currentKernelInstance.log.init(currentKernelInstance.log_name);

    if (res < 0) {
        printf("%sfatal error: log file cannot be created%s", kernel_color, nc);
        return ERROR;
    }

    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "escada kernel v.%s started", version);
    if (currentKernelInstance.init()==OK) {
        if(pthread_create (&dispatcher_thread, nullptr,dispatcher, nullptr) != 0)
            currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "error create dispatcher thread");
    } else {
        currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "%skernel finished, because initialization failed%s",
                                        kernel_color, nc);
        return OK;
    }

    // TODO здесь читаем конфигурацию пока не словим флаг остановки
    int cnt = 100;
    while (cnt) {
        sleep(1);
        cnt--;
    }
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
    glibtop_cpu cpu1;
    glibtop_cpu cpu2;
    int who = RUSAGE_SELF;
    struct rusage usage{};
    char query[300];
    double ct;

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

        glibtop_init();
        //glibtop_get_cpu (&cpu1);
        sleep (1);
        //glibtop_get_cpu (&cpu2);
        ct=100*(cpu2.user - cpu1.user + (cpu2.nice - cpu1.nice) + (cpu2.sys - cpu1.sys));
        ct/=(cpu2.total-cpu1.total);
        getrusage(who, &usage);
        sprintf (query,"INSERT INTO stat(type,cpu,mem) VALUES('1','%f','%ld')",ct, usage.ru_maxrss);
        dBase.sqlexec(query);

        break;
    }
    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "dispatcher finished");
}


