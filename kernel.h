#include "consolecolors.h"
#include <ctime>
#include <sys/time.h>
#include <sys/times.h>
#include "const.h"
#include "logs.h"

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
private:
    Kernel() {}

    ~Kernel() {}

    Kernel(Kernel const &);

    Kernel &operator=(Kernel const &);
};
