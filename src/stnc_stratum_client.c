#include "stnc_stratum_client.h"
#include <string.h>
#include "stnc_platform.h"

void stnc_stratum_client_init(stnc_stratum_client *client){if(client!=NULL)memset(client,0,sizeof(*client));}

int stnc_stratum_client_connect(stnc_stratum_client *client,const char *host,unsigned short port,const char *address)
{
    uint8_t frame[STNC_STNM_ADDRESS_SIZE];
    if(client==NULL||host==NULL||host[0]=='\0'||port==0u||stnc_stratum_build_address(address,frame)!=0)return 1;
    if(client->connected)stnc_stratum_client_disconnect(client);
    if(stnc_platform_network_connect(&client->handle,host,port)!=0)return 1;
    if(stnc_platform_network_send(client->handle,frame,sizeof(frame))!=0){
        stnc_platform_network_disconnect(client->handle);client->handle=NULL;return 1;
    }
    client->connected=1;return 0;
}
void stnc_stratum_client_disconnect(stnc_stratum_client *client)
{
    if(client==NULL)return;if(client->handle!=NULL)stnc_platform_network_disconnect(client->handle);
    client->handle=NULL;client->connected=0;
}
int stnc_stratum_client_poll_job(stnc_stratum_client *client,stnc_stratum_job *job,uint8_t *block,size_t capacity)
{
    uint8_t header[STNC_STNM_JOB_HEADER_SIZE];
    int ready;
    if(client==NULL||!client->connected||job==NULL||block==NULL)return -1;
    ready=stnc_platform_network_read_ready(client->handle);
    if(ready==0)return 1;
    if(ready<0||
       stnc_platform_network_receive(client->handle,header,sizeof(header))!=0||
       stnc_stratum_parse_job_header(header,job)!=0||job->block_length>capacity||
       stnc_platform_network_receive(client->handle,block,job->block_length)!=0||
       stnc_stratum_validate_job_block(job,block,job->block_length)!=0)return -1;
    return 0;
}
int stnc_stratum_client_submit(stnc_stratum_client *client,const uint8_t work_id[32],uint64_t nonce,stnc_stratum_result *result)
{
    uint8_t request[STNC_STNM_SUBMIT_SIZE],response[STNC_STNM_RESULT_SIZE];
    if(client==NULL||!client->connected||result==NULL||stnc_stratum_build_submit(work_id,nonce,request)!=0)return 1;
    if(stnc_platform_network_send(client->handle,request,sizeof(request))!=0||
       stnc_platform_network_receive(client->handle,response,sizeof(response))!=0||
       stnc_stratum_parse_result(response,result)!=0)return 1;return 0;
}
int stnc_stratum_client_progress(stnc_stratum_client *client,const uint8_t work_id[32],uint64_t hashes,uint64_t elapsed_ms)
{
    uint8_t frame[STNC_STNM_HASH_PROGRESS_SIZE];
    if(client==NULL||!client->connected||stnc_stratum_build_hash_progress(work_id,hashes,elapsed_ms,frame)!=0)return 1;
    return stnc_platform_network_send(client->handle,frame,sizeof(frame));
}
