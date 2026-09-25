#include <stdio.h>
#include <string.h>
#include "stnc_wallet.h"

int stn_ed25519_verify(const unsigned char *,size_t,const unsigned char[32],const unsigned char[64]);

static int hexid(const char *text,unsigned char out[32])
{
    size_t i;for(i=0;i<32u;i++){unsigned char a=(unsigned char)text[6u+2u*i],b=(unsigned char)text[7u+2u*i];int h,l;
        h=a>='0'&&a<='9'?a-'0':a-'a'+10;l=b>='0'&&b<='9'?b-'0':b-'a'+10;out[i]=(unsigned char)((h<<4)|l);}return 0;
}
int main(void)
{
    stnc_wallet_key key;unsigned char nonce[32],tx[STNC_WALLET_TRANSFER_SIZE],source_id[32],statement[STNC_WALLET_TRANSFER_STATEMENT_SIZE];
    char source[STNC_WALLET_ADDRESS_SIZE+1u],destination[STNC_WALLET_ADDRESS_SIZE+1u];size_t i;
    memset(&key,0,sizeof(key));for(i=0;i<32u;i++)key.private_key[i]=(unsigned char)(i+1u);
    if(stnc_ed25519_publickey(key.private_key,key.public_key)!=0||stnc_wallet_address(&key,source)!=0)return 1;
    memcpy(destination,"stnw0_0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",71u);
    memset(nonce,0x5a,sizeof(nonce));
    if(stnc_wallet_build_transfer(&key,source,destination,UINT64_C(7),nonce,tx)!=0)return 1;
    if(memcmp(tx,"STNT",4)!=0||tx[5]!=1u||tx[7]!=9u||tx[11]!=202u||tx[12]!=1u)return 1;
    hexid(source,source_id);
    if(memcmp(tx+78u,source_id,32u)!=0)return 1;
    memcpy(statement,"STN-CHAIN:TRANSFER:ENVELOPE:AUTHORIZE:1",39u);statement[39]=0u;statement[40]=1u;
    memcpy(statement+41,key.public_key,32u);memcpy(statement+73,nonce,32u);memcpy(statement+105,tx+77u,73u);
    if(stn_ed25519_verify(statement,sizeof(statement),key.public_key,tx+150u)!=0)return 1;
    nonce[0]=0;memset(nonce,0,sizeof(nonce));if(stnc_wallet_build_transfer(&key,source,destination,7u,nonce,tx)==0)return 1;
    stnc_wallet_clear(&key);puts("Wallet transfer tests passed.");return 0;
}
