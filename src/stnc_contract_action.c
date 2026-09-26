#include "stnc_contract_action.h"
#include "stnc_contract_draft.h"
#include "stnc_identity.h"
#include "stnc_platform.h"
#include <stdlib.h>
#include <string.h>
static uint64_t get(const uint8_t *p,size_t n)
{uint64_t v=0;size_t i;for(i=0;i<n;++i)v=(v<<8)|p[i];return v;}
static void put(uint8_t *p,size_t n,uint64_t v)
{size_t i;for(i=0;i<n;++i)p[n-1-i]=(uint8_t)(v>>(8*i));}
int stnc_contract_action_build(const uint8_t *current,size_t length,uint16_t action,
    const uint8_t *authority,size_t authority_length,uint8_t *transaction,
    size_t capacity,size_t *written)
{
    static const uint8_t domain[24]="STN-CHAIN:RECORD:SIGN:1";
    stnc_identity_status identity;uint8_t *statement=NULL,*draft=NULL,signature[64],digest[32],token[32]={0};
    size_t count,terms,i,total;uint64_t sequence;int rc=1;
    if(written)*written=0;
    if(!current||!authority||!transaction||!written||length<32||length>STNC_CONTRACT_DRAFT_MAX||
       authority_length!=97||action<1||action>7||memcmp(current,"STCT",4)!=0||get(current+4,2)!=1||
       get(current+6,2)<1||get(current+6,2)>6||get(current+24,2)<1||get(current+24,2)>9)return 1;
    count=(size_t)get(current+26,2);terms=(size_t)get(current+28,4);sequence=get(current+8,8);
    if(count>32||terms>65536||length!=32+34*count+terms||sequence==UINT64_MAX)return 1;
    for(i=0;i<count;++i){uint64_t role=get(current+32+34*i+32,2);if(role<1||role>5)return 1;}
    total=12+116+length+97;if(capacity<total)return 1;
    if(stnc_identity_status_read(&identity)!=0||!identity.valid||authority[0]!=1||
       memcmp(authority+1,identity.public_key,32)!=0)return 1;
    token[0]=1;token[1]=0x43;put(token+2,2,action);
    if(memcmp(authority+33,token,32)!=0)return 1;
    draft=(uint8_t *)malloc(length);statement=(uint8_t *)malloc(length+66);
    if(!draft||!statement)goto done;
    memcpy(draft,current,length);memset(draft+8,0,8);put(draft+24,2,1);
    if(stnc_platform_sha256(draft,length,digest)!=0)goto done;
    memset(token,0,32);token[0]=1;token[1]=0x43;memcpy(token+2,digest,30);
    if(memcmp(authority+65,token,32)!=0)goto done;
    memcpy(statement,domain,24);memcpy(statement+24,current,length);memcpy(statement+24+length,identity.public_key,32);
    put(statement+56+length,2,action);put(statement+58+length,8,sequence+1);
    if(stnc_identity_sign(statement,length+66,signature)!=0)goto done;
    memcpy(transaction,"STNT",4);put(transaction+4,2,1);put(transaction+6,2,5);put(transaction+8,4,total-12);
    put(transaction+12,2,1);put(transaction+14,2,action);put(transaction+16,8,sequence+1);
    put(transaction+24,4,length);put(transaction+28,4,97);memcpy(transaction+32,identity.public_key,32);
    memcpy(transaction+64,signature,64);memcpy(transaction+128,current,length);memcpy(transaction+128+length,authority,97);
    *written=total;rc=0;
done:
    stnc_platform_secure_clear(signature,sizeof(signature));free(draft);free(statement);return rc;
}
