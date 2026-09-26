#include "stnc_compensation.h"
#include <stdio.h>
#include <string.h>

static int fail(int line){fprintf(stderr,"STNC compensation test failed: %d\n",line);return 1;}
#define CHECK(x) do{if(!(x))return fail(__LINE__);}while(0)

int main(void)
{
    char identity[70],wallet[71];uint8_t tx[STNC_COMPENSATION_TRANSACTION_SIZE];size_t i;
    memcpy(identity,"stn0_",5u);for(i=5u;i<69u;i++)identity[i]='a';identity[69]='\0';
    memcpy(wallet,"stnw0_",6u);for(i=6u;i<70u;i++)wallet[i]='b';wallet[70]='\0';
    CHECK(stnc_compensation_build(identity,wallet,tx)==0);
    CHECK(memcmp(tx,"STNT",4u)==0&&tx[4]==0&&tx[5]==1&&tx[6]==0&&tx[7]==7);
    CHECK(tx[8]==0&&tx[9]==0&&tx[10]==0&&tx[11]==65&&tx[12]==1);
    for(i=13u;i<45u;i++)CHECK(tx[i]==0xaau);
    for(i=45u;i<77u;i++)CHECK(tx[i]==0xbbu);
    identity[0]='x';CHECK(stnc_compensation_build(identity,wallet,tx)!=0);
    puts("STNC compensation mapping tests passed.");return 0;
}
