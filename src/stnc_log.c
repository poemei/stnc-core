#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#endif

#include "stnc_log.h"
#include "stnc_platform.h"

#define STNC_LOG_FILENAME "stnc-core.log"
#define STNC_LOG_PATH_MAX 1024u

static int log_initialized = 0;
static FILE *log_file = NULL;
static char recent_entries[STNC_LOG_RECENT_CAPACITY][STNC_LOG_ENTRY_MAX];
static size_t recent_count = 0u;
static size_t recent_next = 0u;

static void stnc_log_remember(const char *entry)
{
    size_t length;
    if(entry==NULL)return;
    length=strlen(entry);
    if(length>=STNC_LOG_ENTRY_MAX)length=STNC_LOG_ENTRY_MAX-1u;
    memcpy(recent_entries[recent_next],entry,length);
    recent_entries[recent_next][length]='\0';
    recent_next=(recent_next+1u)%STNC_LOG_RECENT_CAPACITY;
    if(recent_count<STNC_LOG_RECENT_CAPACITY)recent_count++;
}

static void stnc_log_write(const char *level,const char *message)
{
    time_t now;struct tm local_time;char entry[STNC_LOG_ENTRY_MAX];int written;
    if(!log_initialized||log_file==NULL||level==NULL||message==NULL)return;
    now=time(NULL);
#if defined(_WIN32)
    if(localtime_s(&local_time,&now)!=0)return;
#else
    {struct tm *value=localtime(&now);if(value==NULL)return;local_time=*value;}
#endif
    written=snprintf(entry,sizeof(entry),"[%04d-%02d-%02d %02d:%02d:%02d] [%s] %s",
        local_time.tm_year+1900,local_time.tm_mon+1,local_time.tm_mday,
        local_time.tm_hour,local_time.tm_min,local_time.tm_sec,level,message);
    if(written<0)return;
    entry[sizeof(entry)-1u]='\0';
    fprintf(log_file,"%s\n",entry);
    fflush(log_file);
    stnc_log_remember(entry);
}

int stnc_log_init(void)
{
    char directory[STNC_LOG_PATH_MAX];char path[STNC_LOG_PATH_MAX];int written;
    if(log_initialized)return 1;
    memset(directory,0,sizeof(directory));memset(path,0,sizeof(path));
    if(stnc_platform_get_app_directory(directory,sizeof(directory))!=0)return 1;
    written=snprintf(path,sizeof(path),"%s%c%s",directory,stnc_platform_path_separator(),STNC_LOG_FILENAME);
    if(written<0||(size_t)written>=sizeof(path))return 1;
#if defined(_WIN32)
    {
        int fd=-1;
        if(_sopen_s(&fd,path,_O_WRONLY|_O_CREAT|_O_APPEND|_O_TEXT,_SH_DENYNO,_S_IREAD|_S_IWRITE)!=0||fd<0)return 1;
        log_file=_fdopen(fd,"a");
        if(log_file==NULL){_close(fd);return 1;}
    }
#else
    log_file=fopen(path,"a");
    if(log_file==NULL)return 1;
#endif
    memset(recent_entries,0,sizeof(recent_entries));recent_count=0u;recent_next=0u;
    log_initialized=1;
    return 0;
}

void stnc_log_info(const char *message){stnc_log_write("INFO",message);}
void stnc_log_warning(const char *message){stnc_log_write("WARNING",message);}
void stnc_log_error(const char *message){stnc_log_write("ERROR",message);}
size_t stnc_log_recent_count(void){return recent_count;}

int stnc_log_recent_get(size_t index,char *entry,size_t capacity)
{
    size_t oldest,slot,length;
    if(entry==NULL||capacity==0u||index>=recent_count)return 1;
    oldest=(recent_count<STNC_LOG_RECENT_CAPACITY)?0u:recent_next;
    slot=(oldest+index)%STNC_LOG_RECENT_CAPACITY;
    length=strlen(recent_entries[slot]);
    if(length+1u>capacity)return 1;
    memcpy(entry,recent_entries[slot],length+1u);
    return 0;
}

void stnc_log_shutdown(void)
{
    if(!log_initialized)return;
    if(log_file!=NULL){fflush(log_file);fclose(log_file);log_file=NULL;}
    log_initialized=0;
}
