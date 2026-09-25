#include <stdio.h>
#include <string.h>
#include "stnc_mining.h"

static uint8_t last_input[STNC_MINING_HASH_INPUT_SIZE];
static size_t last_length;
static unsigned int sha_calls;
static int sha_fail;
static int sha_partial_fail;
static int sha_nonzero;
static unsigned int clear_calls;
static size_t clear_length;
static size_t clear_lengths[8];
static uint64_t monotonic_value;
static uint64_t monotonic_step;

uint64_t stnc_platform_monotonic_ms(void)
{
    uint64_t value=monotonic_value;
    monotonic_value+=monotonic_step;
    return value;
}

void stnc_platform_secure_clear(void *buffer,size_t length)
{
    volatile uint8_t *p=(volatile uint8_t *)buffer;
    if(clear_calls<sizeof(clear_lengths)/sizeof(clear_lengths[0]))clear_lengths[clear_calls]=length;
    clear_calls++;clear_length=length;
    while(length>0u){*p++=0u;length--;}
}

int stnc_platform_sha256(const unsigned char *buffer,size_t length,unsigned char digest[32])
{
    size_t i;
    sha_calls++;
    if(sha_fail)return 1;
    if(buffer==NULL||digest==NULL||length>sizeof(last_input))return 1;
    if(sha_partial_fail){digest[0]=0xffu;digest[31]=0xffu;return 1;}
    memcpy(last_input,buffer,length);last_length=length;
    for(i=0u;i<32u;i++)digest[i]=0u;
    if(sha_nonzero)digest[0]=1u;
    return 0;
}

static int fail_at(int line){fprintf(stderr,"STNC mining test failed at line %d.\n",line);return 1;}
#define TEST_FAIL() fail_at(__LINE__)

