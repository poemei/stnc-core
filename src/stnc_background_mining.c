#include "stnc_background_mining.h"

#include <stdint.h>
#include <stdio.h>
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
#define STNC_BACKGROUND_MINING_BLOCK_CAPACITY STNC_STNC_BLOCK_MAX_SIZE

static int initialized;
static uint64_t next_work_ms;
static int applied_enabled;
static stnc_mining_backend applied_backend;
static unsigned int applied_cpu_limit;
static char applied_stratum_host[STNC_CONFIG_PEER_MAX];
static unsigned short applied_stratum_port;
static stnc_stratum_client stratum;
static uint8_t block[STNC_BACKGROUND_MINING_BLOCK_CAPACITY];
static stnc_stratum_job active_job;
static int have_job;
static uint64_t next_nonce;

static void clear_stratum_work(void)
{
    stnc_stratum_client_disconnect(&stratum);have_job=0;next_nonce=0u;next_work_ms=0u;
    memset(&active_job,0,sizeof(active_job));memset(block,0,sizeof(block));
    stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);
}

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
    stnc_wallet_key key;char wallet_address[STNC_WALLET_ADDRESS_SIZE+1u];int rc;
    memset(&key,0,sizeof(key));memset(wallet_address,0,sizeof(wallet_address));
    if(!stnc_wallet_store_exists())return 1;
    rc=stnc_wallet_store_load(&key);
    if(rc==0)rc=stnc_wallet_address(&key,wallet_address);
    if(rc==0)rc=stnc_core_derive_address(STNC_STNC_ADDRESS_IDENTITY,key.public_key,
        sizeof(key.public_key),identity,STNC_STNC_ADDRESS_IDENTITY_SIZE+1u);
    stnc_wallet_clear(&key);memset(wallet_address,0,sizeof(wallet_address));return rc;
}

int stnc_background_mining_init(void)
{
    const stnc_config *config=stnc_config_get();stnc_mining_service_config mining;
    if(initialized||config==NULL)return 1;
    stnc_mining_service_reset();stnc_stratum_client_init(&stratum);
    mining.enabled=config->mining_enabled;mining.backend=configured_backend(config->mining_backend);
    mining.cpu_limit_percent=config->mining_cpu_limit_percent;
    if(stnc_mining_service_configure(&mining)!=0)return 1;
    applied_enabled=mining.enabled;applied_backend=mining.backend;applied_cpu_limit=mining.cpu_limit_percent;
    memcpy(applied_stratum_host,config->stratum_host,sizeof(applied_stratum_host));
    applied_stratum_host[sizeof(applied_stratum_host)-1u]='\0';applied_stratum_port=config->stratum_port;
    initialized=1;next_work_ms=0u;have_job=0;next_nonce=0u;memset(&active_job,0,sizeof(active_job));return 0;
}

static int apply_runtime_config(const stnc_config *config)
{
    stnc_mining_service_config mining;stnc_mining_backend backend;
    if(config==NULL)return 1;
    backend=configured_backend(config->mining_backend);
    mining.enabled=config->mining_enabled;mining.backend=backend;
    mining.cpu_limit_percent=config->mining_cpu_limit_percent;
    if(mining.enabled!=applied_enabled||backend!=applied_backend||
       mining.cpu_limit_percent!=applied_cpu_limit){
        if(stnc_mining_service_configure(&mining)!=0)return 1;
    }
    if(!mining.enabled||backend!=applied_backend||
       strcmp(config->stratum_host,applied_stratum_host)!=0||config->stratum_port!=applied_stratum_port){
        clear_stratum_work();
    }
    applied_enabled=mining.enabled;applied_backend=backend;applied_cpu_limit=mining.cpu_limit_percent;
    memcpy(applied_stratum_host,config->stratum_host,sizeof(applied_stratum_host));
    applied_stratum_host[sizeof(applied_stratum_host)-1u]='\0';applied_stratum_port=config->stratum_port;
    return 0;
}

