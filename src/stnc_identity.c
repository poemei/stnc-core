#define _CRT_SECURE_NO_WARNINGS
#include "stnc_identity.h"
#include "stnc_platform.h"
#include <stdio.h>
#include <string.h>
int stnc_ed25519_publickey(const uint8_t[32],uint8_t[32]);
int stn_ed25519_sign(const uint8_t *,size_t,const uint8_t[32],const uint8_t[32],uint8_t[64]);
#define IDENTITY_FILE_SIZE 69u
#define IDENTITY_PATH_MAX 1024u
static int identity_path(char path[IDENTITY_PATH_MAX])
{
    char base[IDENTITY_PATH_MAX],active[IDENTITY_PATH_MAX],name[50];FILE *f;size_t n,m;if(stnc_platform_get_app_directory(base,sizeof(base))!=0)return 1;n=strlen(base);if(n+16u>=sizeof(active))return 1;memcpy(active,base,n);active[n++]=stnc_platform_path_separator();memcpy(active+n,"active.profile",15u);
    f=fopen(active,"rb");if(f==NULL){m=strlen(base);if(m+14u>IDENTITY_PATH_MAX)return 1;memcpy(path,base,m);path[m++]=stnc_platform_path_separator();memcpy(path+m,"identity.key",13u);return 0;}
    m=fread(name,1,sizeof(name)-1u,f);if(ferror(f)||fgetc(f)!=EOF){fclose(f);return 1;}fclose(f);while(m&&(name[m-1]=='\n'||name[m-1]=='\r'))--m;name[m]=0;if(!m||strlen(base)+m+15u>=IDENTITY_PATH_MAX)return 1;
    n=strlen(base);memcpy(path,base,n);path[n++]=stnc_platform_path_separator();memcpy(path+n,"identity_",9u);n+=9u;memcpy(path+n,name,m);n+=m;memcpy(path+n,".key",5u);return 0;
}
static int describe(const uint8_t public_key[32],stnc_identity_status *status)
{
    static const char hex[]="0123456789abcdef";uint8_t digest[32];size_t i;if(stnc_platform_sha256(public_key,32,digest)!=0)return 1;status->present=1;status->valid=1;memcpy(status->public_key,public_key,32);memcpy(status->address,"stn0_",5);for(i=0;i<32;++i){status->address[5+2*i]=hex[digest[i]>>4];status->address[6+2*i]=hex[digest[i]&15];}status->address[69]=0;return 0;
}
static int load(const char *path,uint8_t file[IDENTITY_FILE_SIZE],int *present)
{
    FILE *f;uint8_t derived[32];int valid=0;*present=0;memset(file,0,IDENTITY_FILE_SIZE);f=fopen(path,"rb");if(f==NULL)return 1;*present=1;if(fread(file,1,IDENTITY_FILE_SIZE,f)==IDENTITY_FILE_SIZE&&fgetc(f)==EOF&&!ferror(f)&&memcmp(file,"STNI",4)==0&&file[4]==1&&stnc_ed25519_publickey(file+5,derived)==0&&memcmp(derived,file+37,32)==0)valid=1;fclose(f);stnc_platform_secure_clear(derived,sizeof(derived));if(!valid)stnc_platform_secure_clear(file,IDENTITY_FILE_SIZE);return valid?0:1;
}
int stnc_identity_status_at(const char *path,stnc_identity_status *status){uint8_t file[IDENTITY_FILE_SIZE];int rc;if(path==NULL||!path[0]||status==NULL)return 1;memset(status,0,sizeof(*status));rc=load(path,file,&status->present);if(rc==0){rc=describe(file+37,status);stnc_platform_secure_clear(file,sizeof(file));return rc;}stnc_platform_secure_clear(file,sizeof(file));return 0;}
int stnc_identity_create_at(const char *path,stnc_identity_status *status){uint8_t file[IDENTITY_FILE_SIZE];stnc_identity_status out;int rc=1;if(path==NULL||!path[0]||status==NULL)return 1;memset(file,0,sizeof(file));memset(&out,0,sizeof(out));memcpy(file,"STNI",4);file[4]=1;if(stnc_platform_random(file+5,32)!=0||stnc_ed25519_publickey(file+5,file+37)!=0)goto done;if(describe(file+37,&out)!=0)goto done;if(stnc_platform_write_private_file(path,file,sizeof(file))!=0)goto done;*status=out;rc=0;done:stnc_platform_secure_clear(file,sizeof(file));return rc;}
int stnc_identity_sign_at(const char *path,const uint8_t *statement,size_t length,uint8_t signature[64]){uint8_t file[IDENTITY_FILE_SIZE],sig[64];int present,rc=1;if(path==NULL||!path[0]||statement==NULL||length==0||signature==NULL)return 1;if(load(path,file,&present)==0&&stn_ed25519_sign(statement,length,file+37,file+5,sig)==0){memcpy(signature,sig,64);rc=0;}stnc_platform_secure_clear(file,sizeof(file));stnc_platform_secure_clear(sig,sizeof(sig));return rc;}
int stnc_identity_status_read(stnc_identity_status *status){char path[IDENTITY_PATH_MAX];return identity_path(path)==0?stnc_identity_status_at(path,status):1;}
int stnc_identity_create(stnc_identity_status *status){char path[IDENTITY_PATH_MAX];return identity_path(path)==0?stnc_identity_create_at(path,status):1;}
int stnc_identity_sign(const uint8_t *statement,size_t length,uint8_t signature[64]){char path[IDENTITY_PATH_MAX];return identity_path(path)==0?stnc_identity_sign_at(path,statement,length,signature):1;}
