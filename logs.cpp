#include "logs.h"
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

int Log::setLevel (int loglevel) {
 if (loglevel>=LOG_LEVEL_NONE && loglevel<=LOG_LEVEL_DEBUG) {
      log_level = loglevel; 
      return log_level;
     }
 else {
    printf ("error changing log level\n");
    this->ulogw (LOG_LEVEL_WARNINGS,"error changing log level");
    return -1;
 } 
 
}

int Log::getLevel () {
 return LOG_LEVEL_DEBUG;
}


Log::Log ()
{
 log_level = LOG_LEVEL_DEBUG; 
}

int Log::init (char* kernellog)
{
 struct stat st = {0};
 FILE *logfile; 
 if (stat("logs", &st) == -1) {
    mkdir("logs", 0700);
 }
 logfile = fopen(kernellog,"w"); 
 if (logfile>0) {
      snprintf (this->logname, MAX_FILE_LENGTH, "%s",kernellog);
      fclose (logfile);
     }
 else {
      printf ("error creating logs, finished.....");
      return -1;
     }
 log_level = LOG_LEVEL_DEBUG; 
 mode = 0;
 return 0;
}

void Log::ulogw (int loglevel, const char* string, ...)
{
 if (loglevel>log_level) return;
  
 char 	buf[500];
 FILE 	*Log;
 struct tm 	*ttime;

 time_t tim;
 Log = fopen(this->logname,"a");

 tim=time(&tim);
 ttime=localtime(&tim);
 sprintf (buf,"%02d-%02d %02d:%02d:%02d ",ttime->tm_mon+1,ttime->tm_mday,ttime->tm_hour,ttime->tm_min,ttime->tm_sec);
 fprintf (Log, "%s",buf); 

 va_list arg; va_start(arg, string);
 vsnprintf(buf,sizeof (buf), string, arg);
 fprintf (Log, "%s", buf); 
 if (this->mode!=MODE_SILENT)
     printf ("%s",buf);
 va_end(arg);
 fprintf (Log,"\n"); 
 printf ("\n");
 fclose (Log);
}
