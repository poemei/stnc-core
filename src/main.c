#include <stdio.h>

#include "stnc_command.h"
#include "stnc_core.h"
#include "stnc_log.h"

static void print_recent_log(void)
{
    size_t count,index;
    char entry[STNC_LOG_ENTRY_MAX];

    count=stnc_log_recent_count();
    if(count==0u)return;

    printf("\nRecent activity\n");
    for(index=0u;index<count;index++){
        if(stnc_log_recent_get(index,entry,sizeof(entry))==0)printf("%s\n",entry);
    }
}

int main(int argc,char **argv)
{
    int result;
    int command_mode;

    printf("STNC Core\n");
    command_mode=argc>1;
    result=stnc_core_init();

    if(result!=0){
        fprintf(stderr,"STNC Core initialization failed.\n");
        return 1;
    }

    if(command_mode){
        result=stnc_command_run(argc,argv);
        print_recent_log();
        stnc_core_shutdown();
        return result;
    }

    result=stnc_core_run();
    print_recent_log();

    if(result!=0){
        fprintf(stderr,"STNC Core runtime failed.\n");
        stnc_core_shutdown();
        return 1;
    }

    stnc_core_shutdown();
    printf("STNC Core stopped.\n");
    return 0;
}
