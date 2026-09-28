#include "stnc_network.h"
#include "stnc_platform.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned timeout_ms,timeout_calls,closed,locks;
static int receive_fail,timeout_fail;
static unsigned char reply[24];
int stnc_platform_network_init(void){return 0;}
void stnc_platform_network_shutdown(void){}
int stnc_platform_network_connect(void **h,const char *p,unsigned short port)
{(void)p;(void)port;*h=(void *)1;return 0;}
void stnc_platform_network_disconnect(void *h){(void)h;closed++;}
int stnc_platform_network_timeout(void *h,unsigned ms)
{assert(h);timeout_ms=ms;timeout_calls++;return timeout_fail;}
int stnc_platform_network_send(void *h,const unsigned char *p,size_t n)
{assert(h&&p&&n);return 0;}
int stnc_platform_network_receive(void *h,unsigned char *p,size_t n)
{assert(h);if(receive_fail)return 1;if(n==24)memcpy(p,reply,n);else memset(p,0,n);return 0;}
int stnc_mutex_init(stnc_mutex *m){m->handle=(void *)1;return 0;}
void stnc_mutex_destroy(stnc_mutex *m){m->handle=NULL;}
int stnc_mutex_lock(stnc_mutex *m){assert(m->handle);locks++;return 0;}
void stnc_mutex_unlock(stnc_mutex *m){assert(m->handle&&locks);locks--;}
int main(void)
{
    stnc_network_connection c={0};unsigned char request[24]={0},out[24];
    memcpy(request,"STNC",4);memcpy(reply,"STNC",4);reply[23]=8;
    assert(stnc_network_init()==0);
    assert(stnc_network_connect(&c,"test",1)==0);
    assert(stnc_network_send(&c,request,24)==0);
    assert(timeout_ms==60000&&timeout_calls==1&&locks==1);
    assert(stnc_network_receive(&c,out,24)==0&&locks==1);
    assert(stnc_network_receive(&c,out,8)==0&&locks==0);
    /* Empty protocol errors also release the request lock. */
    reply[23]=0;
    assert(stnc_network_send(&c,request,24)==0);
    assert(stnc_network_receive(&c,out,24)==0&&locks==0);
    receive_fail=1;
    assert(stnc_network_send(&c,request,24)==0);
    assert(stnc_network_receive(&c,out,24)!=0&&!c.connected&&locks==0);
    stnc_network_disconnect(&c);assert(!c.rpc_mutex.handle);
    assert(stnc_network_connect(&c,"test",1)==0);
    memcpy(request,"STNP",4);timeout_calls=0;
    assert(stnc_network_send(&c,request,24)==0&&timeout_calls==0&&locks==0);
    timeout_fail=1;memcpy(request,"STNC",4);
    assert(stnc_network_send(&c,request,24)!=0&&!c.connected&&locks==0);
    stnc_network_disconnect(&c);assert(closed==2);
    stnc_network_shutdown();puts("STNC RPC timeout and cleanup tests passed.");return 0;
}
