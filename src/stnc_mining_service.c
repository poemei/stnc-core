#include "stnc_mining_service.h"
#include <limits.h>
#include <string.h>

static stnc_mining_service_status service_status;

int stnc_mining_service_configure(const stnc_mining_service_config *config)
{
    if(config==NULL||
       (config->enabled!=0&&config->enabled!=1)||
       config->backend<STNC_MINING_BACKEND_AUTOMATIC||
       config->backend>STNC_MINING_BACKEND_USB_ASIC||
       config->cpu_limit_percent==0u||
       config->cpu_limit_percent>STNC_MINING_CPU_LIMIT_MAX)return 1;

    service_status.enabled=config->enabled;
    service_status.configured_backend=config->backend;
    service_status.cpu_limit_percent=config->cpu_limit_percent;
    if(!config->enabled){
        service_status.running=0;
        service_status.active_backend=STNC_MINING_BACKEND_AUTOMATIC;
    }
    return 0;
}

void stnc_mining_service_reset(void)
{
    memset(&service_status,0,sizeof(service_status));
    service_status.configured_backend=STNC_MINING_BACKEND_AUTOMATIC;
    service_status.active_backend=STNC_MINING_BACKEND_AUTOMATIC;
    service_status.cpu_limit_percent=STNC_MINING_CPU_LIMIT_MAX;
}

void stnc_mining_service_set_running(int running,stnc_mining_backend backend)
{
    if(!service_status.enabled||!running){
        service_status.running=0;
        service_status.active_backend=STNC_MINING_BACKEND_AUTOMATIC;
        return;
    }
    if(backend<STNC_MINING_BACKEND_CPU||backend>STNC_MINING_BACKEND_USB_ASIC)return;
    service_status.running=1;
    service_status.active_backend=backend;
}

void stnc_mining_service_record_pass(uint64_t attempts,int solution)
{
    if(!service_status.running)return;
    if(service_status.passes<UINT64_MAX)service_status.passes++;
    if(UINT64_MAX-service_status.attempts<attempts)service_status.attempts=UINT64_MAX;
    else service_status.attempts+=attempts;
    if(solution&&service_status.solutions<UINT64_MAX)service_status.solutions++;
}

void stnc_mining_service_status_read(stnc_mining_service_status *status)
{
    if(status!=NULL)*status=service_status;
}

unsigned int stnc_mining_service_cpu_work_ms(void)
{
    return (STNC_MINING_CPU_WINDOW_MS*service_status.cpu_limit_percent)/100u;
}

unsigned int stnc_mining_service_cpu_rest_ms(unsigned int elapsed_work_ms)
{
    unsigned int work=stnc_mining_service_cpu_work_ms();
    if(elapsed_work_ms>=work||work>=STNC_MINING_CPU_WINDOW_MS)return 0u;
    return STNC_MINING_CPU_WINDOW_MS-elapsed_work_ms;
}

const char *stnc_mining_backend_name(stnc_mining_backend backend)
{
    switch(backend){
    case STNC_MINING_BACKEND_AUTOMATIC:return "automatic";
    case STNC_MINING_BACKEND_CPU:return "cpu";
    case STNC_MINING_BACKEND_GPU:return "gpu";
    case STNC_MINING_BACKEND_USB_ASIC:return "usb-asic";
    default:return "unknown";
    }
}
