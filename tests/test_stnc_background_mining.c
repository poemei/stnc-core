#include <stdio.h>
#include <string.h>

#include "stnc_background_mining.h"
#include "stnc_config.h"
#include "stnc_core.h"
#include "stnc_mining.h"
#include "stnc_wallet.h"

static stnc_config config;
static uint64_t clock_ms;
static int wallet_ok=1;
static int template_ok=1;
static unsigned int search_calls;
static uint64_t search_attempts=25u;

const stnc_config *stnc_config_get(void){return &config;}
uint64_t stnc_platform_monotonic_ms(void){return clock_ms;}
int stnc_wallet_store_load(stnc_wallet_key *key){if(!wallet_ok)return 1;memset(key,0,sizeof(*key));key->public_key[0]=1u;return 0;}
void stnc_wallet_clear(stnc_wallet_key *key){if(key!=NULL)memset(key,0,sizeof(*key));}
int stnc_core_derive_address(uint16_t type,const uint8_t *source,size_t source_length,char *address,size_t capacity)
{(void)type;(void)source;(void)source_length;if(capacity<70u)return 1;memset(address,'a',69u);address[69]='\0';return 0;}
int stnc_core_mining_template(uint8_t **payload,size_t *payload_length,stnc_mining_template *work)
{
    static uint8_t frame[STNC_STNC_BLOCK_HEADER_SIZE];
    if(!template_ok)return 1;memset(frame,0,sizeof(frame));memset(frame+120u,0x11,32u);
    memset(work,0,sizeof(*work));work->block=frame;work->block_length=sizeof(frame);work->work_id[0]=7u;
    memcpy(work->parent_id,"01234567890123456789012345678901",32u);
    *payload=frame;*payload_length=sizeof(frame);return 0;
}
void stnc_core_mining_template_release(uint8_t *payload){(void)payload;}
stnc_core_work_base_result stnc_core_check_work_base(const uint8_t tip_id[32],stnc_mining_context *context)
{
    memset(context,0,sizeof(*context));context->template_available=1u;memcpy(context->tip_id,tip_id,32u);
    memset(context->target,0x11,32u);return STNC_CORE_WORK_BASE_CURRENT;
}
stnc_mining_result stnc_mining_search_timed(uint8_t block[STNC_STNC_BLOCK_HEADER_SIZE],uint64_t first_nonce,
    unsigned int budget_ms,uint64_t *attempts,uint64_t *found_nonce,uint8_t digest[32])
{
    (void)block;(void)first_nonce;if(budget_ms!=20u)return STNC_MINING_ERROR;
    search_calls++;*attempts=search_attempts;*found_nonce=0u;memset(digest,0,32u);return STNC_MINING_EXHAUSTED;
}
int stnc_core_submit_work(const uint8_t parent_id[32],const uint8_t work_id[32],const char *miner_identity,
    const uint8_t *block,size_t block_length,uint8_t block_id[32],uint64_t *height,uint8_t cumulative_work[40])
{(void)parent_id;(void)work_id;(void)miner_identity;(void)block;(void)block_length;(void)block_id;(void)height;(void)cumulative_work;return 0;}

int main(void)
{
    stnc_mining_service_status status;
    memset(&config,0,sizeof(config));strcpy(config.mining_backend,"automatic");config.mining_cpu_limit_percent=2u;
    if(stnc_background_mining_init()!=0)return 1;
    stnc_background_mining_tick();stnc_background_mining_status(&status);
    if(status.enabled||status.running||search_calls!=0u)return 1;
    stnc_background_mining_shutdown();

    config.mining_enabled=1;
    if(stnc_background_mining_init()!=0)return 1;
    clock_ms=0u;stnc_background_mining_tick();stnc_background_mining_status(&status);
    if(!status.enabled||!status.running||status.active_backend!=STNC_MINING_BACKEND_CPU||
       status.passes!=1u||status.attempts!=25u||search_calls!=1u)return 1;
    clock_ms=500u;stnc_background_mining_tick();if(search_calls!=1u)return 1;
    clock_ms=1000u;stnc_background_mining_tick();if(search_calls!=2u)return 1;
    wallet_ok=0;clock_ms=2000u;stnc_background_mining_tick();stnc_background_mining_status(&status);
    if(status.running)return 1;
    stnc_background_mining_shutdown();

    wallet_ok=1;strcpy(config.mining_backend,"gpu");
    if(stnc_background_mining_init()!=0)return 1;
    clock_ms=3000u;stnc_background_mining_tick();stnc_background_mining_status(&status);
    if(status.running||search_calls!=2u)return 1;
    stnc_background_mining_shutdown();

    puts("Background mining tests passed.");
    return 0;
}
