#include <stdio.h>
#include <string.h>
#include "stnc_wallet_status.h"
static int exists_value,load_result,address_result,balance_result;static uint64_t balance_value;
int stnc_wallet_store_exists(void){return exists_value;}
int stnc_wallet_store_load(stnc_wallet_key *key){if(key==NULL||load_result!=0)return 1;memset(key,0,sizeof(*key));key->public_key[0]=1u;return 0;}
int stnc_wallet_address(const stnc_wallet_key *key,char address[STNC_WALLET_ADDRESS_SIZE+1u]){(void)key;if(address==NULL||address_result!=0)return 1;memcpy(address,"stnw0_0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",STNC_WALLET_ADDRESS_SIZE+1u);return 0;}
void stnc_wallet_clear(stnc_wallet_key *key){if(key!=NULL)memset(key,0,sizeof(*key));}
int stnc_core_balance(const char *wallet,uint64_t *units){if(wallet==NULL||units==NULL||balance_result!=0)return 1;*units=balance_value;return 0;}
int main(void)
{
    stnc_wallet_status s;
    if(stnc_wallet_status_read(NULL)==0)return 1;
    memset(&s,0xa5,sizeof(s));exists_value=0;if(stnc_wallet_status_read(&s)!=0||s.present||s.key_valid||s.balance_available||s.address[0]!='\0'||s.accepted_balance)return 1;
    exists_value=1;load_result=0;address_result=0;balance_result=0;balance_value=42;if(stnc_wallet_status_read(&s)!=0||!s.present||!s.key_valid||!s.balance_available||s.accepted_balance!=42)return 1;
    load_result=1;if(stnc_wallet_status_read(&s)!=0||!s.present||s.key_valid||s.balance_available||s.address[0]!='\0')return 1;
    load_result=0;balance_result=1;if(stnc_wallet_status_read(&s)!=0||!s.present||!s.key_valid||s.balance_available||s.address[0]=='\0'||s.accepted_balance)return 1;
    puts("Wallet status tests passed.");return 0;
}
