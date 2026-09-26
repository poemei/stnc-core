#include <stdio.h>
#include <string.h>

#include "stnc_command.h"
#include "stnc_core.h"
#include "stnc_log.h"
#include "stnc_gui.h"

/* Keep the desktop frontend in its own translation source while preserving
 * the established command-line build list. */
#include "../platforms/windows/gui_bitcoin.c"

int main(int argc,char **argv)
{
    int result;
    int command_mode;

    if(argc==1||(argc==2&&strcmp(argv[1],"gui")==0))return stnc_gui_bitcoin_run();
    printf("STNC Core\n");
    if(argc==2&&strcmp(argv[1],"run")==0)argc=1;
    command_mode=argc>1;
    result=stnc_core_init();

    if(result!=0){
        fprintf(stderr,"STNC Core initialization failed.\n");
        return 1;
    }

    if(command_mode){
        result=stnc_command_run(argc,argv);
        stnc_core_shutdown();
        return result;
    }

    stnc_log_console_enable(1);
    result=stnc_core_run();
    stnc_log_console_enable(0);

    if(result!=0){
        fprintf(stderr,"STNC Core runtime failed.\n");
        stnc_core_shutdown();
        return 1;
    }

    stnc_core_shutdown();
    printf("STNC Core stopped.\n");
    return 0;
}
