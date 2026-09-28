#include "stnc_contract_list.h"
#include "stnc_config.h"
#include "stnc_network.h"
#include "stnc_stnc.h"
#include "stnc_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STNC_CONTRACT_LIST_METHOD 12u
#define STNC_CONTRACT_LIST_REQUEST_ID UINT64_C(19)
#define STNC_CONTRACT_LIST_IDENTITY_SIZE 69u
#define STNC_CONTRACT_LIST_PAYLOAD_MAX (2u + STNC_CONTRACT_LIST_MAX * STNC_CONTRACT_LIST_ENTRY_SIZE)

static int hex_identity(const char *s)
{
    size_t i;
    if(s==NULL||strlen(s)!=STNC_CONTRACT_LIST_IDENTITY_SIZE||memcmp(s,"stn0_",5u)!=0)return 0;
    for(i=5u;i<STNC_CONTRACT_LIST_IDENTITY_SIZE;++i)
        if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return 0;
    return 1;
}

int stnc_contract_list_read(const char *identity,stnc_contract_list *list)
{
    const stnc_config *config;stnc_network_connection connection;stnc_stnc_message request,response;
    uint8_t request_bytes[STNC_STNC_HEADER_SIZE+STNC_CONTRACT_LIST_IDENTITY_SIZE];
    uint8_t header[STNC_STNC_HEADER_SIZE],*payload=NULL;size_t written;int rc=1,opened=0;
    if(list==NULL)return 1;memset(list,0,sizeof(*list));
    if(!hex_identity(identity))return 1;
    config=stnc_config_get();if(config==NULL)return 1;memset(&connection,0,sizeof(connection));
    memset(&request,0,sizeof(request));request.kind=STNC_STNC_REQUEST;request.method=STNC_CONTRACT_LIST_METHOD;
    request.code=STNC_STNC_OK;request.request_id=STNC_CONTRACT_LIST_REQUEST_ID;
    request.payload=(const uint8_t *)identity;request.length=STNC_CONTRACT_LIST_IDENTITY_SIZE;
    if(stnc_stnc_encode(&request,request_bytes,sizeof(request_bytes),&written)!=0||
       stnc_network_connect(&connection,config->peer,config->port)!=0)goto done;
    opened=1;
    if(stnc_network_send(&connection,request_bytes,written)!=0||
       stnc_network_receive(&connection,header,sizeof(header))!=0||
       stnc_stnc_decode_header(header,sizeof(header),&response)!=0||
       response.kind!=STNC_STNC_RESPONSE||
       response.method!=STNC_CONTRACT_LIST_METHOD||response.request_id!=STNC_CONTRACT_LIST_REQUEST_ID||
       response.code!=STNC_STNC_OK||response.length<2u||response.length>STNC_CONTRACT_LIST_PAYLOAD_MAX)goto done;
    payload=(uint8_t *)malloc(response.length);if(payload==NULL)goto done;
    if(stnc_network_receive(&connection,payload,response.length)!=0||
       stnc_contract_list_decode(payload,response.length,list)!=0)goto done;
    rc=0;
done:
    if(rc)stnc_log_warning("Contract list request failed; no accepted list was received.");
    free(payload);if(opened)stnc_network_disconnect(&connection);return rc;
}
