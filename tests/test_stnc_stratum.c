#include <stdio.h>
#include <string.h>
#include "stnc_stratum.h"

static void w32(uint8_t *p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static void w64(uint8_t *p,uint64_t v){size_t i;for(i=0u;i<8u;i++){p[7u-i]=(uint8_t)(v&0xffu);v>>=8;}}
int main(void)
{
    uint8_t h[116]={0},block[168]={0},frame[77],submit[48],progress[56],result[12]={0};
    stnc_stratum_job job;stnc_stratum_result code;char address[70];size_t i;
    h[0]='S';h[1]='T';h[2]='N';h[3]='M';h[4]=1u;h[5]=1u;
    for(i=0u;i<32u;i++){h[8u+i]=(uint8_t)i;h[40u+i]=(uint8_t)(i+1u);h[72u+i]=(uint8_t)(i+2u);}
    w32(h+104u,168u);w64(h+108u,9u);
    if(stnc_stratum_parse_job_header(h,&job)!=0||job.block_length!=168u||job.initial_nonce!=9u)return 1;
    memcpy(block+120u,job.chain_target,32u);w64(block+152u,9u);
    if(stnc_stratum_validate_job_block(&job,block,sizeof(block))!=0)return 1;
    block[120]^=1u;if(stnc_stratum_validate_job_block(&job,block,sizeof(block))==0)return 1;block[120]^=1u;
    w64(block+152u,10u);if(stnc_stratum_validate_job_block(&job,block,sizeof(block))==0)return 1;
    h[6]=1u;if(stnc_stratum_parse_job_header(h,&job)==0)return 1;h[6]=0u;
    w32(h+104u,167u);if(stnc_stratum_parse_job_header(h,&job)==0)return 1;w32(h+104u,168u);

    memcpy(address,"stn0_",5u);for(i=5u;i<69u;i++)address[i]='a';address[69]='\0';
    if(stnc_stratum_build_address(address,frame)!=0||frame[5]!=5u||memcmp(frame+8u,address,69u)!=0)return 1;
    address[5]='G';if(stnc_stratum_build_address(address,frame)==0)return 1;address[5]='a';

    if(stnc_stratum_build_submit(h+8u,UINT64_C(0x0102030405060708),submit)!=0||submit[5]!=2u||
       submit[40]!=1u||submit[47]!=8u)return 1;
    if(stnc_stratum_build_hash_progress(h+8u,11u,22u,progress)!=0||progress[5]!=4u||progress[47]!=11u||progress[55]!=22u)return 1;

    result[0]='S';result[1]='T';result[2]='N';result[3]='M';result[4]=1u;result[5]=3u;w32(result+8u,0u);
    if(stnc_stratum_parse_result(result,&code)!=0||code!=STNC_STRATUM_RESULT_ACCEPTED)return 1;
    w32(result+8u,5u);if(stnc_stratum_parse_result(result,&code)==0)return 1;
    result[0]='X';if(stnc_stratum_parse_result(result,&code)==0)return 1;
    puts("STNM protocol tests passed.");return 0;
}
