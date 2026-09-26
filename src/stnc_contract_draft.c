#include "stnc_contract_draft.h"
#include "stnc_platform.h"
#include <stdlib.h>
#include <string.h>
static void put(uint8_t *p,size_t n,uint64_t v)
{size_t i;for(i=0;i<n;++i)p[n-1-i]=(uint8_t)(v>>(8*i));}
static int nibble(char c)
{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1;}
int stnc_contract_actor_decode(const char *address,uint8_t identifier[32])
{
    size_t i;uint8_t out[32];
    if(address==NULL||identifier==NULL||strlen(address)!=64)return 1;
    for(i=0;i<32;++i){int h=nibble(address[2*i]),l=nibble(address[1+2*i]);if(h<0||l<0)return 1;out[i]=(uint8_t)((h<<4)|l);}
    memcpy(identifier,out,32);return 0;
}
int stnc_contract_draft_build(const stnc_contract_draft_input *input,
    uint8_t *bytes,size_t capacity,size_t *written,char address[71])
{
    static const char hex[]="0123456789abcdef";
    size_t n,i,offset;uint8_t ids[32][32],digest[32];
    if(written!=NULL)*written=0;
    if(input==NULL||bytes==NULL||written==NULL||address==NULL||input->type<1||input->type>6||
       input->participant_count>32||input->terms_length>65536||
       (input->terms_length&&input->terms==NULL))return 1;
    n=32+input->participant_count*34+input->terms_length;if(capacity<n)return 1;
    for(i=0;i<input->participant_count;++i){
        if(input->participants[i].role<1||input->participants[i].role>5||
           memchr(input->participants[i].public_key,0,65)==NULL||
           stnc_contract_actor_decode(input->participants[i].public_key,ids[i])!=0)return 1;
    }
    memset(bytes,0,32);memcpy(bytes,"STCT",4);put(bytes+4,2,1);put(bytes+6,2,input->type);
    put(bytes+16,8,input->created_at);put(bytes+24,2,1);put(bytes+26,2,input->participant_count);
    put(bytes+28,4,input->terms_length);offset=32;
    for(i=0;i<input->participant_count;++i){memcpy(bytes+offset,ids[i],32);put(bytes+offset+32,2,input->participants[i].role);offset+=34;}
    if(input->terms_length)memcpy(bytes+offset,input->terms,input->terms_length);
    if(stnc_platform_sha256(bytes,n,digest)!=0)return 1;
    memcpy(address,"stnc0_",6);for(i=0;i<32;++i){address[6+2*i]=hex[digest[i]>>4];address[7+2*i]=hex[digest[i]&15];}
    address[70]=0;*written=n;return 0;
}
int stnc_contract_draft_save(const stnc_contract_draft_input *input,const char *path,char address[71])
{
    uint8_t *bytes;size_t n=0;int rc;
    if(path==NULL||!path[0])return 1;
    bytes=(uint8_t *)malloc(STNC_CONTRACT_DRAFT_MAX);if(bytes==NULL)return 1;
    rc=stnc_contract_draft_build(input,bytes,STNC_CONTRACT_DRAFT_MAX,&n,address);
    if(rc==0)rc=stnc_platform_write_private_file(path,bytes,n);
    free(bytes);return rc;
}
