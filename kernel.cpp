//----------------------------------------------------------------------------
#include "errors.h"
#include "logs.h"
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <stdio.h>
#include <stdarg.h>

#include "kernel.h"

//#include <libgtop-2.0/glibtop.h>
//#include <libgtop-2.0/glibtop/cpu.h>

#include "version/version.h"
//----------------------------------------------------------------------------
Kernel::Kernel ()
{
}

int main ()
{
 Kernel currentKernelInstance;
 Log log;
 int res=0;
 
 time_t tim;
 tim=time(&tim);

 currentKernelInstance.currenttime=localtime(&tim);
 sprintf (currentKernelInstance.logname,"logs/kernel-%04d%02d%02d_%02d%02d.log",currentKernelInstance.currenttime->tm_year+1900,currentKernelInstance.currenttime->tm_mon+1,currentKernelInstance.currenttime->tm_mday,currentKernelInstance.currenttime->tm_hour,currentKernelInstance.currenttime->tm_min);

 res = log.init(currentKernelInstance.logname);

 if (res<0) {
    printf ("%sfatal error: log file cannot be created%s",kernel_color,nc);
 }
 
 log.ulogw ("escada kernel v.%s started",version);
 if (currentKernelInstance.init())
    {
     //if(pthread_create(&thr2,NULL,dispatcher,NULL) != 0) ULOGW ("error create thread");
    }
 else
    {
     log.ulogw ("%skernel finished, because initialization failed%s",kernel_color,nc);
     return 0;
    }    
 //while (WorkRegim) sleep(1);
 //sleep (3600*1572);
 log.ulogw ("kernel finished");
 return 0;
}
//----------------------------------------------------------------------------
int Kernel::init (void)
{
 return 0;
}
