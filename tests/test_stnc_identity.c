#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <aclapi.h>
#include "stnc_identity.h"
#include "stnc_platform.h"
#include <stdio.h>
#include <string.h>
int stn_ed25519_verify(const unsigned char *,size_t,const unsigned char[32],const unsigned char[64]);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Identity test failed: %d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    char path[1024],directory[900];stnc_identity_status before,after;
    uint8_t signature[64],statement[]={1,2,3};FILE *file;
    CHECK(stnc_platform_init()==0);
    CHECK(stnc_platform_get_app_directory(directory,sizeof(directory))==0);
    snprintf(path,sizeof(path),"%s%cidentity-isolated-test.key",directory,stnc_platform_path_separator());
    /* Refuse to overwrite leftover test evidence, just like production custody. */
    CHECK(stnc_identity_status_at(path,&before)==0&&!before.present);
    CHECK(stnc_identity_sign_at(path,statement,sizeof(statement),signature)!=0);
    CHECK(stnc_identity_create_at(path,&before)==0&&before.valid&&before.present);
    {
        PSECURITY_DESCRIPTOR descriptor=NULL;PACL acl=NULL;PSID owner=NULL;
        SECURITY_DESCRIPTOR_CONTROL control;DWORD revision;ACCESS_ALLOWED_ACE *ace;
        CHECK(GetNamedSecurityInfoA(path,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION|OWNER_SECURITY_INFORMATION,
            &owner,NULL,&acl,NULL,&descriptor)==ERROR_SUCCESS);
        CHECK(GetSecurityDescriptorControl(descriptor,&control,&revision));
        CHECK((control&SE_DACL_PROTECTED)!=0&&acl!=NULL&&acl->AceCount==1);
        CHECK(GetAce(acl,0,(void **)&ace)&&ace->Header.AceType==ACCESS_ALLOWED_ACE_TYPE);
        CHECK(EqualSid(owner,(PSID)&ace->SidStart));LocalFree(descriptor);
    }
    CHECK(strlen(before.address)==69&&strncmp(before.address,"stn0_",5)==0);
    CHECK(stnc_identity_create_at(path,&after)!=0);
    CHECK(stnc_identity_status_at(path,&after)==0&&after.valid);
    CHECK(strcmp(before.address,after.address)==0&&memcmp(before.public_key,after.public_key,32)==0);
    CHECK(stnc_identity_sign_at(path,statement,sizeof(statement),signature)==0);
    CHECK(stn_ed25519_verify(statement,sizeof(statement),before.public_key,signature)==0);
    statement[0]^=1;CHECK(stn_ed25519_verify(statement,sizeof(statement),before.public_key,signature)!=0);
    file=fopen(path,"r+b");CHECK(file!=NULL);CHECK(fputc('W',file)!=EOF);CHECK(fclose(file)==0);
    CHECK(stnc_identity_status_at(path,&after)==0&&after.present&&!after.valid);
    CHECK(stnc_identity_sign_at(path,statement,sizeof(statement),signature)!=0);
    CHECK(stnc_identity_create_at(path,&after)!=0);
    CHECK(remove(path)==0);
    CHECK(stnc_identity_create_at(NULL,&after)!=0&&stnc_identity_status_at(path,NULL)!=0);
    stnc_platform_shutdown();puts("STNC identity tests passed.");return 0;
}
