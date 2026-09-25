#include "stnc_stratum.h"
#include <string.h>

#define JOB_SHARE_TARGET 40u
#define JOB_CHAIN_TARGET 72u
#define JOB_BLOCK_LENGTH 104u
#define JOB_INITIAL_NONCE 108u
#define BLOCK_TARGET 120u
#define BLOCK_NONCE 152u

static int header_ok(const uint8_t *p,uint8_t type)
{return p!=NULL&&p[0]=='S'&&p[1]=='T'&&p[2]=='N'&&p[3]=='M'&&p[4]==STNC_STNM_VERSION&&p[5]==type&&p[6]==0u&&p[7]==0u;}
static uint32_t r32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static uint64_t r64(const uint8_t *p){uint64_t v=0u;size_t i;for(i=0u;i<8u;i++)v=(v<<8)|p[i];return v;}
static void w64(uint8_t *p,uint64_t v){size_t i;for(i=0u;i<8u;i++){p[7u-i]=(uint8_t)(v&0xffu);v>>=8;}}
static void init(uint8_t *p,size_t n,uint8_t type){memset(p,0,n);p[0]='S';p[1]='T';p[2]='N';p[3]='M';p[4]=STNC_STNM_VERSION;p[5]=type;}

int stnc_stratum_parse_job_header(const uint8_t header[STNC_STNM_JOB_HEADER_SIZE],stnc_stratum_job *job)
{
    if(!header_ok(header,1u)||job==NULL)return 1;
    memset(job,0,sizeof(*job));memcpy(job->work_id,header+8u,32u);
    memcpy(job->share_target,header+JOB_SHARE_TARGET,32u);memcpy(job->chain_target,header+JOB_CHAIN_TARGET,32u);
    job->block_length=r32(header+JOB_BLOCK_LENGTH);job->initial_nonce=r64(header+JOB_INITIAL_NONCE);
    if(job->block_length<STNC_STNM_BLOCK_HEADER_SIZE)return 1;return 0;
}
int stnc_stratum_validate_job_block(const stnc_stratum_job *job,const uint8_t *block,size_t length)
{
    if(job==NULL||block==NULL||length!=job->block_length||length<STNC_STNM_BLOCK_HEADER_SIZE)return 1;
    if(memcmp(job->chain_target,block+BLOCK_TARGET,32u)!=0)return 1;
    if(r64(block+BLOCK_NONCE)!=job->initial_nonce)return 1;return 0;
}
int stnc_stratum_build_address(const char *address,uint8_t frame[STNC_STNM_ADDRESS_SIZE])
{
    size_t i;if(address==NULL||frame==NULL||strlen(address)!=STNC_STNM_ADDRESS_LENGTH||memcmp(address,"stn0_",5u)!=0)return 1;
    for(i=5u;i<STNC_STNM_ADDRESS_LENGTH;i++)if(!((address[i]>='0'&&address[i]<='9')||(address[i]>='a'&&address[i]<='f')))return 1;
    init(frame,STNC_STNM_ADDRESS_SIZE,5u);memcpy(frame+8u,address,STNC_STNM_ADDRESS_LENGTH);return 0;
}
int stnc_stratum_build_submit(const uint8_t work_id[32],uint64_t nonce,uint8_t frame[STNC_STNM_SUBMIT_SIZE])
{
    if(work_id==NULL||frame==NULL)return 1;init(frame,STNC_STNM_SUBMIT_SIZE,2u);memcpy(frame+8u,work_id,32u);w64(frame+40u,nonce);return 0;
}
int stnc_stratum_build_hash_progress(const uint8_t work_id[32],uint64_t hashes,uint64_t elapsed_ms,uint8_t frame[STNC_STNM_HASH_PROGRESS_SIZE])
{
    if(work_id==NULL||frame==NULL)return 1;init(frame,STNC_STNM_HASH_PROGRESS_SIZE,4u);memcpy(frame+8u,work_id,32u);
    w64(frame+40u,hashes);w64(frame+48u,elapsed_ms);return 0;
}
int stnc_stratum_parse_result(const uint8_t frame[STNC_STNM_RESULT_SIZE],stnc_stratum_result *result)
{
    uint32_t code;if(!header_ok(frame,3u)||result==NULL)return 1;code=r32(frame+8u);if(code>4u)return 1;*result=(stnc_stratum_result)code;return 0;
}
