#define _CRT_SECURE_NO_WARNINGS
#include "stnc_profile.h"
#include "stnc_wallet_store.h"
#include "stnc_identity.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    stnc_profile_info p,profiles[STNC_PROFILE_LIST_MAX];stnc_wallet_key wallet;stnc_identity_status identity;size_t count=0u;
    if(!stnc_profile_name_valid("Treasury")||!stnc_profile_name_valid("Personal Wallet")||stnc_profile_name_valid("")||stnc_profile_name_valid("bad/name")||stnc_profile_name_valid(NULL))return 1;
    if(stnc_profile_paths("Treasury",&p)!=0)return 2;
    if(strstr(p.wallet_path,"wallet_Treasury.key")==NULL||strstr(p.identity_path,"identity_Treasury.key")==NULL)return 3;
    if(stnc_profile_list(profiles,STNC_PROFILE_LIST_MAX,&count)!=0||count<1u||strcmp(profiles[0].name,"Default")!=0)return 4;
    if(strcmp(profiles[0].wallet_path,profiles[0].identity_path)==0)return 5;
    memset(&wallet,0,sizeof(wallet));memset(&identity,0,sizeof(identity));
    printf("profile naming, listing and separate key paths passed\n");return 0;
}
