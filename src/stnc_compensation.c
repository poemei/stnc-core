#include "stnc_compensation.h"
#include "stnc_core.h"
#include "stnc_identity.h"
#include "stnc_wallet.h"
#include "stnc_wallet_store.h"
#include <string.h>

static int hex(unsigned char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1;}
static int decode(const char *text,const char *prefix,size_t prefix_length,size_t text_length,uint8_t id[32])
{
    size_t i;
    if(text==NULL||id==NULL||strlen(text)!=text_length||memcmp(text,prefix,prefix_length)!=0)return 1;
    for(i=0;i<32u;i++){int h=hex((unsigned char)text[prefix_length+i*2u]),l=hex((unsigned char)text[prefix_length+i*2u+1u]);if(h<0||l<0)return 1;id[i]=(uint8_t)((h<<4)|l);}
    return 0;
}
static void put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void put32(uint8_t *p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}

int stnc_compensation_build(const char *identity,const char *wallet,uint8_t transaction[STNC_COMPENSATION_TRANSACTION_SIZE])
{
    uint8_t identity_id[32],wallet_id[32];
    if(transaction==NULL||decode(identity,"stn0_",5u,69u,identity_id)!=0||decode(wallet,"stnw0_",6u,70u,wallet_id)!=0)return 1;
    memcpy(transaction,"STNT",4u);put16(transaction+4u,1u);put16(transaction+6u,7u);put32(transaction+8u,65u);
    transaction[12]=1u;memcpy(transaction+13u,identity_id,32u);memcpy(transaction+45u,wallet_id,32u);
    memset(identity_id,0,sizeof(identity_id));memset(wallet_id,0,sizeof(wallet_id));return 0;
}

int stnc_compensation_ensure(void)
{
    stnc_identity_status identity;stnc_wallet_key key;stnc_submission_result result;
    char wallet[STNC_WALLET_ADDRESS_SIZE+1u];uint8_t transaction[STNC_COMPENSATION_TRANSACTION_SIZE];int rc=1;
    memset(&identity,0,sizeof(identity));memset(&key,0,sizeof(key));memset(&result,0,sizeof(result));memset(wallet,0,sizeof(wallet));memset(transaction,0,sizeof(transaction));
    if(stnc_identity_status_read(&identity)!=0||!identity.present||!identity.valid||
       stnc_wallet_store_load(&key)!=0||stnc_wallet_address(&key,wallet)!=0||
       stnc_compensation_build(identity.address,wallet,transaction)!=0||
       stnc_core_submit_transaction(transaction,sizeof(transaction),&result)!=0)goto done;
    if(result.result==STNC_STNC_SUBMISSION_ADMITTED||result.result==STNC_STNC_SUBMISSION_DUPLICATE)rc=0;
done:
    stnc_wallet_clear(&key);memset(transaction,0,sizeof(transaction));return rc;
}
