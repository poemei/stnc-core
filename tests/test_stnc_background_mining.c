#include <stdio.h>
#include <string.h>

#include "stnc_background_mining.h"
#include "stnc_config.h"
#include "stnc_core.h"
#include "stnc_mining.h"
#include "stnc_stratum_client.h"
#include "stnc_wallet.h"

static stnc_config config;
static uint64_t clock_ms;
static int wallet_ok=1;
static int job_ready=1;
static unsigned int poll_calls;
static unsigned int search_calls;
static uint64_t search_attempts=25u;
static unsigned int connect_calls;
static unsigned int progress_calls;
static uint8_t observed_target[32];

const stnc_config *stnc_config_get(void){return &config;}
uint64_t stnc_platform_monotonic_ms(void){return clock_ms;}
int stnc_wallet_store_exists(void){return wallet_ok;}
int stnc_wallet_store_load(stnc_wallet_key *key){if(!wallet_ok)return 1;memset(key,0,sizeof(*key));key->public_key[0]=1u;return 0;}
int stnc_wallet_address(const stnc_wallet_key *key,char address[STNC_WALLET_ADDRESS_SIZE+1u])
{size_t i;if(!wallet_ok||key==NULL||address==NULL)return 1;memcpy(address,"stnw0_",6u);for(i=6u;i<70u;i++)address[i]='a';address[70]='\0';return 0;}
void stnc_wallet_clear(stnc_wallet_key *key){if(key!=NULL)memset(key,0,sizeof(*key));}
int stnc_core_derive_address(uint16_t type,const uint8_t *source,size_t source_length,char *address,size_t capacity)
{
    size_t i;(void)type;(void)source;(void)source_length;if(capacity<70u)return 1;
    memcpy(address,"stn0_",5u);for(i=5u;i<69u;i++)address[i]='a';address[69]='\0';return 0;
}
void stnc_stratum_client_init(stnc_stratum_client *client){memset(client,0,sizeof(*client));}
int stnc_stratum_client_connect(stnc_stratum_client *client,const char *host,unsigned short port,const char *address)
{
    if(client==NULL||strcmp(host,"stratum.stn-chain.org")!=0||port!=18475u||strncmp(address,"stn0_",5u)!=0)return 1;
    client->handle=client;client->connected=1;connect_calls++;return 0;
}
void stnc_stratum_client_disconnect(stnc_stratum_client *client)
{if(client!=NULL){client->handle=NULL;client->connected=0;}}
int stnc_stratum_client_poll_job(stnc_stratum_client *client,stnc_stratum_job *job,uint8_t *block,size_t capacity)
{
    if(client==NULL||!client->connected||job==NULL||block==NULL||capacity<168u)return -1;
    poll_calls++;
    if(!job_ready)return 1;
    memset(job,0,sizeof(*job));memset(block,0,168u);job->block_length=168u;job->initial_nonce=7u;job->work_id[0]=9u;
    memset(job->share_target,0xabu,sizeof(job->share_target));job_ready=0;return 0;
}
int stnc_stratum_client_submit(stnc_stratum_client *client,const uint8_t work_id[32],uint64_t nonce,stnc_stratum_result *result)
{(void)client;(void)work_id;(void)nonce;if(result!=NULL)*result=STNC_STRATUM_RESULT_ACCEPTED;return 0;}
int stnc_stratum_client_progress(stnc_stratum_client *client,const uint8_t work_id[32],uint64_t hashes,uint64_t elapsed_ms)
{(void)client;(void)work_id;(void)hashes;(void)elapsed_ms;progress_calls++;return 0;}
stnc_mining_result stnc_mining_search_target_timed(uint8_t block[STNC_STNC_BLOCK_HEADER_SIZE],const uint8_t target[32],
    uint64_t first_nonce,unsigned int budget_ms,uint64_t *attempts,uint64_t *found_nonce,uint8_t digest[32])
{
    (void)block;if(target==NULL||budget_ms!=20u)return STNC_MINING_ERROR;
    memcpy(observed_target,target,32u);
    if(search_calls==0u&&first_nonce!=7u)return STNC_MINING_ERROR;
    if(search_calls==1u&&first_nonce!=32u)return STNC_MINING_ERROR;
    search_calls++;*attempts=search_attempts;*found_nonce=0u;memset(digest,0,32u);
    clock_ms+=20u;return STNC_MINING_EXHAUSTED;
}

int main(void)
{
    stnc_mining_service_status status;
    memset(&config,0,sizeof(config));memcpy(config.mining_backend,"automatic",sizeof("automatic"));
    memcpy(config.stratum_host,"stratum.stn-chain.org",sizeof("stratum.stn-chain.org"));
    config.stratum_port=18475u;config.mining_cpu_limit_percent=2u;
    if(stnc_background_mining_init()!=0)return 1;
    stnc_background_mining_tick();stnc_background_mining_status(&status);
    if(status.enabled||status.running||search_calls!=0u)return 1;
    stnc_background_mining_shutdown();

    config.mining_enabled=1;clock_ms=0u;job_ready=1;
    if(stnc_background_mining_init()!=0)return 1;
    stnc_background_mining_tick();stnc_background_mining_status(&status);
    if(!status.enabled||!status.running||status.active_backend!=STNC_MINING_BACKEND_CPU||
       status.passes!=1u||status.attempts!=25u||search_calls!=1u||connect_calls!=1u||progress_calls!=1u||
       observed_target[0]!=0xabu||observed_target[31]!=0xabu)return 1;
    clock_ms=500u;stnc_background_mining_tick();if(search_calls!=1u||poll_calls!=2u)return 1;
    clock_ms=1000u;stnc_background_mining_tick();if(search_calls!=2u||poll_calls!=3u)return 1;
    wallet_ok=0;clock_ms=2000u;stnc_background_mining_tick();stnc_background_mining_status(&status);
    if(status.running)return 1;
    stnc_background_mining_shutdown();

    wallet_ok=1;memcpy(config.mining_backend,"gpu",sizeof("gpu"));
    if(stnc_background_mining_init()!=0)return 1;
    clock_ms=3000u;stnc_background_mining_tick();stnc_background_mining_status(&status);
    if(status.running||search_calls!=2u)return 1;
    stnc_background_mining_shutdown();

    puts("Background mining tests passed.");
    return 0;
}
