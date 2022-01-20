#ifndef ESCADA_CORE_KERNEL_H
#define ESCADA_CORE_KERNEL_H

#include "consolecolors.h"
#include <ctime>
#include <sys/time.h>
#include <sys/times.h>
#include "const.h"
#include "logs.h"
#include <stdint.h>

class Kernel {
public:
    static Kernel &Instance() {
        static Kernel currentKernelInstance;
        return currentKernelInstance;
    }

    Log log;

    int init();

    char    log_name[MAX_FILE_LENGTH];    // maximum file length
    tm      *current_time;            // current system time
    bool isDebug = false;
    uint64_t timeOffset = 0;
private:
    Kernel() {}

    ~Kernel() {}

    Kernel(Kernel const &);

    Kernel &operator=(Kernel const &);
};

#endif