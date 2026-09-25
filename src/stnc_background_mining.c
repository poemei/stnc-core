#include "stnc_background_mining.h"

#include <stdint.h>
#include <string.h>

#include "stnc_config.h"
#include "stnc_core.h"
#include "stnc_log.h"
#include "stnc_mining.h"
#include "stnc_platform.h"
#include "stnc_wallet.h"
#include "stnc_wallet_store.h"

#define STNC_BACKGROUND_MINING_RETRY_MS 1000u

static int initialized;
static uint64_t next_tick_ms;
static uint8_t current_work_id[32];
static int have_work_id;
static uint64_t next_nonce;

static stnc_mining_backend configured_backend(const char *name)
{
    if(name==NULL)return STNC_MINING_BACKEND_AUTOMATIC;
    if(strcmp(name,"cpu")==0)return STNC_MINING_BACKEND_CPU;
    if(strcmp(name,"gpu")==0)return STNC_MINING_BACKEND_GPU;
    if(strcmp(name,"usb-asic")==0)return STNC_MINING_BACKEND_USB_ASIC;
    return STNC_MINING_BACKEND_AUTOMATIC;
}

static uint64_t block_nonce(const uint8_t block[STNC_STNC_BLOCK_HEADER_SIZE])
{
    size_t i;uint64_t nonce=0u;
    for(i=0u;i<STNC_STNC_MINING_NONCE_SIZE;i++)
        nonce=(nonce<<8)|block[STNC_STNC_MINING_NONCE_OFFSET+i];
    return nonce;
}

int stnc_background_mining_init(void)
{
    const stnc_config *config=stnc_config_get();
    stnc_mining_service_config mining;
    if(initialized||config==NULL)return 1;
    stnc_mining_service_reset();
    mining.enabled=config->mining_enabled;
    mining.backend=configured_backend(config->mining_backend);
    mining.cpu_limit_percent=config->mining_cpu_limit_percent;
    if(stnc_mining_service_configure(&mining)!=0)return 1;
    initialized=1;next_tick_ms=0u;have_work_id=0;next_nonce=0u;
    memset(current_work_id,0,sizeof(current_work_id));
    return 0;
}

void stnc_background_mining_tick(void)
{
    stnc_mining_service_status status;stnc_wallet_key key;char identity[STNC_STNC_ADDRESS_IDENTITY_SIZE+1u];
    uint8_t *payload=NULL,block[STNC_STNC_BLOCK_HEADER_SIZE],digest[32],accepted_id[32],accepted_work[40];
    size_t payload_length=0u;stnc_mining_template work;stnc_mining_context checked;
    uint64_t now,attempts=0u,found_nonce=0u,height=0u,first_nonce;
    stnc_mining_result result;stnc_core_work_base_result base;

    if(!initialized)return;
    stnc_mining_service_status_read(&status);
    if(!status.enabled){stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);return;}
    if(status.configured_backend!=STNC_MINING_BACKEND_AUTOMATIC&&
       status.configured_backend!=STNC_MINING_BACKEND_CPU){
        stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);return;
    }

    now=stnc_platform_monotonic_ms();
    if(now<next_tick_ms)return;
    next_tick_ms=now+STNC_BACKGROUND_MINING_RETRY_MS;

    memset(&key,0,sizeof(key));
    if(stnc_wallet_store_load(&key)!=0||
       stnc_core_derive_address(STNC_STNC_ADDRESS_IDENTITY,key.public_key,sizeof(key.public_key),
           identity,sizeof(identity))!=0){
        stnc_wallet_clear(&key);
        stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);
        return;
    }
    stnc_wallet_clear(&key);

    if(stnc_core_mining_template(&payload,&payload_length,&work)!=0||
       work.block_length!=sizeof(block)){
        stnc_core_mining_template_release(payload);
        stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);
        return;
    }
    memcpy(block,work.block,sizeof(block));
    if(!have_work_id||memcmp(current_work_id,work.work_id,sizeof(current_work_id))!=0){
        memcpy(current_work_id,work.work_id,sizeof(current_work_id));
        next_nonce=block_nonce(block);have_work_id=1;
    }
    first_nonce=next_nonce;

    base=stnc_core_check_work_base(work.parent_id,&checked);
    if(base!=STNC_CORE_WORK_BASE_CURRENT||!checked.template_available||
       memcmp(checked.tip_id,work.parent_id,32u)!=0||
       memcmp(checked.target,block+120u,32u)!=0){
        stnc_core_mining_template_release(payload);have_work_id=0;
        stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);
        return;
    }

    if(!status.running||status.active_backend!=STNC_MINING_BACKEND_CPU)
        stnc_log_info("Background mining active: backend=cpu cpu_limit=2%.");
    stnc_mining_service_set_running(1,STNC_MINING_BACKEND_CPU);
    result=stnc_mining_search_timed(block,first_nonce,stnc_mining_service_cpu_work_ms(),
        &attempts,&found_nonce,digest);
    stnc_mining_service_record_pass(attempts,result==STNC_MINING_FOUND);
    if(result==STNC_MINING_FOUND)stnc_log_info("Background mining found candidate work.");

    if(result==STNC_MINING_EXHAUSTED){
        if(attempts>UINT64_MAX-first_nonce)have_work_id=0;
        else next_nonce=first_nonce+attempts;
        stnc_core_mining_template_release(payload);return;
    }
    if(result!=STNC_MINING_FOUND){
        stnc_core_mining_template_release(payload);
        stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);return;
    }

    base=stnc_core_check_work_base(work.parent_id,&checked);
    if(base==STNC_CORE_WORK_BASE_CURRENT&&checked.template_available&&
       memcmp(checked.tip_id,work.parent_id,32u)==0&&
       memcmp(checked.target,block+120u,32u)==0){
        (void)stnc_core_submit_work(work.parent_id,work.work_id,identity,block,sizeof(block),
            accepted_id,&height,accepted_work);
    }
    have_work_id=0;
    stnc_core_mining_template_release(payload);
}

void stnc_background_mining_shutdown(void)
{
    if(!initialized)return;
    stnc_mining_service_set_running(0,STNC_MINING_BACKEND_AUTOMATIC);
    stnc_mining_service_reset();initialized=0;next_tick_ms=0u;next_nonce=0u;have_work_id=0;
    memset(current_work_id,0,sizeof(current_work_id));
}

void stnc_background_mining_status(stnc_mining_service_status *status)
{
    stnc_mining_service_status_read(status);
}
