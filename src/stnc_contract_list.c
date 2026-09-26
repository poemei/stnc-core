#include "stnc_contract_list.h"
#include <string.h>

static uint16_t read_u16(const uint8_t *p)
{return (uint16_t)(((uint16_t)p[0]<<8)|(uint16_t)p[1]);}
static uint64_t read_u64(const uint8_t *p)
{uint64_t v=0;size_t i;for(i=0;i<8u;++i){v<<=8;v|=(uint64_t)p[i];}return v;}
static int hex_address(const char *s,const char *prefix,size_t n)
{size_t i;if(s==NULL||strlen(s)!=n||memcmp(s,prefix,strlen(prefix))!=0)return 0;for(i=strlen(prefix);i<n;++i)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return 0;return 1;}

int stnc_contract_list_decode(const uint8_t *payload,size_t length,stnc_contract_list *list)
{
    stnc_contract_list decoded;size_t count,i,offset;
    if(payload==NULL||list==NULL||length<2u)return 1;
    memset(&decoded,0,sizeof(decoded));count=(size_t)read_u16(payload);
    if(count>STNC_CONTRACT_LIST_MAX||length!=2u+count*STNC_CONTRACT_LIST_ENTRY_SIZE)return 1;
    for(i=0;i<count;++i){stnc_contract_list_entry *e=&decoded.entries[i];offset=2u+i*STNC_CONTRACT_LIST_ENTRY_SIZE;
        memcpy(e->address,payload+offset,STNC_CONTRACT_LIST_ADDRESS_SIZE);e->address[STNC_CONTRACT_LIST_ADDRESS_SIZE]='\0';
        if(!hex_address(e->address,"stnc0_",STNC_CONTRACT_LIST_ADDRESS_SIZE))return 1;
        e->state=read_u16(payload+offset+70u);e->type=read_u16(payload+offset+72u);
        e->sequence=read_u64(payload+offset+74u);e->created_at=read_u64(payload+offset+82u);
        if(e->state<1u||e->state>9u||e->type<1u||e->type>6u)return 1;
    }
    decoded.count=count;*list=decoded;return 0;
}
