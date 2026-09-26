#include "stnc_contract_status.h"
#include <string.h>
#include "stnc_core.h"
int stnc_contract_status_read(const char *address,stnc_contract_status *status)
{
    stnc_contract_status result;size_t i;
    if(address==NULL||status==NULL)return 1;
    memset(&result,0,sizeof(result));
    if(strlen(address)!=STNC_STNC_ADDRESS_TYPED_SIZE||memcmp(address,"stnc0_",6u)!=0)return 1;
    for(i=6;i<STNC_STNC_ADDRESS_TYPED_SIZE;++i)
        if(!((address[i]>='0'&&address[i]<='9')||(address[i]>='a'&&address[i]<='f')))return 1;
    memcpy(result.address,address,STNC_STNC_ADDRESS_TYPED_SIZE+1u);
    if(stnc_core_contract_state(address,&result.state)!=0){*status=result;return 0;}
    result.available=1;*status=result;return 0;
}
const char *stnc_contract_state_name(uint16_t state)
{
    switch(state){case 1u:return "draft";case 2u:return "issued";case 3u:return "review";case 4u:return "approvals";case 5u:return "attestation";case 6u:return "executed";case 7u:return "rejected";case 8u:return "revoked";case 9u:return "closed";default:return "unknown";}
}
const char *stnc_contract_type_name(uint16_t type)
{
    switch(type){case 1u:return "generic";case 2u:return "work-offer";case 3u:return "contributor-agreement";case 4u:return "policy";case 5u:return "organizational-decision";case 6u:return "service-agreement";default:return "unknown";}
}
