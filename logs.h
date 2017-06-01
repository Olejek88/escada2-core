#include "const.h"

#define	LOG_LEVEL_NONE		0
#define	LOG_LEVEL_ERROR		1
#define	LOG_LEVEL_WARNINGS	2
#define	LOG_LEVEL_INFO		3
#define	LOG_LEVEL_DEBUG		4

#define MODE_SILENT	1

class Log {
    public:
	char logname[MAX_FILE_LENGTH];
	int  mode;
	int log_level;
	
	Log();
	int init(char* filename);
	int setLevel (int loglevel);
	int getLevel ();
	void ulogw (const char* string, ...);
};
