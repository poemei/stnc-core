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
static unsigned int submit_calls;
static int search_found;
static stnc_stratum_result submit_result=STNC_STRATUM_RESULT_ACCEPTED;
static uint64_t observed_first_nonce;
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
    if(client==NULL||host==NULL||strncmp(address,"stn0_",5u)!=0)return 1;
    if(!((strcmp(host,"stratum.stn-chain.org")==0&&port==18475u)||
         (strcmp(host,"stratum2.stn-chain.org")==0&&port==18476u)))return 1;
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
{(void)client;(void)work_id;(void)nonce;submit_calls++;if(result!=NULL)*result=submit_result;return 0;}
int stnc_stratum_client_progress(stnc_stratum_client *client,const uint8_t work_id[32],uint64_t hashes,uint64_t elapsed_ms)
{(void)client;(void)work_id;(void)hashes;(void)elapsed_ms;progress_calls++;return 0;}
stnc_mining_result stnc_mining_search_target_timed(uint8_t block[STNC_STNC_BLOCK_HEADER_SIZE],const uint8_t target[32],
    uint64_t first_nonce,unsigned int budget_ms,uint64_t *attempts,uint64_t *found_nonce,uint8_t digest[32])
{
    (void)block;if(target==NULL||budget_ms!=20u)return STNC_MINING_ERROR;
    memcpy(observed_target,target,32u);observed_first_nonce=first_nonce;
    search_calls++;*attempts=search_attempts;*found_nonce=0u;memset(digest,0,32u);
    clock_ms+=20u;
    if(search_found){*found_nonce=first_nonce+search_attempts-1u;return STNC_MINING_FOUND;}
    return STNC_MINING_EXHAUSTED;
}

static int fail_at(int line){fprintf(stderr,"Background mining test failed at line %d.\n",line);return 1;}
#define TEST_FAIL() fail_at(__LINE__)

int main(void)
{
    stnc_mining_service_status status;
    memset(&config,0,sizeof(config));memcpy(config.mining_backend,"automatic",sizeof("automatic"));
    memcpy(config.stratum_host,"stratum.stn-chain.org",sizeof("stratum.stn-chain.org"));
    config.stratum_port=18475u;config.mining_cpu_limit_percent=2u;
    if(stnc_background_mining_init()!=0)return TEST_FAIL();
    stnc_background_mining_tick();stnc_background_mining_status(&status);
    if(status.enabled||status.running||search_calls!=0u)return TEST_FAIL();
    stnc_background_mining_shutdown();

    config.mining_enabled=1;clock_ms=0u;job_ready=1;
    if(stnc_background_mining_init()!=0)return TEST_FAIL();
    stnc_background_mining_tick();stnc_background_mining_status(&status);
    if(!status.enabled||!status.running||status.active_backend!=STNC_MINING_BACKEND_CPU||
       status.passes!=1u||status.attempts!=25u||search_calls!=1u||connect_calls!=1u||progress_calls!=1u||
       observed_target[0]!=0xabu||observed_target[31]!=0xabu)return TEST_FAIL();
    clock_ms=500u;stnc_background_mining_tick();if(search_calls!=1u||poll_calls!=2u)return TEST_FAIL();
    clock_ms=1000u;stnc_background_mining_tick();if(search_calls!=2u||poll_calls!=3u)return TEST_FAIL();
    config.mining_enabled=0;clock_ms=1500u;stnc_background_mining_tick();stnc_background_mining_status(&status);
    if(status.enabled||status.running)return TEST_FAIL();
    config.mining_enabled=1;clock_ms=1600u;stnc_background_mining_tick();stnc_background_mining_status(&status);
    if(!status.enabled||status.running||connect_calls!=2u)return TEST_FAIL();
    job_ready=1;clock_ms=1700u;stnc_background_mining_tick();stnc_background_mining_status(&status);
    if(!status.running||search_calls!=3u)return TEST_FAIL();
    wallet_ok=0;clock_ms=2000u;stnc_background_mining_tick();stnc_background_mining_status(&status);
    if(status.running)return TEST_FAIL();
    stnc_background_mining_shutdown();

    wallet_ok=1;memcpy(config.mining_backend,"cpu",sizeof("cpu"));config.mining_enabled=1;
    search_calls=0u;submit_calls=0u;
    search_found=1;submit_result=STNC_STRATUM_RESULT_ACCEPTED;job_ready=1;clock_ms=4000u;
    if(stnc_background_mining_init()!=0)return TEST_FAIL();
    stnc_background_mining_tick();
    if(submit_calls!=1u||observed_first_nonce!=7u)return TEST_FAIL();
    clock_ms=5000u;stnc_background_mining_tick();
    if(submit_calls!=2u||observed_first_nonce!=32u)return TEST_FAIL();
    submit_result=STNC_STRATUM_RESULT_REJECTED;clock_ms=6000u;stnc_background_mining_tick();
    if(submit_calls!=3u||observed_first_nonce!=57u)return TEST_FAIL();
    submit_result=STNC_STRATUM_RESULT_STALE;clock_ms=7000u;stnc_background_mining_tick();
    if(submit_calls!=4u||observed_first_nonce!=82u)return TEST_FAIL();
    clock_ms=8000u;stnc_background_mining_tick();
    if(submit_calls!=4u)return TEST_FAIL();
    job_ready=1;submit_result=STNC_STRATUM_RESULT_PROVIDER;clock_ms=9000u;stnc_background_mining_tick();
    if(submit_calls!=5u||observed_first_nonce!=7u)return TEST_FAIL();
    clock_ms=10000u;stnc_background_mining_tick();
    if(submit_calls!=5u)return TEST_FAIL();
    stnc_background_mining_shutdown();
    search_found=0;

    memcpy(config.mining_backend,"cpu",sizeof("cpu"));config.mining_enabled=1;
    memcpy(config.stratum_host,"stratum.stn-chain.org",sizeof("stratum.stn-chain.org"));config.stratum_port=18475u;
    job_ready=1;clock_ms=11000u;
    if(stnc_background_mining_init()!=0)return TEST_FAIL();
    stnc_background_mining_tick();stnc_background_mining_status(&status);
    if(!status.running)return TEST_FAIL();
    memcpy(config.stratum_host,"stratum2.stn-chain.org",sizeof("stratum2.stn-chain.org"));config.stratum_port=18476u;
    job_ready=0;clock_ms=11500u;stnc_background_mining_tick();stnc_background_mining_status(&status);
    if(status.running||connect_calls<2u)return TEST_FAIL();
    job_ready=1;clock_ms=12000u;stnc_background_mining_tick();stnc_background_mining_status(&status);
    if(!status.running)return TEST_FAIL();
    stnc_background_mining_shutdown();

    wallet_ok=1;memcpy(config.mining_backend,"gpu",sizeof("gpu"));
    if(stnc_background_mining_init()!=0)return TEST_FAIL();
    clock_ms=11000u;stnc_background_mining_tick();stnc_background_mining_status(&status);
    if(status.running||search_calls!=5u)return TEST_FAIL();
    stnc_background_mining_shutdown();

    puts("Background mining tests passed.");
    return 0;
}
