#include <stdio.h>
#include <string.h>
#include "stnc_mining_service.h"

int main(void)
{
    stnc_mining_service_config config;
    stnc_mining_service_status status;

    stnc_mining_service_reset();
    stnc_mining_service_status_read(&status);
    if(status.enabled||status.running||status.hashrate_hps!=0u||
       status.configured_backend!=STNC_MINING_BACKEND_AUTOMATIC||
       status.cpu_limit_percent!=2u)return 1;

    memset(&config,0,sizeof(config));
    if(stnc_mining_service_configure(NULL)==0)return 1;
    config.enabled=2;config.backend=STNC_MINING_BACKEND_CPU;config.cpu_limit_percent=2u;
    if(stnc_mining_service_configure(&config)==0)return 1;
    config.enabled=1;config.backend=(stnc_mining_backend)99;config.cpu_limit_percent=2u;
    if(stnc_mining_service_configure(&config)==0)return 1;
    config.backend=STNC_MINING_BACKEND_CPU;config.cpu_limit_percent=0u;
    if(stnc_mining_service_configure(&config)==0)return 1;
    config.cpu_limit_percent=3u;
    if(stnc_mining_service_configure(&config)==0)return 1;

    config.cpu_limit_percent=2u;
    if(stnc_mining_service_configure(&config)!=0)return 1;
    stnc_mining_service_set_running(1,STNC_MINING_BACKEND_CPU);
    stnc_mining_service_record_pass(100u,0);
    stnc_mining_service_record_rate(100u,20u);
    stnc_mining_service_record_pass(50u,1);
    stnc_mining_service_record_rate(50u,10u);
    stnc_mining_service_status_read(&status);
    if(!status.enabled||!status.running||
       status.active_backend!=STNC_MINING_BACKEND_CPU||
       status.passes!=2u||status.attempts!=150u||status.solutions!=1u||
       status.hashrate_hps!=5000u)return 1;
    stnc_mining_service_record_rate(UINT64_MAX,1u);
    stnc_mining_service_status_read(&status);
    if(status.hashrate_hps!=UINT64_MAX)return 1;
    if(stnc_mining_service_cpu_work_ms()!=20u)return 1;
    if(stnc_mining_service_cpu_rest_ms(10u)!=990u)return 1;
    if(stnc_mining_service_cpu_rest_ms(20u)!=980u)return 1;
    if(stnc_mining_service_cpu_rest_ms(1000u)!=0u)return 1;

    config.enabled=0;
    if(stnc_mining_service_configure(&config)!=0)return 1;
    stnc_mining_service_status_read(&status);
    if(status.enabled||status.running||status.hashrate_hps!=0u)return 1;

    if(strcmp(stnc_mining_backend_name(STNC_MINING_BACKEND_AUTOMATIC),"automatic")!=0||
       strcmp(stnc_mining_backend_name(STNC_MINING_BACKEND_CPU),"cpu")!=0||
       strcmp(stnc_mining_backend_name(STNC_MINING_BACKEND_GPU),"gpu")!=0||
       strcmp(stnc_mining_backend_name(STNC_MINING_BACKEND_USB_ASIC),"usb-asic")!=0)return 1;

    puts("Mining service tests passed.");
    return 0;
}
