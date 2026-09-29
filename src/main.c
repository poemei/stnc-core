#include <stdio.h>
#include <string.h>

#include "stnc_command.h"
#include "stnc_core.h"
#include "stnc_log.h"
#include "stnc_gui.h"
#include "stnc_profile.h"
/* Kept here until build manifests add stnc_profile.c as its own unit. */
#include "stnc_profile.c"

static int profile_command(int argc,char **argv)
{
    stnc_profile_info profile;
    if(argc==3&&strcmp(argv[2],"status")==0){if(stnc_profile_active(&profile)!=0)return 1;printf("STNC Core profile\n  Active: %s\n",profile.name);return 0;}
    if(argc==4&&strcmp(argv[2],"create")==0){if(stnc_profile_create(argv[3],&profile)!=0){fprintf(stderr,"Profile creation failed. Name must be unique and use letters, numbers, spaces, '-' or '_'.\n");return 1;}printf("STNC Core profile created\n  Active: %s\n",profile.name);return 0;}
    if(argc==4&&strcmp(argv[2],"select")==0){if(stnc_profile_select(argv[3],&profile)!=0){fprintf(stderr,"Profile selection failed. Both wallet and identity must exist and be valid.\n");return 1;}printf("STNC Core profile selected\n  Active: %s\n",profile.name);return 0;}
    fprintf(stderr,"Usage:\n  stnc-core profile status\n  stnc-core profile create <name>\n  stnc-core profile select <name>\n");return 1;
}

int main(int argc,char **argv)
{
    int result;int command_mode;if(argc>=2&&strcmp(argv[1],"profile")==0)return profile_command(argc,argv);
    if(argc==1||(argc==2&&strcmp(argv[1],"gui")==0))return stnc_gui_run();printf("STNC Core\n");if(argc==2&&strcmp(argv[1],"run")==0)argc=1;
    command_mode=argc>1;result=stnc_core_init();if(result!=0){fprintf(stderr,"STNC Core initialization failed.\n");return 1;}
    if(command_mode){result=stnc_command_run(argc,argv);stnc_core_shutdown();return result;}stnc_log_console_enable(1);result=stnc_core_run();stnc_log_console_enable(0);
    if(result!=0){fprintf(stderr,"STNC Core runtime failed.\n");stnc_core_shutdown();return 1;}stnc_core_shutdown();printf("STNC Core stopped.\n");return 0;
}
