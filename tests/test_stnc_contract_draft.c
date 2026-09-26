#define _CRT_SECURE_NO_WARNINGS
#include "stnc_contract_draft.h"
#include "stnc_platform.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Contract draft test failed: %d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    stnc_contract_draft_input input;static uint8_t bytes[STNC_CONTRACT_DRAFT_MAX],terms[65536];
    char address[71],same[71],directory[900],path[1024];size_t n,i;uint8_t id[32];FILE *file;
    memset(&input,0,sizeof(input));input.type=1;input.created_at=0x0102030405060708ULL;
    input.participant_count=1;input.participants[0].role=4;
    strcpy(input.participants[0].public_key,"1111111111111111111111111111111111111111111111111111111111111111");
    input.terms=(const uint8_t *)"Terms\n";input.terms_length=6;
    CHECK(stnc_contract_draft_build(&input,bytes,sizeof(bytes),&n,address)==0&&n==72);
    CHECK(memcmp(bytes,"STCT\0\1\0\1",8)==0&&bytes[24]==0&&bytes[25]==1&&bytes[27]==1&&bytes[31]==6);
    for(i=0;i<8;++i)CHECK(bytes[8+i]==0&&bytes[16+i]==i+1);
    for(i=32;i<64;++i)CHECK(bytes[i]==0x11);
    CHECK(bytes[64]==0&&bytes[65]==4&&memcmp(bytes+66,"Terms\n",6)==0);
    CHECK(stnc_contract_draft_build(&input,bytes,sizeof(bytes),&n,same)==0&&strcmp(address,same)==0);
    CHECK(stnc_platform_init()==0&&stnc_platform_get_app_directory(directory,sizeof(directory))==0);
    snprintf(path,sizeof(path),"%s%ccontract-isolated-test.stct",directory,stnc_platform_path_separator());
    CHECK(stnc_contract_draft_save(&input,path,same)==0&&strcmp(address,same)==0);
    CHECK(stnc_contract_draft_save(&input,path,same)!=0);
    file=fopen(path,"rb");CHECK(file!=NULL);memset(bytes,0,sizeof(bytes));
    CHECK(fread(bytes,1,sizeof(bytes),file)==72&&memcmp(bytes+66,"Terms\n",6)==0);fclose(file);
    CHECK(remove(path)==0);stnc_platform_shutdown();
    CHECK(stnc_contract_draft_build(&input,bytes,71,&n,same)!=0&&n==0);
    input.type=7;CHECK(stnc_contract_draft_build(&input,bytes,sizeof(bytes),&n,same)!=0);input.type=1;
    input.participant_count=33;CHECK(stnc_contract_draft_build(&input,bytes,sizeof(bytes),&n,same)!=0);
    input.participant_count=32;for(i=1;i<32;++i)input.participants[i]=input.participants[0];
    input.terms=terms;input.terms_length=65536;
    CHECK(stnc_contract_draft_build(&input,bytes,sizeof(bytes),&n,same)==0&&n==sizeof(bytes));
    input.terms_length=65537;CHECK(stnc_contract_draft_build(&input,bytes,sizeof(bytes),&n,same)!=0);input.terms_length=0;
    input.participants[0].role=6;CHECK(stnc_contract_draft_build(&input,bytes,sizeof(bytes),&n,same)!=0);
    CHECK(stnc_contract_actor_decode("stnw0_1111111111111111111111111111111111111111111111111111111111111111",id)!=0);
    input.participants[0].public_key[5]='A';input.participants[0].role=1;
    CHECK(stnc_contract_draft_build(&input,bytes,sizeof(bytes),&n,same)!=0);
    puts("STNC Contract draft tests passed.");return 0;
}
