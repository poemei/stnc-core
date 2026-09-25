#include <stdio.h>
#include <string.h>
#include "stnc_stratum_client.h"

static uint8_t sent[256];static size_t sent_len;static uint8_t incoming[512];static size_t incoming_len,incoming_at;static int disconnected;static int ready;
int stnc_platform_network_connect(void **handle,const char *peer,unsigned short port)
{if(handle==NULL||strcmp(peer,"stratum.test")!=0||port!=18475u)return 1;*handle=(void *)1;return 0;}
int stnc_platform_network_send(void *handle,const unsigned char *buffer,size_t length)
{if(handle!=(void *)1||sent_len+length>sizeof(sent))return 1;memcpy(sent+sent_len,buffer,length);sent_len+=length;return 0;}
int stnc_platform_network_read_ready(void *handle){return handle==(void *)1?ready:-1;}
int stnc_platform_network_receive(void *handle,unsigned char *buffer,size_t length)
{if(handle!=(void *)1||incoming_at+length>incoming_len)return 1;memcpy(buffer,incoming+incoming_at,length);incoming_at+=length;return 0;}
void stnc_platform_network_disconnect(void *handle){if(handle==(void *)1)disconnected++;}

static void w32(uint8_t *p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static void w64(uint8_t *p,uint64_t v){size_t i;for(i=0u;i<8u;i++){p[7u-i]=(uint8_t)(v&255u);v>>=8;}}
int main(void)
{
    stnc_stratum_client client;stnc_stratum_job job;stnc_stratum_result result;uint8_t block[168];char address[70];size_t i;
    memcpy(address,"stn0_",5u);for(i=5u;i<69u;i++)address[i]='a';address[69]='\0';
    stnc_stratum_client_init(&client);
    if(stnc_stratum_client_connect(&client,"stratum.test",18475u,address)!=0||sent_len!=77u||sent[5]!=5u)return 1;

    memset(incoming,0,sizeof(incoming));incoming[0]='S';incoming[1]='T';incoming[2]='N';incoming[3]='M';incoming[4]=1u;incoming[5]=1u;
    for(i=0u;i<32u;i++){incoming[8u+i]=(uint8_t)i;incoming[72u+i]=(uint8_t)(i+1u);incoming[116u+120u+i]=(uint8_t)(i+1u);}
    w32(incoming+104u,168u);w64(incoming+108u,9u);w64(incoming+116u+152u,9u);incoming_len=284u;incoming_at=0u;
    ready=0;if(stnc_stratum_client_poll_job(&client,&job,block,sizeof(block))!=1||incoming_at!=0u)return 1;
    ready=1;if(stnc_stratum_client_poll_job(&client,&job,block,sizeof(block))!=0||job.initial_nonce!=9u)return 1;

    sent_len=0u;memset(incoming,0,sizeof(incoming));incoming[0]='S';incoming[1]='T';incoming[2]='N';incoming[3]='M';incoming[4]=1u;incoming[5]=3u;
    w32(incoming+8u,0u);incoming_len=12u;incoming_at=0u;
    if(stnc_stratum_client_submit(&client,job.work_id,9u,&result)!=0||result!=STNC_STRATUM_RESULT_ACCEPTED||sent_len!=48u)return 1;
    sent_len=0u;if(stnc_stratum_client_progress(&client,job.work_id,100u,1000u)!=0||sent_len!=56u||sent[5]!=4u)return 1;
    stnc_stratum_client_disconnect(&client);if(client.connected||disconnected!=1)return 1;
    puts("Stratum client tests passed.");return 0;
}
