#define _CRT_SECURE_NO_WARNINGS
#include "stnc_profile.h"
#include "stnc_platform.h"
#include "stnc_wallet_store.h"
#include "stnc_identity.h"

#include <stdio.h>
#include <string.h>

#define ACTIVE_FILE "active.profile"
#define PROFILE_LIST_FILE "profiles.list"

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
    char path[STNC_PROFILE_PATH_MAX];FILE *f;size_t n;
    if(!stnc_profile_name_valid(name)||app_file(ACTIVE_FILE,path)!=0)return 1;n=strlen(name);f=fopen(path,"wb");if(f==NULL)return 1;
    if(fwrite(name,1,n,f)!=n||fputc('\n',f)==EOF||fflush(f)!=0){fclose(f);return 1;}return fclose(f)==0?0:1;
}

static int profile_list_contains(const char *name)
{
    char path[STNC_PROFILE_PATH_MAX],line[STNC_PROFILE_NAME_MAX+4u];FILE *f;
    if(app_file(PROFILE_LIST_FILE,path)!=0)return 0;f=fopen(path,"rb");if(f==NULL)return 0;
    while(fgets(line,sizeof(line),f)!=NULL){size_t n=strlen(line);while(n&&(line[n-1]=='\n'||line[n-1]=='\r'))line[--n]='\0';if(strcmp(line,name)==0){fclose(f);return 1;}}
    fclose(f);return 0;
}

static int profile_list_add(const char *name)
{
    char path[STNC_PROFILE_PATH_MAX];FILE *f;
    if(profile_list_contains(name))return 0;if(app_file(PROFILE_LIST_FILE,path)!=0)return 1;
    f=fopen(path,"ab");if(f==NULL)return 1;if(fprintf(f,"%s\n",name)<0||fflush(f)!=0){fclose(f);return 1;}return fclose(f)==0?0:1;
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

int stnc_profile_list(stnc_profile_info *profiles,size_t capacity,size_t *count)
{
    char path[STNC_PROFILE_PATH_MAX],line[STNC_PROFILE_NAME_MAX+4u];FILE *f;size_t used=0u;
    if(count==NULL||(capacity&&profiles==NULL))return 1;
    if(capacity){stnc_profile_info d;memset(&d,0,sizeof(d));memcpy(d.name,"Default",8u);if(app_file("wallet.key",d.wallet_path)!=0||app_file("identity.key",d.identity_path)!=0)return 1;profiles[used++]=d;}
    if(app_file(PROFILE_LIST_FILE,path)!=0)return 1;f=fopen(path,"rb");
    if(f!=NULL){while(fgets(line,sizeof(line),f)!=NULL&&used<capacity){size_t n=strlen(line),i;int duplicate=0;while(n&&(line[n-1]=='\n'||line[n-1]=='\r'))line[--n]='\0';if(!stnc_profile_name_valid(line)||strcmp(line,"Default")==0)continue;for(i=0u;i<used;i++)if(strcmp(profiles[i].name,line)==0)duplicate=1;if(!duplicate&&stnc_profile_paths(line,&profiles[used])==0)used++;}fclose(f);}
    *count=used;return 0;
}

int stnc_profile_select(const char *name,stnc_profile_info *profile)
{
    stnc_profile_info p;
    if(strcmp(name,"Default")==0){memset(&p,0,sizeof(p));memcpy(p.name,"Default",8u);if(app_file("wallet.key",p.wallet_path)!=0||app_file("identity.key",p.identity_path)!=0)return 1;}
    else if(stnc_profile_paths(name,&p)!=0)return 1;
    if(!stnc_wallet_store_exists_at(p.wallet_path))return 1;
    {stnc_identity_status s;if(stnc_identity_status_at(p.identity_path,&s)!=0||!s.present||!s.valid)return 1;}
    if(strcmp(name,"Default")==0){char active[STNC_PROFILE_PATH_MAX];if(app_file(ACTIVE_FILE,active)!=0)return 1;remove(active);}else if(active_write(name)!=0)return 1;
    if(profile!=NULL)*profile=p;return 0;
}

int stnc_profile_create(const char *name,stnc_profile_info *profile)
{
    stnc_profile_info p;stnc_wallet_key wallet;stnc_identity_status identity;
    if(strcmp(name,"Default")==0||stnc_profile_paths(name,&p)!=0||stnc_wallet_store_exists_at(p.wallet_path))return 1;
    memset(&wallet,0,sizeof(wallet));memset(&identity,0,sizeof(identity));
    if(stnc_wallet_store_create_at(p.wallet_path,&wallet)!=0){stnc_wallet_clear(&wallet);return 1;}stnc_wallet_clear(&wallet);
    if(stnc_identity_create_at(p.identity_path,&identity)!=0){remove(p.wallet_path);return 1;}
    if(profile_list_add(name)!=0||active_write(name)!=0){remove(p.wallet_path);remove(p.identity_path);return 1;}
    if(profile!=NULL)*profile=p;return 0;
}
