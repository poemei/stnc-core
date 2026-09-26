#include <stdio.h>
#include <string.h>

#include "stnc_transfer.h"
#include "stnc_wallet_store.h"
#include "stnc_core.h"
#include "stnc_platform.h"

static stnc_wallet_key stored;
static int wallet_available=1;
static int submit_error=0;
static uint16_t submit_result=STNC_STNC_SUBMISSION_ADMITTED;
static int balance_error=0;
static uint64_t balance_value=125u;
static unsigned submit_calls=0u;

int stnc_wallet_store_load(stnc_wallet_key *key){if(!wallet_available||key==NULL)return 1;*key=stored;return 0;}
int stnc_wallet_address(const stnc_wallet_key *key,char address[STNC_WALLET_ADDRESS_SIZE+1u])
{(void)key;memcpy(address,"stnw0_1111111111111111111111111111111111111111111111111111111111111111",71u);return 0;}
int stnc_platform_random(uint8_t *buffer,size_t length){size_t i;if(buffer==NULL||length!=32u)return 1;for(i=0;i<length;i++)buffer[i]=(uint8_t)(i+1u);return 0;}
void stnc_platform_secure_clear(void *buffer,size_t length){if(buffer!=NULL)memset(buffer,0,length);}
int stnc_wallet_build_transfer(const stnc_wallet_key *key,const char *source,const char *destination,uint64_t units,const uint8_t nonce[32],uint8_t transaction[214])
{(void)key;if(source==NULL||destination==NULL||units==0u||nonce==NULL||transaction==NULL)return 1;if(strncmp(destination,"stnw0_",6u)!=0||strlen(destination)!=70u)return 1;memset(transaction,0x5a,214u);return 0;}
int stnc_core_submit_transaction(const uint8_t *transaction,size_t length,stnc_submission_result *result)
{size_t i;if(submit_error||transaction==NULL||length!=214u||result==NULL)return 1;submit_calls++;memset(result,0,sizeof(*result));result->result=submit_result;result->has_transaction_id=1;for(i=0;i<32u;i++)result->transaction_id[i]=(uint8_t)i;return 0;}
int stnc_core_balance(const char *wallet,uint64_t *units){(void)wallet;if(balance_error||units==NULL)return 1;*units=balance_value;return 0;}

static int fail(int line){fprintf(stderr,"STNC transfer test failed at line %d.\n",line);return 1;}
#define CHECK(x) do{if(!(x))return fail(__LINE__);}while(0)

int main(void)
{
    stnc_transfer_result result;
    const char *destination="stnw0_2222222222222222222222222222222222222222222222222222222222222222";
    memset(&stored,0,sizeof(stored));memset(&result,0,sizeof(result));
    CHECK(stnc_transfer_send(destination,25u,&result)==0);
    CHECK(result.submission==STNC_STNC_SUBMISSION_ADMITTED&&result.has_transaction_id);
    CHECK(result.balance_available&&result.accepted_balance==125u&&submit_calls==1u);
    CHECK(strcmp(stnc_transfer_submission_name(STNC_STNC_SUBMISSION_ADMITTED),"admitted")==0);
    submit_result=STNC_STNC_SUBMISSION_DUPLICATE;balance_value=100u;
    CHECK(stnc_transfer_send(destination,25u,&result)==0&&result.submission==STNC_STNC_SUBMISSION_DUPLICATE&&result.accepted_balance==100u);
    submit_result=STNC_STNC_SUBMISSION_REPLAY;balance_error=1;
    CHECK(stnc_transfer_send(destination,25u,&result)==0&&!result.balance_available);
    CHECK(stnc_transfer_send(destination,0u,&result)!=0);
    CHECK(stnc_transfer_send("bad",1u,&result)!=0);
    wallet_available=0;CHECK(stnc_transfer_send(destination,1u,&result)!=0);
    wallet_available=1;submit_error=1;CHECK(stnc_transfer_send(destination,1u,&result)!=0);
    puts("STNC transfer tests passed.");return 0;
}
