#include <stdio.h>
#include <string.h>

#include "stnc_platform.h"
#include "stnc_wallet.h"
#include "stnc_wallet_store.h"

#define TEST_WALLET_FILE "stnc-wallet-store-test.key"

static int wallet_path(char *path,size_t capacity)
{
    size_t n;
    if(stnc_platform_get_app_directory(path,capacity)!=0)return 1;
    n=strlen(path);
    if(n+1u+strlen(TEST_WALLET_FILE)+1u>capacity)return 1;
    path[n++]=stnc_platform_path_separator();
    memcpy(path+n,TEST_WALLET_FILE,strlen(TEST_WALLET_FILE)+1u);
    return 0;
}

int main(void)
{
    char path[1024];
    stnc_wallet_key created,loaded;
    char a[STNC_WALLET_ADDRESS_SIZE+1u],b[STNC_WALLET_ADDRESS_SIZE+1u];
    FILE *file;
    int value;

    memset(&created,0,sizeof(created));memset(&loaded,0,sizeof(loaded));
    if(stnc_platform_init()!=0||wallet_path(path,sizeof(path))!=0)return 1;
    remove(path);
    if(stnc_wallet_store_exists_at(path))return 1;
    if(stnc_wallet_store_create_at(path,&created)!=0||!stnc_wallet_store_exists_at(path)||
       stnc_wallet_address(&created,a)!=0||stnc_wallet_store_load_at(path,&loaded)!=0||
       stnc_wallet_address(&loaded,b)!=0||strcmp(a,b)!=0)return 1;
    stnc_wallet_clear(&created);stnc_wallet_clear(&loaded);

    file=fopen(path,"r+b");if(file==NULL)return 1;
    if(fseek(file,5L,SEEK_SET)!=0){fclose(file);return 1;}
    value=fgetc(file);if(value==EOF){fclose(file);return 1;}
    if(fseek(file,5L,SEEK_SET)!=0||fputc(value^1,file)==EOF||fclose(file)!=0)return 1;
    memset(&loaded,0xa5,sizeof(loaded));
    if(stnc_wallet_store_load_at(path,&loaded)==0)return 1;
    {
        size_t i;const unsigned char *bytes=(const unsigned char *)&loaded;
        for(i=0;i<sizeof(loaded);i++)if(bytes[i]!=0u)return 1;
    }
    remove(path);stnc_platform_shutdown();
    printf("Wallet store tests passed.\n");
    return 0;
}
