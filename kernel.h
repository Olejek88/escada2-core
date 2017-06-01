#include "consolecolors.h"
#include "const.h"

class Kernel {
    public:
	Kernel();
	int	init ();
	char	logname[MAX_FILE_LENGTH];	// maximum file length
	tm 	*currenttime;			// current system time
};
