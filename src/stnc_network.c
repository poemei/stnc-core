#include <stddef.h>
#include <string.h>

#include "stnc_network.h"
#include "stnc_platform.h"

#define STNC_RPC_HEADER_SIZE 24u

static int network_initialized = 0;

static int stnc_network_is_stnc(const unsigned char *buffer,size_t length)
{
    return buffer!=NULL&&length>=4u&&memcmp(buffer,"STNC",4u)==0;
}

static size_t stnc_network_rpc_payload_length(const unsigned char *header)
{
    return ((size_t)header[20]<<24)|((size_t)header[21]<<16)|
           ((size_t)header[22]<<8)|(size_t)header[23];
}

static void stnc_network_release_rpc(stnc_network_connection *connection)
{
    if(connection==NULL||!connection->rpc_active)return;
    connection->rpc_active=0;
    connection->rpc_payload_remaining=0u;
    stnc_mutex_unlock(&connection->rpc_mutex);
}

static void stnc_network_fail(stnc_network_connection *connection)
{
    if(connection==NULL)return;
    if(connection->handle!=NULL)stnc_platform_network_disconnect(connection->handle);
    connection->handle=NULL;
    connection->connected=0;
    stnc_network_release_rpc(connection);
}

int stnc_network_init(void)
{
    if(network_initialized)return 1;
    if(stnc_platform_network_init()!=0)return 1;
    network_initialized=1;
    return 0;
}

void stnc_network_shutdown(void)
{
    if(!network_initialized)return;
    stnc_platform_network_shutdown();
    network_initialized=0;
}

int stnc_network_connect(stnc_network_connection *connection,const char *peer,unsigned short port)
{
    if(!network_initialized||connection==NULL||peer==NULL||peer[0]=='\0'||port==0)return 1;
    memset(connection,0,sizeof(*connection));
    if(stnc_mutex_init(&connection->rpc_mutex)!=0)return 1;
    if(stnc_platform_network_connect(&connection->handle,peer,port)!=0){
        connection->handle=NULL;
        stnc_mutex_destroy(&connection->rpc_mutex);
        return 1;
    }
    connection->connected=1;
    return 0;
}

int stnc_network_send(stnc_network_connection *connection,const unsigned char *buffer,size_t length)
{
    int is_rpc;
    if(!network_initialized||connection==NULL||!connection->connected||connection->handle==NULL||buffer==NULL||length==0)return 1;
    is_rpc=stnc_network_is_stnc(buffer,length);
    if(is_rpc){
        if(stnc_mutex_lock(&connection->rpc_mutex)!=0)return 1;
        connection->rpc_active=1;
        connection->rpc_payload_remaining=0u;
    }
    if(stnc_platform_network_send(connection->handle,buffer,length)!=0){
        stnc_network_fail(connection);
        return 1;
    }
    return 0;
}

int stnc_network_receive(stnc_network_connection *connection,unsigned char *buffer,size_t length)
{
    if(!network_initialized||connection==NULL||!connection->connected||connection->handle==NULL||buffer==NULL||length==0)return 1;
    if(stnc_platform_network_receive(connection->handle,buffer,length)!=0){
        stnc_network_fail(connection);
        return 1;
    }
    if(connection->rpc_active){
        if(connection->rpc_payload_remaining==0u){
            if(length!=STNC_RPC_HEADER_SIZE||!stnc_network_is_stnc(buffer,length)){
                stnc_network_fail(connection);
                return 1;
            }
            connection->rpc_payload_remaining=stnc_network_rpc_payload_length(buffer);
            if(connection->rpc_payload_remaining==0u)stnc_network_release_rpc(connection);
        }else{
            if(length>connection->rpc_payload_remaining){
                stnc_network_fail(connection);
                return 1;
            }
            connection->rpc_payload_remaining-=length;
            if(connection->rpc_payload_remaining==0u)stnc_network_release_rpc(connection);
        }
    }
    return 0;
}

void stnc_network_disconnect(stnc_network_connection *connection)
{
    if(connection==NULL)return;
    if(connection->handle!=NULL)stnc_platform_network_disconnect(connection->handle);
    connection->handle=NULL;
    connection->connected=0;
    stnc_network_release_rpc(connection);
    stnc_mutex_destroy(&connection->rpc_mutex);
}

int stnc_network_is_connected(const stnc_network_connection *connection)
{
    return connection!=NULL&&connection->connected&&connection->handle!=NULL;
}
