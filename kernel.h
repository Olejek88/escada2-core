#include "consolecolors.h"
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include "const.h"

class Kernel {
    public:
	static Kernel& Instance()
	    {
             static Kernel currentKernelInstance;
    	     return currentKernelInstance;
	    }
	Log log;
	int init ();
	char logname[MAX_FILE_LENGTH];	// maximum file length
	tm 	*currenttime;			// current system time
    private:
	    Kernel() {}
	    ~Kernel() {}

    	    Kernel(Kernel const&);
    	    Kernel& operator= (Kernel const&);
};