int main(void)
{
    static const uint8_t domain[]="STN-CHAIN:BLOCK:ID:1";
    uint8_t block[STNC_STNC_BLOCK_HEADER_SIZE]={0},original[STNC_STNC_BLOCK_HEADER_SIZE];
    uint8_t digest[32]={0},target[32]={0};uint64_t nonce=99u,attempts=0u;size_t i;

    if(sizeof(domain)!=STNC_MINING_HASH_DOMAIN_SIZE||
       STNC_MINING_HASH_INPUT_SIZE!=sizeof(domain)+STNC_STNC_BLOCK_HEADER_SIZE)return TEST_FAIL();

    target[31]=1u;
    if(!stnc_mining_hash_meets_target(digest,target))return TEST_FAIL();
    memcpy(digest,target,sizeof(digest));
    if(!stnc_mining_hash_meets_target(digest,target))return TEST_FAIL();
    digest[31]=2u;if(stnc_mining_hash_meets_target(digest,target))return TEST_FAIL();
    memset(digest,0,sizeof(digest));memset(target,0,sizeof(target));
    digest[0]=1u;
    if(stnc_mining_hash_meets_target(digest,target))return TEST_FAIL();
    digest[0]=0u;target[0]=1u;
    if(!stnc_mining_hash_meets_target(digest,target))return TEST_FAIL();

    for(i=0u;i<sizeof(block);i++)block[i]=(uint8_t)i;
    clear_calls=0u;clear_length=0u;
    if(stnc_mining_hash(block,digest)!=0||
       last_length!=STNC_MINING_HASH_INPUT_SIZE||
       memcmp(last_input,domain,sizeof(domain))!=0||
       memcmp(last_input+sizeof(domain),block,sizeof(block))!=0||
       clear_calls!=1u||clear_length!=STNC_MINING_HASH_INPUT_SIZE)return TEST_FAIL();

    memset(block,0,sizeof(block));memset(block+120u,0xff,32u);memcpy(original,block,sizeof(block));
    sha_calls=0u;clear_calls=0u;memset(clear_lengths,0,sizeof(clear_lengths));
    if(stnc_mining_search(block,UINT64_C(0x0102030405060708),1u,&nonce,digest)!=STNC_MINING_FOUND||
       nonce!=UINT64_C(0x0102030405060708)||sha_calls!=1u||clear_calls!=3u||
       clear_lengths[0]!=STNC_MINING_HASH_INPUT_SIZE||clear_lengths[1]!=32u||clear_lengths[2]!=32u)return TEST_FAIL();
    if(memcmp(block,original,STNC_STNC_MINING_NONCE_OFFSET)!=0||
       memcmp(block+STNC_STNC_MINING_NONCE_OFFSET+STNC_STNC_MINING_NONCE_SIZE,
              original+STNC_STNC_MINING_NONCE_OFFSET+STNC_STNC_MINING_NONCE_SIZE,
              sizeof(block)-STNC_STNC_MINING_NONCE_OFFSET-STNC_STNC_MINING_NONCE_SIZE)!=0)return TEST_FAIL();
    if(block[152]!=0x01u||block[153]!=0x02u||block[154]!=0x03u||block[155]!=0x04u||
       block[156]!=0x05u||block[157]!=0x06u||block[158]!=0x07u||block[159]!=0x08u)return TEST_FAIL();

    memset(block+120u,0u,32u);nonce=UINT64_MAX;memset(digest,0xa5,sizeof(digest));
    sha_nonzero=1;sha_calls=0u;clear_calls=0u;clear_length=0u;memset(clear_lengths,0,sizeof(clear_lengths));
    if(stnc_mining_search(block,UINT64_MAX,2u,&nonce,digest)!=STNC_MINING_EXHAUSTED)return TEST_FAIL();
    sha_nonzero=0;
    if(sha_calls!=1u||nonce!=0u||digest[0]!=0u||digest[31]!=0u)return TEST_FAIL();
    if(clear_calls!=3u){
        fprintf(stderr,"overflow cleanup clear_calls=%u expected=3\n",clear_calls);return TEST_FAIL();
    }
    if(clear_lengths[0]!=STNC_MINING_HASH_INPUT_SIZE||clear_lengths[1]!=32u||
       clear_lengths[2]!=32u||clear_length!=32u){
        fprintf(stderr,"overflow cleanup lengths=%llu,%llu,%llu last=%llu expected=%u,32,32\n",
            (unsigned long long)clear_lengths[0],(unsigned long long)clear_lengths[1],
            (unsigned long long)clear_lengths[2],(unsigned long long)clear_length,
            (unsigned int)STNC_MINING_HASH_INPUT_SIZE);
        return TEST_FAIL();
    }

    memset(block,0,sizeof(block));memset(block+120u,0u,32u);memcpy(original,block,sizeof(block));
    nonce=UINT64_MAX;memset(digest,0xa5,sizeof(digest));sha_calls=0u;
    if(stnc_mining_search(block,UINT64_C(9),3u,&nonce,digest)!=STNC_MINING_EXHAUSTED||
       sha_calls!=3u||nonce!=0u||digest[0]!=0u||digest[31]!=0u||
       block[152]!=0u||block[153]!=0u||block[154]!=0u||block[155]!=0u||
       block[156]!=0u||block[157]!=0u||block[158]!=0u||block[159]!=11u)return TEST_FAIL();
    if(memcmp(block,original,STNC_STNC_MINING_NONCE_OFFSET)!=0||
       memcmp(block+STNC_STNC_MINING_NONCE_OFFSET+STNC_STNC_MINING_NONCE_SIZE,
              original+STNC_STNC_MINING_NONCE_OFFSET+STNC_STNC_MINING_NONCE_SIZE,
              sizeof(block)-STNC_STNC_MINING_NONCE_OFFSET-STNC_STNC_MINING_NONCE_SIZE)!=0)return TEST_FAIL();

    memset(block+120u,0xff,32u);sha_fail=1;nonce=UINT64_MAX;memset(digest,0xa5,sizeof(digest));
    clear_calls=0u;clear_length=0u;memset(clear_lengths,0,sizeof(clear_lengths));
    if(stnc_mining_search(block,0u,1u,&nonce,digest)!=STNC_MINING_ERROR||
       clear_calls!=3u||clear_lengths[0]!=STNC_MINING_HASH_INPUT_SIZE||
       clear_lengths[1]!=32u||clear_lengths[2]!=32u||nonce!=0u||
       digest[0]!=0u||digest[31]!=0u)return TEST_FAIL();
    sha_fail=0;

    memset(block+120u,0xff,32u);sha_partial_fail=1;nonce=UINT64_MAX;memset(digest,0xa5,sizeof(digest));
    if(stnc_mining_search(block,0u,1u,&nonce,digest)!=STNC_MINING_ERROR||
       nonce!=0u||digest[0]!=0u||digest[31]!=0u)return TEST_FAIL();
    sha_partial_fail=0;

    memset(block,0,sizeof(block));memset(block+120u,0u,32u);
    monotonic_value=100u;monotonic_step=5u;sha_calls=0u;attempts=UINT64_MAX;nonce=UINT64_MAX;
    if(stnc_mining_search_timed(block,7u,20u,&attempts,&nonce,digest)!=STNC_MINING_EXHAUSTED||
       attempts!=4u||sha_calls!=4u||nonce!=0u)return TEST_FAIL();
    memset(block+120u,0xff,32u);
    monotonic_value=0u;monotonic_step=1u;attempts=0u;nonce=0u;
    if(stnc_mining_search_timed(block,9u,20u,&attempts,&nonce,digest)!=STNC_MINING_FOUND||
       attempts!=1u||nonce!=9u)return TEST_FAIL();
    if(stnc_mining_search_timed(block,0u,0u,&attempts,&nonce,digest)!=STNC_MINING_ERROR)return TEST_FAIL();
    if(stnc_mining_search_timed(NULL,0u,1u,&attempts,&nonce,digest)!=STNC_MINING_ERROR)return TEST_FAIL();

    sha_calls=0u;
    if(stnc_mining_search(block,0u,0u,&nonce,digest)!=STNC_MINING_ERROR||sha_calls!=0u)return TEST_FAIL();
    if(stnc_mining_search(NULL,0u,1u,&nonce,digest)!=STNC_MINING_ERROR||sha_calls!=0u)return TEST_FAIL();
    if(stnc_mining_search(block,0u,1u,NULL,digest)!=STNC_MINING_ERROR||sha_calls!=0u)return TEST_FAIL();
    if(stnc_mining_search(block,0u,1u,&nonce,NULL)!=STNC_MINING_ERROR||sha_calls!=0u)return TEST_FAIL();

    sha_calls=0u;
    if(stnc_mining_hash(NULL,digest)!=1||sha_calls!=0u)return TEST_FAIL();
    if(stnc_mining_hash(block,NULL)!=1||sha_calls!=0u)return TEST_FAIL();
    if(stnc_mining_hash_meets_target(NULL,target)!=0||
       stnc_mining_hash_meets_target(digest,NULL)!=0)return TEST_FAIL();

    puts("STNC mining tests passed.");return 0;
}
