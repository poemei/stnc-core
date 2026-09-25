#include "stnc_wallet_status.h"
#include <string.h>
#include "stnc_core.h"
#include "stnc_wallet_store.h"
int stnc_wallet_status_read(stnc_wallet_status *status)
{
    stnc_wallet_status result;stnc_wallet_key key;
    if(status==NULL)return 1;
    memset(&result,0,sizeof(result));memset(&key,0,sizeof(key));
    if(!stnc_wallet_store_exists()){*status=result;return 0;}
    result.present=1;
    if(stnc_wallet_store_load(&key)!=0||stnc_wallet_address(&key,result.address)!=0){
        stnc_wallet_clear(&key);*status=result;return 0;
    }
    result.key_valid=1;
    if(stnc_core_balance(result.address,&result.accepted_balance)==0)result.balance_available=1;
    stnc_wallet_clear(&key);*status=result;return 0;
}
