#include "stnc_contract_list.h"
#include <stdio.h>
#include <string.h>

static void put16(unsigned char *p,unsigned int v){p[0]=(unsigned char)(v>>8);p[1]=(unsigned char)v;}
static void put64(unsigned char *p,unsigned long long v){int i;for(i=7;i>=0;--i){p[i]=(unsigned char)(v&255u);v>>=8;}}

int main(void)
{
    unsigned char payload[2u+2u*STNC_CONTRACT_LIST_ENTRY_SIZE];stnc_contract_list list;size_t i;
    memset(payload,0,sizeof(payload));put16(payload,2u);
    for(i=0;i<2u;++i){size_t o=2u+i*STNC_CONTRACT_LIST_ENTRY_SIZE,j;memcpy(payload+o,"stnc0_",6u);for(j=6u;j<70u;++j)payload[o+j]=(unsigned char)"0123456789abcdef"[(j+i)&15u];put16(payload+o+70u,(unsigned int)(2u+i));put16(payload+o+72u,(unsigned int)(4u+i));put64(payload+o+74u,7u+i);put64(payload+o+82u,9u+i);}
    if(stnc_contract_list_decode(payload,sizeof(payload),&list)!=0||list.count!=2u||list.entries[0].state!=2u||list.entries[1].type!=5u||list.entries[1].sequence!=8u){puts("STNC Contract list tests failed: valid payload");return 1;}
    payload[2]='x';if(stnc_contract_list_decode(payload,sizeof(payload),&list)==0){puts("STNC Contract list tests failed: malformed address accepted");return 1;}payload[2]='s';
    put16(payload,17u);if(stnc_contract_list_decode(payload,sizeof(payload),&list)==0){puts("STNC Contract list tests failed: oversized count accepted");return 1;}
    put16(payload,2u);if(stnc_contract_list_decode(payload,sizeof(payload)-1u,&list)==0){puts("STNC Contract list tests failed: bad length accepted");return 1;}
    puts("STNC Contract list tests passed.");return 0;
}
