#include <stdio.h>
#include <string.h>
#include "stnc_contract_status.h"
static int query_result;
int stnc_core_contract_state(const char *contract,stnc_contract_state *state)
{
    if(contract==NULL||state==NULL||query_result!=0)return 1;
    memset(state,0,sizeof(*state));state->state=4u;state->type=6u;state->sequence=7u;state->created_at=9u;state->participant_count=3u;state->terms_length=12u;return 0;
}
int main(void)
{
    const char *a="stnc0_0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";stnc_contract_status s;
    if(stnc_contract_status_read(NULL,&s)==0||stnc_contract_status_read(a,NULL)==0)return 1;
    if(stnc_contract_status_read("stnw0_0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",&s)==0)return 1;
    query_result=0;if(stnc_contract_status_read(a,&s)!=0||!s.available||strcmp(s.address,a)!=0||s.state.state!=4u||s.state.type!=6u)return 1;
    query_result=1;if(stnc_contract_status_read(a,&s)!=0||s.available||strcmp(s.address,a)!=0)return 1;
    if(strcmp(stnc_contract_state_name(4u),"approvals")!=0||strcmp(stnc_contract_state_name(99u),"unknown")!=0)return 1;
    if(strcmp(stnc_contract_type_name(6u),"service-agreement")!=0||strcmp(stnc_contract_type_name(99u),"unknown")!=0)return 1;
    puts("Contract status tests passed.");return 0;
}
