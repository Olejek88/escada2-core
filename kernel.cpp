//----------------------------------------------------------------------------
#include "errors.h"
#include <ctime>
#include <sys/time.h>
#include <sys/times.h>
#include <cstdio>
#include <cstdarg>
#include <pthread.h>
#include <libgtop-2.0/glibtop.h>
#include <libgtop-2.0/glibtop/cpu.h>
#include <sys/resource.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include "dbase.h"
#include "kernel.h"
#include "drivers/ce102.h"
#include "tinyxml2.h"
#include "version/version.h"
#include "TypeThread.h"
#include "drivers/MtmZigbee.h"

bool runKernel = true;
DBase *dBase;

void *dispatcher(void *thread_arg);

void signal_callback_handler(int signum) {
    printf("Caught signal %d\n", signum);
    runKernel = false;
}

//----------------------------------------------------------------------------
int main(int argc, char *argv[]) {
    int res = 0;
    Kernel &currentKernelInstance = Kernel::Instance();
    pthread_t dispatcher_thread;
    time_t tim;
    tim = time(&tim);
    dBase = new DBase();

    signal(SIGTERM, signal_callback_handler);

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
    if (currentKernelInstance.init() == OK) {
        if (pthread_create(&dispatcher_thread, nullptr, dispatcher, nullptr) != 0)
            currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "error create dispatcher thread");
    } else {
        currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "%skernel finished, because initialization failed%s",
                                        kernel_color, nc);
        return OK;
    }

    // TODO здесь читаем конфигурацию пока не словим флаг остановки
//    int cnt = 180;
    while (runKernel) {
        sleep(1);
//        cnt--;
//        if (cnt == 0) {
//            runKernel = false;
//            currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "-> kernel run timeout...");
//        }
    }

    // ждём пока завершится dispatcher
    pthread_join(dispatcher_thread, nullptr);

    dBase->disconnect();
    delete dBase;

    mysql_library_end();

    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "kernel finished");
    return OK;
}

//----------------------------------------------------------------------------
int Kernel::init() {
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile("config/escada.conf") == tinyxml2::XML_SUCCESS) {
        if (dBase->openConnection() == 0) {
            log.ulogw(LOG_LEVEL_INFO, "database initialisation success");
        } else {
            log.ulogw(LOG_LEVEL_INFO, "database initialisation error");
            return ERROR;
        }
    } else {
        this->log.ulogw(LOG_LEVEL_ERROR, "load configuration file failed");
        return ERROR;
    }
    return OK;
}

// create thread read variable from channel
// create thread evaluate
void *dispatcher(void *thread_arg) {
    Kernel &currentKernelInstance = Kernel::Instance();
    pthread_t thr = 0;
    pthread_t zb_thr = 0;
    glibtop_cpu cpu1;
    glibtop_cpu cpu2;
    int who = RUSAGE_SELF;
//    unsigned temp = 2;
    struct rusage usage{};
    char query[300] = {0};
    double ct;
    int32_t pRc;
    uuid_t newUuid;
    char newUuidString[37];

    glibtop_init();

    while (runKernel) {
        // читаем конфигурацию
        TypeThread *typeThreads = nullptr;
        uint32_t numThreads = TypeThread::getAllThreads(&typeThreads);
        // запускаем активные потоки, те сами вызывают функции сохранения
        // периодически пишем в базу загрузку CPU
        for (int th = 0; th < numThreads; th++) {
            time_t now = time(nullptr);
            // поток походу протух
//            currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "thr [%s] %ld %ld", typeThreads[th].title, typeThreads[th].lastDate, now);
            if ((now - typeThreads[th].lastDate) > 30) {
                pRc = 0;
                if (strncasecmp("0FBACF26-31CA-4B92-BCA3-220E09A6D2D3", typeThreads[th].deviceType, 36) == 0) {
                    if (typeThreads[th].work > 0)
                        pRc = pthread_create(&thr, nullptr, ceDeviceThread, (void *) &typeThreads[th]);
                } else if (strncasecmp("CFD3C7CC-170C-4764-9A8D-10047C8B8B1D", typeThreads[th].deviceType, 36) == 0) {
                    if (typeThreads[th].work > 0)
                        pRc = pthread_create(&zb_thr, nullptr, mtmZigbeeDeviceThread, (void *) &typeThreads[th]);
                } else {
                    pRc = 0;
                }

                if (pRc != 0) {
                    currentKernelInstance.log.ulogw(LOG_LEVEL_ERROR, "error create %s thread", typeThreads[th].title);
                }
            }
        }

        sleep(10);

        // удаляем информацию о потоках после задержки, т.к. потоки могут не успеть скопировать необходимую информацию
        // из переданной им структуры
        delete[] typeThreads;

        // TODO решить как собирать статистику по загрузке и свободному месту с памятью
        glibtop_get_cpu(&cpu1);
        sleep(1);
        glibtop_get_cpu(&cpu2);
        ct = 100 * (cpu2.user - cpu1.user + (cpu2.nice - cpu1.nice) + (cpu2.sys - cpu1.sys));
        ct /= (cpu2.total - cpu1.total);
        getrusage(who, &usage);
        memset(newUuidString, 0, 37);
        uuid_generate(newUuid);
        uuid_unparse_upper(newUuid, newUuidString);
        sprintf(query, "INSERT INTO stat(uuid, type, cpu, mem) VALUES('%s', '1','%f','%ld')", newUuidString, ct,
                usage.ru_maxrss);
        dBase->sqlexec(query);
//        if (!temp--) {
//            break;
//        }
    }

    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "dispatcher try to finish");

    glibtop_close();

    ce102SetRun(false);
    if (thr != 0) {
        pthread_join(thr, nullptr);
    }

    // пример как остановить поток драйвера zigbee
    mtmZigbeeSetRun(false);
    if (zb_thr != 0) {
        pthread_join(zb_thr, nullptr);
    }

    currentKernelInstance.log.ulogw(LOG_LEVEL_INFO, "dispatcher finished");

    return nullptr;
}


