#include <stdio.h>
#include <string.h>
#include "stnc_mining.h"

static uint8_t last_input[STNC_MINING_HASH_INPUT_SIZE];
static size_t last_length;
static unsigned int sha_calls;
static int sha_fail;
static unsigned int clear_calls;
static size_t clear_length;

void stnc_platform_secure_clear(void *buffer,size_t length)
{
    volatile uint8_t *p=(volatile uint8_t *)buffer;
    clear_calls++;clear_length=length;
    while(length>0u){*p++=0u;length--;}
}

int stnc_platform_sha256(const unsigned char *buffer,size_t length,unsigned char digest[32])
{
    size_t i;
    sha_calls++;
    if(sha_fail)return 1;
    if(buffer==NULL||digest==NULL||length>sizeof(last_input))return 1;
    memcpy(last_input,buffer,length);last_length=length;
    for(i=0u;i<32u;i++)digest[i]=0u;
    return 0;
}

int main(void)
{
    static const uint8_t domain[]="STN-CHAIN:BLOCK:ID:1";
    uint8_t block[STNC_STNC_BLOCK_HEADER_SIZE]={0},original[STNC_STNC_BLOCK_HEADER_SIZE];
    uint8_t digest[32]={0},target[32]={0};uint64_t nonce=99u;size_t i;

    target[31]=1u;
    if(!stnc_mining_hash_meets_target(digest,target))return 1;
    memcpy(digest,target,sizeof(digest));
    if(!stnc_mining_hash_meets_target(digest,target))return 1;
    digest[31]=2u;if(stnc_mining_hash_meets_target(digest,target))return 1;
    memset(digest,0,sizeof(digest));memset(target,0,sizeof(target));
    digest[0]=1u;
    if(stnc_mining_hash_meets_target(digest,target))return 1;
    digest[0]=0u;target[0]=1u;
    if(!stnc_mining_hash_meets_target(digest,target))return 1;

    for(i=0u;i<sizeof(block);i++)block[i]=(uint8_t)i;
    clear_calls=0u;clear_length=0u;
    if(stnc_mining_hash(block,digest)!=0||
       last_length!=STNC_MINING_HASH_INPUT_SIZE||
       memcmp(last_input,domain,sizeof(domain))!=0||
       memcmp(last_input+sizeof(domain),block,sizeof(block))!=0||
       clear_calls!=1u||clear_length!=STNC_MINING_HASH_INPUT_SIZE)return 1;

    memset(block,0,sizeof(block));memset(block+120u,0xff,32u);memcpy(original,block,sizeof(block));
    sha_calls=0u;
    if(stnc_mining_search(block,UINT64_C(0x0102030405060708),1u,&nonce,digest)!=STNC_MINING_FOUND||
       nonce!=UINT64_C(0x0102030405060708)||sha_calls!=1u)return 1;
    if(memcmp(block,original,STNC_STNC_MINING_NONCE_OFFSET)!=0||
       memcmp(block+STNC_STNC_MINING_NONCE_OFFSET+STNC_STNC_MINING_NONCE_SIZE,
              original+STNC_STNC_MINING_NONCE_OFFSET+STNC_STNC_MINING_NONCE_SIZE,
              sizeof(block)-STNC_STNC_MINING_NONCE_OFFSET-STNC_STNC_MINING_NONCE_SIZE)!=0)return 1;
    if(block[152]!=0x01u||block[153]!=0x02u||block[154]!=0x03u||block[155]!=0x04u||
       block[156]!=0x05u||block[157]!=0x06u||block[158]!=0x07u||block[159]!=0x08u)return 1;

    memset(block+120u,0u,32u);sha_calls=0u;
    if(stnc_mining_search(block,UINT64_MAX,2u,&nonce,digest)!=STNC_MINING_EXHAUSTED||sha_calls!=1u)return 1;

    memset(block,0,sizeof(block));memset(block+120u,0u,32u);memcpy(original,block,sizeof(block));sha_calls=0u;
    if(stnc_mining_search(block,UINT64_C(9),3u,&nonce,digest)!=STNC_MINING_EXHAUSTED||
       sha_calls!=3u||block[152]!=0u||block[153]!=0u||block[154]!=0u||block[155]!=0u||
       block[156]!=0u||block[157]!=0u||block[158]!=0u||block[159]!=11u)return 1;
    if(memcmp(block,original,STNC_STNC_MINING_NONCE_OFFSET)!=0||
       memcmp(block+STNC_STNC_MINING_NONCE_OFFSET+STNC_STNC_MINING_NONCE_SIZE,
              original+STNC_STNC_MINING_NONCE_OFFSET+STNC_STNC_MINING_NONCE_SIZE,
              sizeof(block)-STNC_STNC_MINING_NONCE_OFFSET-STNC_STNC_MINING_NONCE_SIZE)!=0)return 1;

    memset(block+120u,0xff,32u);sha_fail=1;clear_calls=0u;clear_length=0u;
    if(stnc_mining_search(block,0u,1u,&nonce,digest)!=STNC_MINING_ERROR||
       clear_calls!=1u||clear_length!=STNC_MINING_HASH_INPUT_SIZE)return 1;
    sha_fail=0;

    sha_calls=0u;
    if(stnc_mining_search(block,0u,0u,&nonce,digest)!=STNC_MINING_ERROR||sha_calls!=0u)return 1;
    if(stnc_mining_search(NULL,0u,1u,&nonce,digest)!=STNC_MINING_ERROR||sha_calls!=0u)return 1;
    if(stnc_mining_search(block,0u,1u,NULL,digest)!=STNC_MINING_ERROR||sha_calls!=0u)return 1;
    if(stnc_mining_search(block,0u,1u,&nonce,NULL)!=STNC_MINING_ERROR||sha_calls!=0u)return 1;

    sha_calls=0u;
    if(stnc_mining_hash(NULL,digest)!=1||sha_calls!=0u)return 1;
    if(stnc_mining_hash(block,NULL)!=1||sha_calls!=0u)return 1;
    if(stnc_mining_hash_meets_target(NULL,target)!=0||
       stnc_mining_hash_meets_target(digest,NULL)!=0)return 1;

    puts("STNC mining tests passed.");return 0;
}
