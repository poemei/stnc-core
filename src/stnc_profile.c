#define _CRT_SECURE_NO_WARNINGS
#include "stnc_profile.h"
#include "stnc_platform.h"
#include "stnc_wallet_store.h"
#include "stnc_identity.h"

#include <stdio.h>
#include <string.h>

#define ACTIVE_FILE "active.profile"

static int app_file(const char *file,char path[STNC_PROFILE_PATH_MAX])
{
    size_t n,m;
    if(file==NULL||path==NULL||stnc_platform_get_app_directory(path,STNC_PROFILE_PATH_MAX)!=0)return 1;
    n=strlen(path);m=strlen(file);
    if(n+1u+m+1u>STNC_PROFILE_PATH_MAX)return 1;
    path[n++]=stnc_platform_path_separator();memcpy(path+n,file,m+1u);return 0;
}

int stnc_profile_name_valid(const char *name)
{
    size_t i,n;if(name==NULL)return 0;n=strlen(name);if(n==0u||n>STNC_PROFILE_NAME_MAX)return 0;
    for(i=0u;i<n;i++)if(!((name[i]>='a'&&name[i]<='z')||(name[i]>='A'&&name[i]<='Z')||
        (name[i]>='0'&&name[i]<='9')||name[i]=='-'||name[i]=='_'||name[i]==' '))return 0;
    return name[0]!=' '&&name[n-1]!=' ';
}

int stnc_profile_paths(const char *name,stnc_profile_info *profile)
{
    char file[96];int n;if(!stnc_profile_name_valid(name)||profile==NULL)return 1;memset(profile,0,sizeof(*profile));
    memcpy(profile->name,name,strlen(name)+1u);
    n=snprintf(file,sizeof(file),"wallet_%s.key",name);if(n<0||(size_t)n>=sizeof(file)||app_file(file,profile->wallet_path)!=0)return 1;
    n=snprintf(file,sizeof(file),"identity_%s.key",name);if(n<0||(size_t)n>=sizeof(file)||app_file(file,profile->identity_path)!=0)return 1;
    return 0;
}

static int active_write(const char *name)
{
    char path[STNC_PROFILE_PATH_MAX],text[STNC_PROFILE_NAME_MAX+2u];size_t n;
    if(!stnc_profile_name_valid(name)||app_file(ACTIVE_FILE,path)!=0)return 1;n=strlen(name);memcpy(text,name,n);text[n++]='\n';
    return stnc_platform_write_file_replace(path,(const uint8_t *)text,n);
}

int stnc_profile_active(stnc_profile_info *profile)
{
    char path[STNC_PROFILE_PATH_MAX],name[STNC_PROFILE_NAME_MAX+2u];FILE *f;size_t n;
    if(profile==NULL||app_file(ACTIVE_FILE,path)!=0)return 1;f=fopen(path,"rb");
    if(f==NULL){memset(profile,0,sizeof(*profile));memcpy(profile->name,"Default",8u);
        if(app_file("wallet.key",profile->wallet_path)!=0||app_file("identity.key",profile->identity_path)!=0)return 1;return 0;}
    n=fread(name,1,sizeof(name)-1u,f);if(ferror(f)||fgetc(f)!=EOF){fclose(f);return 1;}fclose(f);
    while(n>0u&&(name[n-1u]=='\n'||name[n-1u]=='\r'))--n;name[n]='\0';return stnc_profile_paths(name,profile);
}

int stnc_profile_select(const char *name,stnc_profile_info *profile)
{
    stnc_profile_info p;if(stnc_profile_paths(name,&p)!=0||!stnc_wallet_store_exists_at(p.wallet_path))return 1;
    {stnc_identity_status s;if(stnc_identity_status_at(p.identity_path,&s)!=0||!s.present||!s.valid)return 1;}
    if(active_write(name)!=0)return 1;if(profile!=NULL)*profile=p;return 0;
}

int stnc_profile_create(const char *name,stnc_profile_info *profile)
{
    stnc_profile_info p;stnc_wallet_key wallet;stnc_identity_status identity;
    if(stnc_profile_paths(name,&p)!=0||stnc_wallet_store_exists_at(p.wallet_path))return 1;
    memset(&wallet,0,sizeof(wallet));memset(&identity,0,sizeof(identity));
    if(stnc_wallet_store_create_at(p.wallet_path,&wallet)!=0){stnc_wallet_clear(&wallet);return 1;}stnc_wallet_clear(&wallet);
    if(stnc_identity_create_at(p.identity_path,&identity)!=0)return 1;
    if(active_write(name)!=0)return 1;if(profile!=NULL)*profile=p;return 0;
}
