#include "stnc_transfer.h"

#include <string.h>

#include "stnc_core.h"
#include "stnc_platform.h"
#include "stnc_wallet_store.h"

const char *stnc_transfer_submission_name(uint16_t submission)
{
    switch(submission){
    case STNC_STNC_SUBMISSION_ADMITTED:return "admitted";
    case STNC_STNC_SUBMISSION_DUPLICATE:return "duplicate";
    case STNC_STNC_SUBMISSION_POOL_FULL:return "pool-full";
    case STNC_STNC_SUBMISSION_BAD:return "bad-submission";
    case STNC_STNC_SUBMISSION_UNSUPPORTED:return "unsupported";
    case STNC_STNC_SUBMISSION_REPLAY:return "replay";
    case STNC_STNC_SUBMISSION_UNAUTHORIZED:return "unauthorized";
    case STNC_STNC_SUBMISSION_UNAVAILABLE:return "unavailable";
    case STNC_STNC_SUBMISSION_INTERNAL:return "internal";
    default:return "unknown";
    }
}

int stnc_transfer_send(const char *destination,uint64_t units,stnc_transfer_result *result)
{
    stnc_transfer_result out;
    stnc_wallet_key key;
    stnc_submission_result submission;
    uint8_t nonce[STNC_WALLET_NONCE_SIZE];
    uint8_t transaction[STNC_WALLET_TRANSFER_SIZE];
    int rc=1;

    if(destination==NULL||result==NULL||units==0u)return 1;
    memset(&out,0,sizeof(out));memset(&key,0,sizeof(key));
    memset(&submission,0,sizeof(submission));memset(nonce,0,sizeof(nonce));
    memset(transaction,0,sizeof(transaction));

    if(stnc_wallet_store_load(&key)!=0)return 1;
    if(stnc_wallet_address(&key,out.source)!=0)goto cleanup;
    if(stnc_platform_random(nonce,sizeof(nonce))!=0)goto cleanup;
    if(stnc_wallet_build_transfer(&key,out.source,destination,units,nonce,transaction)!=0)goto cleanup;
    if(stnc_core_submit_transaction(transaction,sizeof(transaction),&submission)!=0)goto cleanup;

    out.submission=submission.result;
    out.has_transaction_id=submission.has_transaction_id;
    if(submission.has_transaction_id)memcpy(out.transaction_id,submission.transaction_id,sizeof(out.transaction_id));
    if((submission.result==STNC_STNC_SUBMISSION_ADMITTED||submission.result==STNC_STNC_SUBMISSION_DUPLICATE)&&
       stnc_core_balance(out.source,&out.accepted_balance)==0)out.balance_available=1;
    *result=out;rc=0;

cleanup:
    stnc_wallet_clear(&key);
    stnc_platform_secure_clear(nonce,sizeof(nonce));
    stnc_platform_secure_clear(transaction,sizeof(transaction));
    return rc;
}
