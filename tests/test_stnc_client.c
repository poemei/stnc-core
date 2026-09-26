#define _CRT_SECURE_NO_WARNINGS
#include "stnc_client.h"
#include "stnc_transfer.h"
#include <stdio.h>
#include <string.h>
static int wallet,enabled,submitted,queried;
static uint16_t submission=STNC_STNC_SUBMISSION_ADMITTED;
static stnc_core_chain_state chain;
static stnc_core_peer_status peer;
static stnc_config config;
int stnc_wallet_store_load(stnc_wallet_key *key){memset(key,0,sizeof(*key));return wallet?0:1;}
void stnc_wallet_clear(stnc_wallet_key *key){memset(key,0,sizeof(*key));}
int stnc_wallet_store_create(stnc_wallet_key *key){(void)key;return 1;}
int stnc_wallet_address(const stnc_wallet_key *key,char *address){(void)key;(void)address;return 1;}
int stnc_wallet_status_read(stnc_wallet_status *status){memset(status,0,sizeof(*status));status->present=wallet;status->key_valid=wallet;return 0;}
int stnc_identity_status_read(stnc_identity_status *status){memset(status,0,sizeof(*status));return 0;}
int stnc_identity_create(stnc_identity_status *status){(void)status;return 1;}
void stnc_core_get_runtime_status(stnc_core_runtime_status *status){memset(status,0,sizeof(*status));}
const stnc_core_chain_state *stnc_core_get_chain_state(void){return &chain;}
const stnc_core_peer_status *stnc_core_get_peer_status(void){return &peer;}
void stnc_background_mining_status(stnc_mining_service_status *status){memset(status,0,sizeof(*status));}
int stnc_background_mining_connected(void){return 0;}
const stnc_config *stnc_config_get(void){return &config;}
int stnc_config_set_mining_enabled(int value){enabled=value;return 0;}
int stnc_config_set_mining_cpu_limit(unsigned int value){return value<1||value>2;}
size_t stnc_log_recent_count(void){return 5;}
int stnc_log_recent_get(size_t index,char *entry,size_t capacity){snprintf(entry,capacity,"event %zu",index);return 0;}
void stnc_log_info(const char *text){(void)text;}
void stnc_log_warning(const char *text){(void)text;}
int stnc_transfer_send(const char *destination,uint64_t units,stnc_transfer_result *result)
{(void)destination;(void)units;if(!wallet)return 1;submitted++;memset(result,0,sizeof(*result));result->submission=submission;return 0;}
const char *stnc_transfer_submission_name(uint16_t value){return value==0?"admitted":"unauthorized";}
int stnc_contract_status_read(const char *address,stnc_contract_status *status)
{(void)address;queried++;memset(status,0,sizeof(*status));status->available=queried>1;status->state.sequence=7;return 0;}
const char *stnc_contract_type_name(uint16_t value){(void)value;return "generic";}
const char *stnc_contract_state_name(uint16_t value){(void)value;return "review";}
int stnc_contract_draft_save(const stnc_contract_draft_input *input,const char *path,char address[71])
{(void)input;(void)path;(void)address;return 1;}
int stnc_contract_create(const stnc_contract_draft_input *input,stnc_contract_create_result *result)
{(void)input;if(result==NULL)return 1;memset(result,0,sizeof(*result));strcpy(result->address,"stnc0_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");result->submission=STNC_STNC_SUBMISSION_ADMITTED;result->has_transaction_id=1;memset(result->transaction_id,0x55,sizeof(result->transaction_id));return 0;}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Client boundary test failed: %d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    static stnc_client_request request;stnc_client_snapshot snapshot;char result[2048];uint64_t units=0;
    CHECK(stnc_client_parse_units("18446744073709551615",&units)==0&&units==UINT64_MAX);
    CHECK(stnc_client_parse_units("18446744073709551616",&units)!=0);
    CHECK(stnc_client_parse_units("0",&units)!=0&&stnc_client_parse_units("-1",&units)!=0);
    CHECK(stnc_client_parse_units("1.0",&units)!=0&&stnc_client_parse_units(" 1",&units)!=0);
    CHECK(stnc_client_set_mining(1)!=0&&!enabled);wallet=1;
    CHECK(stnc_client_set_mining(1)==0&&enabled);CHECK(stnc_client_set_mining(0)==0&&!enabled);
    CHECK(stnc_client_set_mining(2)!=0);
    request.operation=STNC_CLIENT_SEND;strcpy(request.address,"stnw0_2222222222222222222222222222222222222222222222222222222222222222");strcpy(request.units,"7");
    CHECK(stnc_client_execute(&request,result,sizeof(result))!=0&&submitted==0);
    request.confirmed=1;CHECK(stnc_client_execute(&request,result,sizeof(result))==0&&submitted==1);
    CHECK(strstr(result,"not supplied")&&strstr(result,"Accepted balance: unavailable"));
    submission=STNC_STNC_SUBMISSION_UNAUTHORIZED;CHECK(stnc_client_execute(&request,result,sizeof(result))!=0&&strstr(result,"Rejected"));
    request.address[6]='A';CHECK(stnc_client_execute(&request,result,sizeof(result))!=0&&submitted==2);
    CHECK(stnc_client_read(&snapshot)==0&&snapshot.activity_count==5&&!snapshot.wallet.balance_available);
    request.operation=STNC_CLIENT_CONTRACT_LOOKUP;
    CHECK(stnc_client_execute(&request,result,sizeof(result))!=0);
    CHECK(stnc_client_execute(&request,result,sizeof(result))==0&&strstr(result,"Sequence: 7"));
    memset(&request,0,sizeof(request));request.operation=STNC_CLIENT_CREATE_CONTRACT;
    CHECK(stnc_client_execute(&request,result,sizeof(result))==0);
    CHECK(strstr(result,"stnc0_")&&strstr(result,"Pending Chain acceptance."));
    puts("STNC client boundary tests passed.");return 0;
}
