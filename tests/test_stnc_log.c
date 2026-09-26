#include <stdio.h>
#include <string.h>

#include "stnc_log.h"

int stnc_platform_get_app_directory(char *buffer,size_t buffer_size)
{
    const char *directory="build";size_t length=strlen(directory);
    if(buffer==NULL||buffer_size<=length)return 1;
    memcpy(buffer,directory,length+1u);return 0;
}

char stnc_platform_path_separator(void){return '\\';}

int main(void)
{
    char entry[STNC_LOG_ENTRY_MAX];size_t i;FILE *file;
    const char *expected[5]={"two","three","four","five","six"};

    remove("build\\stnc-core.log");
    if(stnc_log_init()!=0)return 1;
    stnc_log_info("one");
    stnc_log_warning("two");
    stnc_log_error("three");
    stnc_log_info("four");
    stnc_log_info("five");
    stnc_log_info("six");
    if(stnc_log_recent_count()!=5u)return 1;
    for(i=0u;i<5u;i++){
        memset(entry,0,sizeof(entry));
        if(stnc_log_recent_get(i,entry,sizeof(entry))!=0)return 1;
        if(strstr(entry,expected[i])==NULL)return 1;
    }
    if(stnc_log_recent_get(5u,entry,sizeof(entry))==0)return 1;
    stnc_log_shutdown();
    file=fopen("build\\stnc-core.log","r");
    if(file==NULL)return 1;
    fclose(file);
    puts("Log tests passed.");
    return 0;
}
