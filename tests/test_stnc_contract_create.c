#define _CRT_SECURE_NO_WARNINGS
#include "stnc_contract_create.h"
#include "stnc_contract_action.h"
#include "stnc_core.h"
#include "stnc_identity.h"
#include <stdio.h>
#include <string.h>
static int submitted,identity_enabled;static uint16_t submit_result=STNC_STNC_SUBMISSION_ADMITTED;static char captured_actor[65];
int stnc_identity_status_read(stnc_identity_status *status)
{size_t i;memset(status,0,sizeof(*status));if(!identity_enabled)return 0;status->present=1;status->valid=1;for(i=0;i<32;++i)status->public_key[i]=(uint8_t)i;memcpy(status->address,"stn0_",5);memset(status->address+5,'a',64);status->address[69]=0;return 0;}
int stnc_contract_draft_build(const stnc_contract_draft_input *input,uint8_t *bytes,size_t capacity,size_t *written,char address[71])
{if(input&&input->participant_count)memcpy(captured_actor,input->participants[0].public_key,65);if(capacity<32)return 1;memset(bytes,0,32);memcpy(bytes,"STCT",4);*written=32;strcpy(address,"stnc0_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");return 0;}
int stnc_contract_action_build(const uint8_t *current,size_t length,uint16_t action,const uint8_t *authority,size_t authority_length,uint8_t *transaction,size_t capacity,size_t *written)
{if(!current||length!=32||action!=1||authority||authority_length||capacity<160)return 1;memset(transaction,0,160);memcpy(transaction,"STNT",4);*written=160;return 0;}
int stnc_core_submit_transaction(const uint8_t *transaction,size_t length,stnc_submission_result *result)
{if(!transaction||length!=160||memcmp(transaction,"STNT",4)!=0)return 1;submitted++;memset(result,0,sizeof(*result));result->result=submit_result;result->has_transaction_id=1;memset(result->transaction_id,0x55,32);return 0;}
void stnc_platform_secure_clear(void *p,size_t n){memset(p,0,n);}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Contract create test failed: %d\n",__LINE__);return 1;}}while(0)
int main(void){stnc_contract_draft_input in;stnc_contract_create_result out;size_t i;memset(&in,0,sizeof(in));CHECK(stnc_contract_create(&in,&out)==0);CHECK(submitted==1);CHECK(out.submission==STNC_STNC_SUBMISSION_ADMITTED);CHECK(out.has_transaction_id);CHECK(out.transaction_id[0]==0x55);CHECK(strncmp(out.address,"stnc0_",6)==0);CHECK(stnc_contract_create(NULL,&out)!=0);CHECK(stnc_contract_create(&in,NULL)!=0);
memset(&in,0,sizeof(in));in.participant_count=1;for(i=0;i<32;++i)sprintf(in.participants[0].public_key+2*i,"%02x",(unsigned)i);in.participants[0].role=1;identity_enabled=1;memset(captured_actor,0,sizeof(captured_actor));CHECK(stnc_contract_create(&in,&out)==0);CHECK(strcmp(captured_actor,"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")==0);
puts("STNC Contract create tests passed.");return 0;}