void stnc_background_mining_tick(void)
{
    const stnc_config *config;stnc_mining_service_status status;stnc_stratum_job job;
    stnc_stratum_result submit_result;char identity[STNC_STNC_ADDRESS_IDENTITY_SIZE+1u];
    uint64_t now,attempts=0u,found_nonce=0u,start_ms,elapsed_ms;uint8_t digest[32];
    stnc_mining_result result;int poll_result;

    if(!initialized)return;
    config=stnc_config_get();
    if(apply_runtime_config(config)!=0)return;
    stnc_mining_service_status_read(&status);
    if(!status.enabled){clear_stratum_work();stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);return;}
    if(status.configured_backend!=STNC_MINING_BACKEND_AUTOMATIC&&status.configured_backend!=STNC_MINING_BACKEND_CPU){
        clear_stratum_work();stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);return;
    }

    now=stnc_platform_monotonic_ms();
    if(config==NULL||mining_identity(identity)!=0){
        stnc_stratum_client_disconnect(&stratum);stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);return;
    }

    if(!stratum.connected){
        char message[512];
        if(stnc_stratum_client_connect(&stratum,config->stratum_host,config->stratum_port,identity)!=0){
            stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);return;
        }
        if(snprintf(message,sizeof(message),"Background mining connected to STN-Stratum: %s:%u identity=%s",
                config->stratum_host,(unsigned int)config->stratum_port,identity)>=0)stnc_log_info(message);
    }

    memset(&job,0,sizeof(job));
    poll_result=stnc_stratum_client_poll_job(&stratum,&job,block,sizeof(block));
    if(poll_result<0){
        clear_stratum_work();
        stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);return;
    }
    if(poll_result==0){
        active_job=job;next_nonce=job.initial_nonce;have_job=1;next_work_ms=now;
    }
    if(!have_job)return;
    if(now<next_work_ms)return;
    next_work_ms=now+STNC_BACKGROUND_MINING_RETRY_MS;

    if(!status.running||status.active_backend!=STNC_MINING_BACKEND_CPU)
        stnc_log_info("Background mining active through STN-Stratum: backend=cpu.");
    stnc_mining_service_set_running(1,STNC_MINING_BACKEND_CPU);
    start_ms=stnc_platform_monotonic_ms();
    result=stnc_mining_search_target_timed(block,active_job.share_target,next_nonce,
        stnc_mining_service_cpu_work_ms(),&attempts,&found_nonce,digest);
    elapsed_ms=stnc_platform_monotonic_ms()-start_ms;
    stnc_mining_service_record_pass(attempts,result==STNC_MINING_FOUND);
    if(attempts>0u&&elapsed_ms>0u)(void)stnc_stratum_client_progress(&stratum,active_job.work_id,attempts,elapsed_ms);

    if(result==STNC_MINING_FOUND){
        stnc_log_info("Background mining found qualifying share; submitting through STN-Stratum.");
        if(stnc_stratum_client_submit(&stratum,active_job.work_id,found_nonce,&submit_result)!=0){
            clear_stratum_work();
            stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);return;
        }
        if(submit_result==STNC_STRATUM_RESULT_ACCEPTED)
            stnc_log_info("STN-Stratum accepted qualifying share.");
        else if(submit_result==STNC_STRATUM_RESULT_REJECTED)
            stnc_log_info("STN-Stratum reported Chain rejection for qualifying share.");
        else if(submit_result==STNC_STRATUM_RESULT_STALE){
            stnc_log_info("STN-Stratum reported stale work.");have_job=0;return;
        }else if(submit_result==STNC_STRATUM_RESULT_PROVIDER){
            stnc_log_info("STN-Stratum provider is temporarily unavailable.");have_job=0;return;
        }else{
            stnc_log_error("STN-Stratum rejected the submission protocol.");
            clear_stratum_work();
            stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);return;
        }
        if(found_nonce==UINT64_MAX)have_job=0;
        else next_nonce=found_nonce+1u;
    }else if(result==STNC_MINING_EXHAUSTED){
        if(attempts>UINT64_MAX-next_nonce)have_job=0;
        else next_nonce+=attempts;
    }else{
        clear_stratum_work();
        stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);
    }
}

void stnc_background_mining_shutdown(void)
{
    if(!initialized)return;
    stnc_stratum_client_disconnect(&stratum);stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);
    stnc_mining_service_reset();initialized=0;next_work_ms=0u;have_job=0;next_nonce=0u;
    applied_enabled=0;applied_backend=STNC_MINING_BACKEND_AUTOMATIC;applied_cpu_limit=0u;
    memset(applied_stratum_host,0,sizeof(applied_stratum_host));applied_stratum_port=0u;
    memset(&active_job,0,sizeof(active_job));memset(block,0,sizeof(block));
}

void stnc_background_mining_status(stnc_mining_service_status *status)
{
    stnc_mining_service_status_read(status);
}
