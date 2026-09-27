#include "stnc_contract_create.h"
#include "stnc_contract_action.h"
#include "stnc_core.h"
#include "stnc_identity.h"
#include "stnc_platform.h"
#include <stdlib.h>
#include <string.h>

#define STNC_CONTRACT_ROLE_PARTICIPANT 1u
#define STNC_CONTRACT_ROLE_ISSUER 2u
#define STNC_CONTRACT_ROLE_APPROVER 4u

static void identity_public_key_hex(const uint8_t public_key[32],char hex[65])
{
    static const char digits[]="0123456789abcdef";
    size_t i;
    for(i=0;i<32;++i){hex[2*i]=digits[public_key[i]>>4];hex[2*i+1]=digits[public_key[i]&15];}
    hex[64]=0;
}

static void normalize_local_participants(stnc_contract_draft_input *draft)
{
    stnc_identity_status identity;
    char public_key_hex[65];
    size_t i;

    if(draft==NULL||draft->participant_count==0)return;

    memset(&identity,0,sizeof(identity));
    memset(public_key_hex,0,sizeof(public_key_hex));
    if(stnc_identity_status_read(&identity)!=0||!identity.valid||
       strlen(identity.address)!=69||memcmp(identity.address,"stn0_",5)!=0)return;

    identity_public_key_hex(identity.public_key,public_key_hex);

    for(i=0;i<draft->participant_count;++i){
        if(strcmp(draft->participants[i].public_key,public_key_hex)==0)
            memcpy(draft->participants[i].public_key,identity.address+5,65);
    }

    /*
     * The current GUI's legacy one-participant draft represented the local
     * creator as a generic PARTICIPANT. Phase 18 accepted Contract state
     * requires at least one eligible APPROVER so the majority threshold is
     * defined. Convert only that exact legacy shape into the smallest complete
     * local agreement: the local identity is both ISSUER and APPROVER.
     *
     * Explicit multi-participant drafts are never rewritten here; their roles
     * remain caller-owned and are validated by Chain consensus.
     */
    if(draft->participant_count==1u &&
       draft->participants[0].role==STNC_CONTRACT_ROLE_PARTICIPANT &&
       strcmp(draft->participants[0].public_key,identity.address+5)==0){
        draft->participants[0].role=STNC_CONTRACT_ROLE_ISSUER;
        draft->participants[1]=draft->participants[0];
        draft->participants[1].role=STNC_CONTRACT_ROLE_APPROVER;
        draft->participant_count=2u;
    }
}

int stnc_contract_create(const stnc_contract_draft_input *input,stnc_contract_create_result *result)
{
    stnc_contract_create_result out;stnc_submission_result submission;stnc_contract_draft_input draft;
    uint8_t *contract=NULL,*transaction=NULL;size_t contract_length=0,transaction_length=0;int rc=1;
    if(input==NULL||result==NULL)return 1;
    memset(&out,0,sizeof(out));memset(&submission,0,sizeof(submission));draft=*input;
    normalize_local_participants(&draft);
    contract=(uint8_t*)malloc(STNC_CONTRACT_DRAFT_MAX);
    transaction=(uint8_t*)malloc(STNC_CONTRACT_ACTION_MAX);
    if(contract==NULL||transaction==NULL)goto done;
    if(stnc_contract_draft_build(&draft,contract,STNC_CONTRACT_DRAFT_MAX,&contract_length,out.address)!=0)goto done;
    if(stnc_contract_action_build(contract,contract_length,STNC_CONTRACT_ACTION_CREATE,NULL,0u,
        transaction,STNC_CONTRACT_ACTION_MAX,&transaction_length)!=0)goto done;
    if(stnc_core_submit_transaction(transaction,transaction_length,&submission)!=0)goto done;
    out.submission=submission.result;out.has_transaction_id=submission.has_transaction_id;
    if(submission.has_transaction_id)memcpy(out.transaction_id,submission.transaction_id,sizeof(out.transaction_id));
    *result=out;rc=0;
done:
    if(transaction!=NULL)stnc_platform_secure_clear(transaction,STNC_CONTRACT_ACTION_MAX);
    if(contract!=NULL)stnc_platform_secure_clear(contract,STNC_CONTRACT_DRAFT_MAX);
    free(transaction);free(contract);return rc;
}
