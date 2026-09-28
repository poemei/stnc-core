#include <stdio.h>
#include <string.h>
#include "stnc_network.h"
#include "stnc_platform.h"

static int send_error,receive_error,disconnects;
int stnc_platform_network_timeout(void *handle,unsigned int ms){(void)handle;(void)ms;return 0;}
int stnc_platform_network_init(void){return 0;}
void stnc_platform_network_shutdown(void){}
int stnc_platform_network_connect(void **handle,const char *peer,unsigned short port)
{if(handle==NULL||peer==NULL||!*peer||port==0)return 1;*handle=(void*)1;return 0;}
int stnc_platform_network_send(void *handle,const unsigned char *buffer,size_t length)
{(void)handle;(void)buffer;(void)length;return send_error;}
int stnc_platform_network_receive(void *handle,unsigned char *buffer,size_t length)
{(void)handle;if(buffer!=NULL&&length)memset(buffer,0,length);return receive_error;}
void stnc_platform_network_disconnect(void *handle){(void)handle;disconnects++;}

static int fail(int line){fprintf(stderr,"STNC network test failed at line %d.\n",line);return 1;}
#define CHECK(x) do{if(!(x))return fail(__LINE__);}while(0)

int main(void)
{
    stnc_network_connection c={0};unsigned char byte=0;
    CHECK(stnc_network_init()==0);
    CHECK(stnc_network_connect(&c,"chain",18473u)==0);
    CHECK(stnc_network_is_connected(&c));
    send_error=1;
    CHECK(stnc_network_send(&c,&byte,1u)!=0);
    CHECK(!stnc_network_is_connected(&c));
    CHECK(disconnects==1);

    send_error=0;
    CHECK(stnc_network_connect(&c,"chain",18473u)==0);
    receive_error=1;
    CHECK(stnc_network_receive(&c,&byte,1u)!=0);
    CHECK(!stnc_network_is_connected(&c));
    CHECK(disconnects==2);

    receive_error=0;
    CHECK(stnc_network_connect(&c,"chain",18473u)==0);
    CHECK(stnc_network_send(&c,&byte,1u)==0);
    CHECK(stnc_network_receive(&c,&byte,1u)==0);
    CHECK(stnc_network_is_connected(&c));
    stnc_network_disconnect(&c);
    CHECK(!stnc_network_is_connected(&c));
    CHECK(disconnects==3);
    stnc_network_shutdown();
    puts("STNC network tests passed.");return 0;
}
