#include "stnc_background_mining.h"

#include <stdint.h>
#include <string.h>

#include "stnc_config.h"
#include "stnc_core.h"
#include "stnc_log.h"
#include "stnc_mining.h"
#include "stnc_platform.h"
#include "stnc_stratum_client.h"
#include "stnc_wallet.h"
#include "stnc_wallet_store.h"

#define STNC_BACKGROUND_MINING_RETRY_MS 1000u
#define STNC_BACKGROUND_MINING_BLOCK_CAPACITY (1024u*1024u+4096u)

static int initialized;
static uint64_t next_tick_ms;
static stnc_stratum_client stratum;
static uint8_t block[STNC_BACKGROUND_MINING_BLOCK_CAPACITY];
static stnc_stratum_job active_job;
static int have_job;
static uint64_t next_nonce;

static stnc_mining_backend configured_backend(const char *name)
{
    if(name==NULL)return STNC_MINING_BACKEND_AUTOMATIC;
    if(strcmp(name,"cpu")==0)return STNC_MINING_BACKEND_CPU;
    if(strcmp(name,"gpu")==0)return STNC_MINING_BACKEND_GPU;
    if(strcmp(name,"usb-asic")==0)return STNC_MINING_BACKEND_USB_ASIC;
    return STNC_MINING_BACKEND_AUTOMATIC;
}

static int mining_identity(char identity[STNC_STNC_ADDRESS_IDENTITY_SIZE+1u])
{
    stnc_wallet_key key;int rc;
    memset(&key,0,sizeof(key));
    rc=stnc_wallet_store_load(&key);
    if(rc==0)rc=stnc_core_derive_address(STNC_STNC_ADDRESS_IDENTITY,key.public_key,
        sizeof(key.public_key),identity,STNC_STNC_ADDRESS_IDENTITY_SIZE+1u);
    stnc_wallet_clear(&key);return rc;
}

int stnc_background_mining_init(void)
{
    const stnc_config *config=stnc_config_get();stnc_mining_service_config mining;
    if(initialized||config==NULL)return 1;
    stnc_mining_service_reset();stnc_stratum_client_init(&stratum);
    mining.enabled=config->mining_enabled;mining.backend=configured_backend(config->mining_backend);
    mining.cpu_limit_percent=config->mining_cpu_limit_percent;
    if(stnc_mining_service_configure(&mining)!=0)return 1;
    initialized=1;next_tick_ms=0u;have_job=0;next_nonce=0u;memset(&active_job,0,sizeof(active_job));return 0;
}

void stnc_background_mining_tick(void)
{
    const stnc_config *config;stnc_mining_service_status status;stnc_stratum_job job;
    stnc_stratum_result submit_result;char identity[STNC_STNC_ADDRESS_IDENTITY_SIZE+1u];
    uint64_t now,attempts=0u,found_nonce=0u,start_ms,elapsed_ms;uint8_t digest[32];
    stnc_mining_result result;int poll_result;

    if(!initialized)return;
    stnc_mining_service_status_read(&status);
    if(!status.enabled){stnc_stratum_client_disconnect(&stratum);stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);return;}
    if(status.configured_backend!=STNC_MINING_BACKEND_AUTOMATIC&&status.configured_backend!=STNC_MINING_BACKEND_CPU){
        stnc_stratum_client_disconnect(&stratum);stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);return;
    }

    now=stnc_platform_monotonic_ms();if(now<next_tick_ms)return;next_tick_ms=now+STNC_BACKGROUND_MINING_RETRY_MS;
    config=stnc_config_get();if(config==NULL||mining_identity(identity)!=0){
        stnc_stratum_client_disconnect(&stratum);stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);return;
    }

    if(!stratum.connected){
        if(stnc_stratum_client_connect(&stratum,config->stratum_host,config->stratum_port,identity)!=0){
            stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);return;
        }
        stnc_log_info("Background mining connected to STN-Stratum.");
    }

    memset(&job,0,sizeof(job));
    poll_result=stnc_stratum_client_poll_job(&stratum,&job,block,sizeof(block));
    if(poll_result<0){
        stnc_stratum_client_disconnect(&stratum);have_job=0;
        stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);return;
    }
    if(poll_result==0){
        if(job.block_length!=STNC_STNM_BLOCK_HEADER_SIZE){
            stnc_stratum_client_disconnect(&stratum);have_job=0;
            stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);return;
        }
        active_job=job;next_nonce=job.initial_nonce;have_job=1;
    }
    if(!have_job)return;

    if(!status.running||status.active_backend!=STNC_MINING_BACKEND_CPU)
        stnc_log_info("Background mining active through STN-Stratum: backend=cpu.");
    stnc_mining_service_set_running(1,STNC_MINING_BACKEND_CPU);
    start_ms=stnc_platform_monotonic_ms();
    result=stnc_mining_search_timed(block,next_nonce,stnc_mining_service_cpu_work_ms(),
        &attempts,&found_nonce,digest);
    elapsed_ms=stnc_platform_monotonic_ms()-start_ms;
    stnc_mining_service_record_pass(attempts,result==STNC_MINING_FOUND);
    if(attempts>0u&&elapsed_ms>0u)(void)stnc_stratum_client_progress(&stratum,active_job.work_id,attempts,elapsed_ms);

    if(result==STNC_MINING_FOUND){
        stnc_log_info("Background mining found candidate work; submitting through STN-Stratum.");
        if(stnc_stratum_client_submit(&stratum,active_job.work_id,found_nonce,&submit_result)!=0)
            stnc_stratum_client_disconnect(&stratum);
        have_job=0;
    }else if(result==STNC_MINING_EXHAUSTED){
        if(attempts>UINT64_MAX-next_nonce)have_job=0;
        else next_nonce+=attempts;
    }else{
        stnc_stratum_client_disconnect(&stratum);
        stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);
    }
}

void stnc_background_mining_shutdown(void)
{
    if(!initialized)return;
    stnc_stratum_client_disconnect(&stratum);stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);
    stnc_mining_service_reset();initialized=0;next_tick_ms=0u;have_job=0;next_nonce=0u;
    memset(&active_job,0,sizeof(active_job));memset(block,0,sizeof(block));
}

void stnc_background_mining_status(stnc_mining_service_status *status)
{
    stnc_mining_service_status_read(status);
}
