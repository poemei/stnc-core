#include <stdio.h>
#include <string.h>

#include "stnc_background_mining.h"
#include "stnc_config.h"
#include "stnc_core.h"
#include "stnc_identity.h"
#include "stnc_mining.h"
#include "stnc_stratum_client.h"

static stnc_config config;
static uint64_t clock_ms;
static int identity_ok=1;
static int job_ready=1;
static unsigned connect_calls;
static unsigned poll_calls;
static unsigned search_calls;
static unsigned progress_calls;
static char observed_address[70];

const stnc_config *stnc_config_get(void){return &config;}
uint64_t stnc_platform_monotonic_ms(void){return clock_ms;}
int stnc_platform_get_app_directory(char *buffer,size_t buffer_size)
{const char *directory="build";size_t n=strlen(directory);if(buffer==NULL||buffer_size<=n)return 1;memcpy(buffer,directory,n+1u);return 0;}
char stnc_platform_path_separator(void){return '\\';}

int stnc_identity_status_read(stnc_identity_status *status)
{
    size_t i;
    if(status==NULL)return 1;
    memset(status,0,sizeof(*status));
    if(!identity_ok)return 0;
    status->present=1;status->valid=1;
    memcpy(status->address,"stn0_",5u);
    for(i=5u;i<69u;i++)status->address[i]='a';
    status->address[69]='\0';
    return 0;
}

void stnc_stratum_client_init(stnc_stratum_client *client){memset(client,0,sizeof(*client));}
int stnc_stratum_client_connect(stnc_stratum_client *client,const char *host,unsigned short port,const char *address)
{
    if(client==NULL||host==NULL||address==NULL)return 1;
    if(strlen(address)!=69u||strncmp(address,"stn0_",5u)!=0)return 1;
    if(!((strcmp(host,"stratum.stn-chain.org")==0&&port==18475u)||
         (strcmp(host,"stratum2.stn-chain.org")==0&&port==18476u)))return 1;
    memcpy(observed_address,address,sizeof(observed_address));
    client->handle=client;client->connected=1;connect_calls++;return 0;
}
void stnc_stratum_client_disconnect(stnc_stratum_client *client)
{if(client!=NULL){client->handle=NULL;client->connected=0;}}
int stnc_stratum_client_poll_job(stnc_stratum_client *client,stnc_stratum_job *job,uint8_t *block,size_t capacity)
{
    if(client==NULL||!client->connected||job==NULL||block==NULL||capacity<168u)return -1;
    poll_calls++;if(!job_ready)return 1;
    memset(job,0,sizeof(*job));memset(block,0,168u);job->block_length=168u;job->initial_nonce=7u;
    job->work_id[0]=9u;memset(job->share_target,0xabu,sizeof(job->share_target));job_ready=0;return 0;
}
int stnc_stratum_client_submit(stnc_stratum_client *client,const uint8_t work_id[32],uint64_t nonce,stnc_stratum_result *result)
{(void)client;(void)work_id;(void)nonce;if(result!=NULL)*result=STNC_STRATUM_RESULT_ACCEPTED;return 0;}
int stnc_stratum_client_progress(stnc_stratum_client *client,const uint8_t work_id[32],uint64_t hashes,uint64_t elapsed_ms)
{(void)client;(void)work_id;(void)hashes;(void)elapsed_ms;progress_calls++;return 0;}
stnc_mining_result stnc_mining_search_target_timed(uint8_t block[STNC_STNC_BLOCK_HEADER_SIZE],const uint8_t target[32],uint64_t first_nonce,unsigned int budget_ms,uint64_t *attempts,uint64_t *found_nonce,uint8_t digest[32])
{
    (void)block;(void)target;(void)first_nonce;(void)found_nonce;
    if(budget_ms!=20u)return STNC_MINING_ERROR;
    search_calls++;*attempts=25u;memset(digest,0,32u);clock_ms+=20u;return STNC_MINING_EXHAUSTED;
}

static int fail_at(int line){fprintf(stderr,"Background mining test failed at line %d.\n",line);return 1;}
#define CHECK(x) do{if(!(x))return fail_at(__LINE__);}while(0)

int main(void)
{
    stnc_mining_service_status status;
    memset(&config,0,sizeof(config));
    memcpy(config.mining_backend,"automatic",sizeof("automatic"));
    memcpy(config.stratum_host,"stratum.stn-chain.org",sizeof("stratum.stn-chain.org"));
    config.stratum_port=18475u;config.mining_cpu_limit_percent=2u;config.mining_enabled=1;

    identity_ok=0;
    CHECK(stnc_background_mining_init()==0);
    stnc_background_mining_tick();stnc_background_mining_status(&status);
    CHECK(!status.running&&connect_calls==0u&&search_calls==0u);
    stnc_background_mining_shutdown();

    identity_ok=1;job_ready=1;clock_ms=1000u;
    CHECK(stnc_background_mining_init()==0);
    stnc_background_mining_tick();stnc_background_mining_status(&status);
    CHECK(connect_calls==1u&&poll_calls==1u&&search_calls==1u&&progress_calls==1u);
    CHECK(status.running&&status.active_backend==STNC_MINING_BACKEND_CPU);
    CHECK(status.hashrate_hps==25u);
    CHECK(strlen(observed_address)==69u&&strncmp(observed_address,"stn0_",5u)==0);

    memcpy(config.stratum_host,"stratum2.stn-chain.org",sizeof("stratum2.stn-chain.org"));
    config.stratum_port=18476u;job_ready=1;clock_ms=2000u;
    stnc_background_mining_tick();stnc_background_mining_status(&status);
    CHECK(connect_calls==2u&&status.running);
    CHECK(strlen(observed_address)==69u&&strncmp(observed_address,"stn0_",5u)==0);

    config.mining_enabled=0;clock_ms=3000u;stnc_background_mining_tick();
    stnc_background_mining_status(&status);CHECK(!status.enabled&&!status.running);
    stnc_background_mining_shutdown();

    puts("Background mining identity tests passed.");
    return 0;
}
