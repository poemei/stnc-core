#define _CRT_SECURE_NO_WARNINGS

#include "stnc_wallet_store.h"
#include "stnc_platform.h"
#include <stdio.h>
#include <string.h>

#define STNC_WALLET_FILE "wallet.key"
#define STNC_WALLET_FILE_MAGIC "STNW"
#define STNC_WALLET_FILE_VERSION 1u
#define STNC_WALLET_PATH_MAX 1024u
#define STNC_WALLET_FILE_SIZE 69u

static int wallet_path(char path[STNC_WALLET_PATH_MAX])
{
    size_t n;
    if(stnc_platform_get_app_directory(path,STNC_WALLET_PATH_MAX)!=0)return 1;
    n=strlen(path);
    if(n+1u+strlen(STNC_WALLET_FILE)+1u>STNC_WALLET_PATH_MAX)return 1;
    path[n++]=stnc_platform_path_separator();memcpy(path+n,STNC_WALLET_FILE,strlen(STNC_WALLET_FILE)+1u);
    return 0;
}
int stnc_wallet_store_exists_at(const char *path)
{
    FILE *f;
    if(path==NULL||path[0]=='\0')return 0;
    f=fopen(path,"rb");if(f==NULL)return 0;fclose(f);return 1;
}

int stnc_wallet_store_create_at(const char *path,stnc_wallet_key *key)
{
    uint8_t file[STNC_WALLET_FILE_SIZE];stnc_wallet_key generated;
    if(path==NULL||path[0]=='\0'||key==NULL||stnc_wallet_store_exists_at(path))return 1;
    memset(&generated,0,sizeof(generated));memset(file,0,sizeof(file));
    if(stnc_wallet_generate(&generated)!=0)return 1;
    memcpy(file,STNC_WALLET_FILE_MAGIC,4);file[4]=STNC_WALLET_FILE_VERSION;
    memcpy(file+5,generated.private_key,32);memcpy(file+37,generated.public_key,32);
    if(stnc_platform_write_private_file(path,file,sizeof(file))!=0){
        stnc_wallet_clear(&generated);stnc_platform_secure_clear(file,sizeof(file));return 1;
    }
    *key=generated;stnc_platform_secure_clear(file,sizeof(file));return 0;
}

int stnc_wallet_store_load_at(const char *path,stnc_wallet_key *key)
{
    FILE *f;uint8_t file[STNC_WALLET_FILE_SIZE],derived[STNC_WALLET_PUBLIC_KEY_SIZE];stnc_wallet_key loaded;int extra;
    if(path==NULL||path[0]=='\0'||key==NULL)return 1;
    memset(key,0,sizeof(*key));
    memset(&loaded,0,sizeof(loaded));memset(file,0,sizeof(file));memset(derived,0,sizeof(derived));
    f=fopen(path,"rb");if(f==NULL)return 1;
    if(fread(file,1,sizeof(file),f)!=sizeof(file)){
        fclose(f);stnc_platform_secure_clear(file,sizeof(file));return 1;
    }
    extra=fgetc(f);fclose(f);
    if(extra!=EOF||memcmp(file,STNC_WALLET_FILE_MAGIC,4)!=0||file[4]!=STNC_WALLET_FILE_VERSION){
        stnc_platform_secure_clear(file,sizeof(file));return 1;
    }
    memcpy(loaded.private_key,file+5,32);memcpy(loaded.public_key,file+37,32);
    if(stnc_ed25519_publickey(loaded.private_key,derived)!=0||memcmp(derived,loaded.public_key,sizeof(derived))!=0){
        stnc_platform_secure_clear(derived,sizeof(derived));stnc_platform_secure_clear(file,sizeof(file));
        stnc_wallet_clear(&loaded);return 1;
    }
    stnc_platform_secure_clear(derived,sizeof(derived));stnc_platform_secure_clear(file,sizeof(file));*key=loaded;return 0;
}

int stnc_wallet_store_exists(void)
{
    char path[STNC_WALLET_PATH_MAX];
    return wallet_path(path)==0?stnc_wallet_store_exists_at(path):0;
}

int stnc_wallet_store_create(stnc_wallet_key *key)
{
    char path[STNC_WALLET_PATH_MAX];
    return wallet_path(path)==0?stnc_wallet_store_create_at(path,key):1;
}

int stnc_wallet_store_load(stnc_wallet_key *key)
{
    char path[STNC_WALLET_PATH_MAX];
    return wallet_path(path)==0?stnc_wallet_store_load_at(path,key):1;
}
