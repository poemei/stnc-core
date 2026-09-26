#include "stnc_contract_create.h"
#include "stnc_contract_action.h"
#include "stnc_core.h"
#include "stnc_platform.h"
#include <stdlib.h>
#include <string.h>
int stnc_contract_create(const stnc_contract_draft_input *input,stnc_contract_create_result *result)
{
    stnc_contract_create_result out;stnc_submission_result submission;
    uint8_t *contract=NULL,*transaction=NULL;size_t contract_length=0,transaction_length=0;int rc=1;
    if(input==NULL||result==NULL)return 1;
    memset(&out,0,sizeof(out));memset(&submission,0,sizeof(submission));
    contract=(uint8_t*)malloc(STNC_CONTRACT_DRAFT_MAX);
    transaction=(uint8_t*)malloc(STNC_CONTRACT_ACTION_MAX);
    if(contract==NULL||transaction==NULL)goto done;
    if(stnc_contract_draft_build(input,contract,STNC_CONTRACT_DRAFT_MAX,&contract_length,out.address)!=0)goto done;
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
