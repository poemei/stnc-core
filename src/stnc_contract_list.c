#include "stnc_contract_list.h"
#include "stnc_config.h"
#include "stnc_network.h"
#include "stnc_stnc.h"
#include <stdlib.h>
#include <string.h>

#define STNC_CONTRACT_LIST_METHOD 12u
#define STNC_CONTRACT_LIST_REQUEST_ID UINT64_C(19)
#define STNC_CONTRACT_LIST_IDENTITY_SIZE 69u
#define STNC_CONTRACT_LIST_PAYLOAD_MAX (2u + STNC_CONTRACT_LIST_MAX * STNC_CONTRACT_LIST_ENTRY_SIZE)

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

int stnc_contract_list_read(const char *identity,stnc_contract_list *list)
{
    const stnc_config *config;stnc_network_connection connection;stnc_stnc_message request,response;
    uint8_t request_bytes[STNC_STNC_HEADER_SIZE+STNC_CONTRACT_LIST_IDENTITY_SIZE];
    uint8_t header[STNC_STNC_HEADER_SIZE],*payload=NULL;size_t written;int rc=1;
    if(list==NULL)return 1;memset(list,0,sizeof(*list));
    if(!hex_address(identity,"stn0_",STNC_CONTRACT_LIST_IDENTITY_SIZE))return 1;
    config=stnc_config_get();if(config==NULL)return 1;memset(&connection,0,sizeof(connection));
    memset(&request,0,sizeof(request));request.kind=STNC_STNC_REQUEST;request.method=STNC_CONTRACT_LIST_METHOD;
    request.code=STNC_STNC_OK;request.request_id=STNC_CONTRACT_LIST_REQUEST_ID;
    request.payload=(const uint8_t *)identity;request.length=STNC_CONTRACT_LIST_IDENTITY_SIZE;
    if(stnc_stnc_encode(&request,request_bytes,sizeof(request_bytes),&written)!=0||
       stnc_network_connect(&connection,config->peer,config->port)!=0||
       stnc_network_send(&connection,request_bytes,written)!=0||
       stnc_network_receive(&connection,header,sizeof(header))!=0||
       stnc_stnc_decode_header(header,sizeof(header),&response)!=0||
       response.method!=STNC_CONTRACT_LIST_METHOD||response.request_id!=STNC_CONTRACT_LIST_REQUEST_ID||
       response.code!=STNC_STNC_OK||response.length<2u||response.length>STNC_CONTRACT_LIST_PAYLOAD_MAX)goto done;
    payload=(uint8_t *)malloc(response.length);if(payload==NULL)goto done;
    if(stnc_network_receive(&connection,payload,response.length)!=0||
       stnc_contract_list_decode(payload,response.length,list)!=0)goto done;
    rc=0;
done:
    free(payload);if(stnc_network_is_connected(&connection))stnc_network_disconnect(&connection);return rc;
}
