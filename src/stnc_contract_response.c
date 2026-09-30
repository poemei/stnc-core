#include "stnc_contract_response.h"
#include "stnc_identity.h"
#include "stnc_platform.h"
#include <stdlib.h>
#include <string.h>

static void put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void put32(uint8_t *p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static int hex_nibble(char c,uint8_t *v)
{
    if(c>='0'&&c<='9'){*v=(uint8_t)(c-'0');return 0;}
    if(c>='a'&&c<='f'){*v=(uint8_t)(c-'a'+10);return 0;}
    return 1;
}
static int contract_id_decode(const char *address,uint8_t id[32])
{
    size_t i;uint8_t hi,lo;
    if(address==NULL||strlen(address)!=70u||memcmp(address,"stnc0_",6)!=0)return 1;
    for(i=0u;i<32u;++i){
        if(hex_nibble(address[6u+2u*i],&hi)!=0||hex_nibble(address[7u+2u*i],&lo)!=0)return 1;
        id[i]=(uint8_t)((hi<<4)|lo);
    }
    return 0;
}

int stnc_contract_response_build(const char *contract_address,
    const uint8_t *text,size_t text_length,uint8_t *transaction,
    size_t capacity,size_t *written)
{
    static const uint8_t identity_domain[24]="STN-CHAIN:IDENTITY:SIGN:1";
    stnc_identity_status identity;uint8_t contract_id[32],signature[64];
    uint8_t *unsigned_response=NULL,*statement=NULL;size_t unsigned_length,response_length,total;int rc=1;
    if(written!=NULL)*written=0u;
    if(contract_address==NULL||text==NULL||transaction==NULL||written==NULL||
       text_length==0u||text_length>STNC_CONTRACT_RESPONSE_MAX_TEXT)return 1;
    if(contract_id_decode(contract_address,contract_id)!=0)return 1;
    memset(&identity,0,sizeof(identity));
    if(stnc_identity_status_read(&identity)!=0||!identity.valid)return 1;
    unsigned_length=76u+text_length;
    response_length=STNC_CONTRACT_RESPONSE_HEADER_SIZE+text_length;
    total=STNC_CONTRACT_RESPONSE_TRANSACTION_HEADER_SIZE+response_length;
    if(capacity<total)return 1;
    unsigned_response=(uint8_t*)malloc(unsigned_length);
    statement=(uint8_t*)malloc(sizeof(identity_domain)+unsigned_length);
    if(unsigned_response==NULL||statement==NULL)goto done;
    memcpy(unsigned_response,"STRP",4);put16(unsigned_response+4,STNC_CONTRACT_RESPONSE_VERSION);put16(unsigned_response+6,0u);
    memcpy(unsigned_response+8,contract_id,32);memcpy(unsigned_response+40,identity.public_key,32);put32(unsigned_response+72,(uint32_t)text_length);memcpy(unsigned_response+76,text,text_length);
    memcpy(statement,identity_domain,sizeof(identity_domain));memcpy(statement+sizeof(identity_domain),unsigned_response,unsigned_length);
    if(stnc_identity_sign(statement,sizeof(identity_domain)+unsigned_length,signature)!=0)goto done;
    memcpy(transaction,"STNT",4);put16(transaction+4,1u);put16(transaction+6,11u);put32(transaction+8,(uint32_t)response_length);
    memcpy(transaction+12,unsigned_response,76u);memcpy(transaction+88,signature,64u);memcpy(transaction+152,text,text_length);
    *written=total;rc=0;
done:
    stnc_platform_secure_clear(signature,sizeof(signature));
    if(statement!=NULL)stnc_platform_secure_clear(statement,sizeof(identity_domain)+unsigned_length);
    if(unsigned_response!=NULL)stnc_platform_secure_clear(unsigned_response,unsigned_length);
    free(statement);free(unsigned_response);return rc;
}
