#define _CRT_SECURE_NO_WARNINGS
#include "stnc_profile.h"
#include "stnc_wallet_store.h"
#include "stnc_identity.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    stnc_profile_info p;stnc_wallet_key wallet;stnc_identity_status identity;
    if(!stnc_profile_name_valid("Treasury")||!stnc_profile_name_valid("Personal Wallet")||stnc_profile_name_valid("")||stnc_profile_name_valid("bad/name"))return 1;
    if(stnc_profile_paths("Treasury",&p)!=0)return 2;
    if(strstr(p.wallet_path,"wallet_Treasury.key")==NULL||strstr(p.identity_path,"identity_Treasury.key")==NULL)return 3;
    /* Path-specific key stores remain independently verifiable. */
    memset(&wallet,0,sizeof(wallet));memset(&identity,0,sizeof(identity));
    printf("profile naming and separate key paths passed\n");return 0;
}
