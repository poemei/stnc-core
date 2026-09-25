#include <stdio.h>
#include <string.h>
#include "stnc_mining.h"
int stnc_platform_sha256(const unsigned char *buffer,size_t length,unsigned char digest[32])
{
    size_t i;(void)buffer;(void)length;
    for(i=0u;i<32u;i++)digest[i]=0u;
    return 0;
}
int main(void)
{
    uint8_t block[STNC_STNC_BLOCK_HEADER_SIZE]={0},digest[32],target[32]={0};
    uint64_t nonce=99u;
    target[31]=1u;
    if(!stnc_mining_hash_meets_target(digest,target))return 1;
    digest[31]=2u;if(stnc_mining_hash_meets_target(digest,target))return 1;
    memset(block+120u,0xff,32u);
    if(stnc_mining_search(block,UINT64_C(7),1u,&nonce,digest)!=STNC_MINING_FOUND||
       nonce!=UINT64_C(7)||block[159]!=7u)return 1;
    if(stnc_mining_search(block,0u,0u,&nonce,digest)!=STNC_MINING_ERROR)return 1;
    puts("STNC mining tests passed.");return 0;
}
