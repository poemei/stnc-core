#include "stnc_wallet.h"
#include "stnc_platform.h"

#include <string.h>

int stnc_ed25519_publickey(const uint8_t private_key[32],uint8_t public_key[32]);
int stn_ed25519_sign(const uint8_t *,size_t,const uint8_t[32],const uint8_t[32],uint8_t[64]);

static int nibble(unsigned char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1;}
static int decode_wallet(const char *text,uint8_t id[32])
{
    size_t i;
    if(text==NULL||strlen(text)!=STNC_WALLET_ADDRESS_SIZE||memcmp(text,"stnw0_",6)!=0)return 1;
    for(i=0;i<32;++i){int h=nibble((unsigned char)text[6+2*i]),l=nibble((unsigned char)text[7+2*i]);if(h<0||l<0)return 1;id[i]=(uint8_t)((h<<4)|l);}
    return 0;
}
static void put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void put32(uint8_t *p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static void put64(uint8_t *p,uint64_t v){size_t i;for(i=0;i<8;++i)p[7-i]=(uint8_t)(v>>(i*8));}

int stnc_wallet_generate(stnc_wallet_key *key)
{
    stnc_wallet_key generated;
    if(key==NULL)return 1;
    memset(&generated,0,sizeof(generated));
    if(stnc_platform_random(generated.private_key,sizeof(generated.private_key))!=0 ||
       stnc_ed25519_publickey(generated.private_key,generated.public_key)!=0){stnc_wallet_clear(&generated);return 1;}
    *key=generated;return 0;
}
void stnc_wallet_clear(stnc_wallet_key *key){if(key!=NULL)stnc_platform_secure_clear(key,sizeof(*key));}

int stnc_wallet_address(const stnc_wallet_key *key,char address[STNC_WALLET_ADDRESS_SIZE+1u])
{
    static const char hex[]="0123456789abcdef";uint8_t digest[32];size_t i;
    if(key==NULL||address==NULL||stnc_platform_sha256(key->public_key,32,digest)!=0)return 1;
    memcpy(address,"stnw0_",6);for(i=0;i<32;++i){address[6+2*i]=hex[digest[i]>>4];address[7+2*i]=hex[digest[i]&15];}
    address[70]='\0';stnc_platform_secure_clear(digest,sizeof(digest));return 0;
}

int stnc_wallet_build_transfer(const stnc_wallet_key *key,const char *source,const char *destination,
    uint64_t units,const uint8_t nonce[STNC_WALLET_NONCE_SIZE],uint8_t transaction[STNC_WALLET_TRANSFER_SIZE])
{
    static const uint8_t domain[40]="STN-CHAIN:TRANSFER:ENVELOPE:AUTHORIZE:1";
    uint8_t source_id[32],destination_id[32],expected[32],statement[STNC_WALLET_TRANSFER_STATEMENT_SIZE];
    uint8_t envelope[202],signature[64];size_t i;int nonzero=0;
    if(key==NULL||source==NULL||destination==NULL||nonce==NULL||transaction==NULL||units==0 ||
       decode_wallet(source,source_id)!=0||decode_wallet(destination,destination_id)!=0||
       memcmp(source_id,destination_id,32)==0||stnc_platform_sha256(key->public_key,32,expected)!=0||
       memcmp(source_id,expected,32)!=0)return 1;
    for(i=0;i<32;++i)nonzero|=nonce[i];if(!nonzero)return 1;
    memset(envelope,0,sizeof(envelope));envelope[0]=1;memcpy(envelope+1,key->public_key,32);memcpy(envelope+33,nonce,32);
    envelope[65]=1;memcpy(envelope+66,source_id,32);memcpy(envelope+98,destination_id,32);put64(envelope+130,units);
    memcpy(statement,domain,sizeof(domain));statement[40]=1;memcpy(statement+41,key->public_key,32);memcpy(statement+73,nonce,32);memcpy(statement+105,envelope+65,73);
    if(stn_ed25519_sign(statement,sizeof(statement),key->public_key,key->private_key,signature)!=0)return 1;
    memcpy(envelope+138,signature,64);
    memcpy(transaction,"STNT",4);put16(transaction+4,1);put16(transaction+6,9);put32(transaction+8,202);memcpy(transaction+12,envelope,202);
    stnc_platform_secure_clear(source_id,sizeof(source_id));stnc_platform_secure_clear(destination_id,sizeof(destination_id));
    stnc_platform_secure_clear(expected,sizeof(expected));stnc_platform_secure_clear(statement,sizeof(statement));stnc_platform_secure_clear(signature,sizeof(signature));stnc_platform_secure_clear(envelope,sizeof(envelope));
    return 0;
}
