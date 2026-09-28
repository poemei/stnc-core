#include "stnc_contract_list.h"
#include "stnc_config.h"
#include "stnc_network.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static stnc_config config;
static int mode,closes;
const stnc_config *stnc_config_get(void){return &config;}
void stnc_log_warning(const char *s){(void)s;}
int stnc_network_connect(stnc_network_connection *c,const char *p,unsigned short port)
{(void)p;(void)port;c->connected=1;return 0;}
int stnc_network_send(stnc_network_connection *c,const unsigned char *p,size_t n)
{(void)c;assert(n==93&&p[9]==12);return 0;}
int stnc_network_receive(stnc_network_connection *c,unsigned char *p,size_t n)
{
    if(mode==1){c->connected=0;return 1;}
    memset(p,0,n);
    if(n==24){memcpy(p,"STNC",4);p[5]=2;p[7]=mode==2?1:2;p[9]=12;p[19]=19;p[23]=2;}
    return 0;
}
void stnc_network_disconnect(stnc_network_connection *c){c->connected=0;closes++;}
int main(void)
{
    stnc_contract_list list;
    const char *id="stn0_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    assert(stnc_contract_list_read(id,&list)==0&&list.count==0&&closes==1);
    mode=1;assert(stnc_contract_list_read(id,&list)!=0&&closes==2);
    mode=2;assert(stnc_contract_list_read(id,&list)!=0&&closes==3);
    assert(stnc_contract_list_read("bad",&list)!=0&&closes==3);
    puts("STNC Contract list transport tests passed.");return 0;
}
