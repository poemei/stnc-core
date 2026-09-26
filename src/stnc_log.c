#include <stdio.h>
#include <time.h>

#include "stnc_log.h"

#define STNC_LOG_PATH "stnc-core.log"

static int log_initialized = 0;
static FILE *log_file = NULL;

static void stnc_log_write(const char *level,const char *message)
{
    time_t now;struct tm local_time;
    if(!log_initialized||log_file==NULL||level==NULL||message==NULL)return;
    now=time(NULL);
#if defined(_WIN32)
    if(localtime_s(&local_time,&now)==0)
#else
    {struct tm *value=localtime(&now);if(value==NULL)return;local_time=*value;
#endif
    fprintf(log_file,"[%04d-%02d-%02d %02d:%02d:%02d] [%s] %s\n",
        local_time.tm_year+1900,local_time.tm_mon+1,local_time.tm_mday,
        local_time.tm_hour,local_time.tm_min,local_time.tm_sec,level,message);
    fflush(log_file);
#if !defined(_WIN32)
    }
#endif
}

int stnc_log_init(void)
{
    if(log_initialized)return 1;
    log_file=fopen(STNC_LOG_PATH,"a");
    if(log_file==NULL)return 1;
    log_initialized=1;
    return 0;
}

void stnc_log_info(const char *message)
{
    stnc_log_write("INFO",message);
}

void stnc_log_warning(const char *message)
{
    stnc_log_write("WARNING",message);
}

void stnc_log_error(const char *message)
{
    stnc_log_write("ERROR",message);
}

void stnc_log_shutdown(void)
{
    if(!log_initialized)return;
    if(log_file!=NULL){fflush(log_file);fclose(log_file);log_file=NULL;}
    log_initialized=0;
}
