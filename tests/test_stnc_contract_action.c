#include "stnc_contract_action.h"
#include "stnc_identity.h"
#include <stdio.h>
#include <string.h>
static int valid=1,sign_error,signed_count;
static uint8_t signing_bytes[128];static size_t signing_length;
int stnc_identity_status_read(stnc_identity_status *status)
{memset(status,0,sizeof(*status));status->valid=valid;memset(status->public_key,0x11,32);return 0;}
int stnc_identity_sign(const uint8_t *bytes,size_t length,uint8_t signature[64])
{if(sign_error||length>sizeof(signing_bytes))return 1;memcpy(signing_bytes,bytes,length);signing_length=length;memset(signature,0x33,64);signed_count++;return 0;}
int stnc_platform_sha256(const uint8_t *bytes,size_t length,uint8_t digest[32])
{if(length!=32||bytes[25]!=1||bytes[15]!=0)return 1;memset(digest,0x22,32);return 0;}
void stnc_platform_secure_clear(void *p,size_t length){memset(p,0,length);}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Contract action test failed: %d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    uint8_t current[32]={0},authority[97]={0},transaction[512];size_t n;unsigned int action;
    memcpy(current,"STCT",4);current[5]=1;current[7]=1;current[25]=1;
    authority[0]=1;memset(authority+1,0x11,32);authority[33]=1;authority[34]=0x43;
    authority[65]=1;authority[66]=0x43;memset(authority+67,0x22,30);

    /* CREATE: sequence zero, signed identity, no pre-existing authority grant. */
    CHECK(stnc_contract_action_build(current,32,STNC_CONTRACT_ACTION_CREATE,NULL,0,transaction,sizeof(transaction),&n)==0);
    CHECK(n==160u);
    CHECK(memcmp(transaction,"STNT\0\1\0\5",8)==0&&transaction[15]==STNC_CONTRACT_ACTION_CREATE);
    CHECK(transaction[16]==0&&transaction[17]==0&&transaction[18]==0&&transaction[19]==0&&transaction[20]==0&&transaction[21]==0&&transaction[22]==0&&transaction[23]==0);
    CHECK(transaction[28]==0&&transaction[29]==0&&transaction[30]==0&&transaction[31]==0);
    CHECK(signing_length==98&&memcmp(signing_bytes,"STN-CHAIN:RECORD:SIGN:1",23)==0);
    CHECK(memcmp(transaction+128,current,32)==0);
    CHECK(stnc_contract_action_build(current,32,STNC_CONTRACT_ACTION_CREATE,authority,97,transaction,sizeof(transaction),&n)!=0);

    for(action=2;action<=7;++action){
        authority[36]=(uint8_t)action;
        CHECK(stnc_contract_action_build(current,32,(uint16_t)action,authority,97,transaction,sizeof(transaction),&n)==0&&n==257);
        CHECK(transaction[15]==action&&transaction[23]==1);
        CHECK(memcmp(transaction+128,current,32)==0&&memcmp(transaction+160,authority,97)==0);
    }
    CHECK(signed_count==7);
    CHECK(stnc_contract_action_build(current,32,2,NULL,0,transaction,sizeof(transaction),&n)!=0);
    valid=0;CHECK(stnc_contract_action_build(current,32,1,NULL,0,transaction,sizeof(transaction),&n)!=0);valid=1;
    sign_error=1;CHECK(stnc_contract_action_build(current,32,1,NULL,0,transaction,sizeof(transaction),&n)!=0);sign_error=0;
    CHECK(stnc_contract_action_build(current,32,8,authority,97,transaction,sizeof(transaction),&n)!=0);
    current[15]=1;CHECK(stnc_contract_action_build(current,32,1,NULL,0,transaction,sizeof(transaction),&n)!=0);current[15]=0;
    puts("STNC Contract action tests passed.");return 0;
}
