#include "stnc_mining.h"
#include "stnc_platform.h"
#include <string.h>
static void put64(uint8_t *p,uint64_t v){size_t i;for(i=0u;i<8u;i++)p[7u-i]=(uint8_t)(v>>(i*8u));}
int stnc_mining_hash(const uint8_t block[STNC_STNC_BLOCK_HEADER_SIZE],uint8_t digest[32])
{
    static const uint8_t domain[]="STN-CHAIN:BLOCK:ID:1";
    uint8_t input[STNC_MINING_HASH_INPUT_SIZE];
    if(block==NULL||digest==NULL)return 1;
    memcpy(input,domain,sizeof(domain)-1u);input[sizeof(domain)-1u]=0u;
    memcpy(input+sizeof(domain),block,STNC_STNC_BLOCK_HEADER_SIZE);
    if(stnc_platform_sha256(input,sizeof(input),digest)!=0){
        stnc_platform_secure_clear(input,sizeof(input));
        return 1;
    }
    stnc_platform_secure_clear(input,sizeof(input));
    return 0;
}
int stnc_mining_hash_meets_target(const uint8_t digest[32],const uint8_t target[32])
{
    size_t i;if(digest==NULL||target==NULL)return 0;
    for(i=0u;i<32u;i++){if(digest[i]<target[i])return 1;if(digest[i]>target[i])return 0;}return 1;
}
stnc_mining_result stnc_mining_search(uint8_t block[STNC_STNC_BLOCK_HEADER_SIZE],
    uint64_t first_nonce,uint64_t attempts,uint64_t *found_nonce,uint8_t digest[32])
{
    uint8_t hash[32],target[32];uint64_t i,nonce;
    if(block==NULL||found_nonce==NULL||digest==NULL||attempts==0u)return STNC_MINING_ERROR;
    *found_nonce=0u;
    memset(digest,0,32u);
    memcpy(target,block+120u,32u);
    for(i=0u;i<attempts;i++){
        if(i>UINT64_MAX-first_nonce)return STNC_MINING_EXHAUSTED;
        nonce=first_nonce+i;
        put64(block+STNC_STNC_MINING_NONCE_OFFSET,nonce);
        if(stnc_mining_hash(block,hash)!=0){
            stnc_platform_secure_clear(hash,sizeof(hash));
            stnc_platform_secure_clear(target,sizeof(target));
            return STNC_MINING_ERROR;
        }
        if(stnc_mining_hash_meets_target(hash,target)){
            *found_nonce=nonce;memcpy(digest,hash,32u);
            stnc_platform_secure_clear(hash,sizeof(hash));
            stnc_platform_secure_clear(target,sizeof(target));
            return STNC_MINING_FOUND;
        }
    }
    stnc_platform_secure_clear(hash,sizeof(hash));
    stnc_platform_secure_clear(target,sizeof(target));
    return STNC_MINING_EXHAUSTED;
}
