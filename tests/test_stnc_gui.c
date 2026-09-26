#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
static DWORD child_pid;static HWND app;
static BOOL CALLBACK find_window(HWND w,LPARAM x){DWORD pid=0;char name[64];(void)x;GetWindowThreadProcessId(w,&pid);GetClassNameA(w,name,sizeof(name));if(pid==child_pid&&strcmp(name,"STNCCoreWindow")==0)app=w;return TRUE;}
static int wait_control(int id,int enabled){ULONGLONG until=GetTickCount64()+45000;while(GetTickCount64()<until){HWND h=GetDlgItem(app,id);if(h&&IsWindowEnabled(h)==enabled)return 0;Sleep(50);}return 1;}
static int wait_visible(int id){ULONGLONG until=GetTickCount64()+45000;while(GetTickCount64()<until){HWND h=GetDlgItem(app,id);if(h&&IsWindowVisible(h))return 0;Sleep(50);}return 1;}
static int stays_visible(int id,DWORD ms){ULONGLONG until=GetTickCount64()+ms;while(GetTickCount64()<until){HWND h=GetDlgItem(app,id);if(!h||!IsWindowVisible(h))return 1;Sleep(50);}return 0;}
static void click(int id){SendMessage(app,WM_COMMAND,MAKEWPARAM(id,BN_CLICKED),(LPARAM)GetDlgItem(app,id));}
static void menu_command(int id){SendMessage(app,WM_COMMAND,MAKEWPARAM(id,0),0);}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"GUI smoke test failed: %d\n",__LINE__);failed=1;goto cleanup;}}while(0)
int main(void){char temp[MAX_PATH],dir[MAX_PATH]={0},path[MAX_PATH],exe[MAX_PATH],cmd[MAX_PATH+16];STARTUPINFOA si;PROCESS_INFORMATION pi={0};FILE *f;DWORD code;RECT r;int failed=0,started=0;size_t i;const char *files[]={"stnc-core.exe","config.json","wallet.key","identity.key","stnc-core.log"};
 CHECK(GetTempPathA(sizeof(temp),temp)>0);CHECK(GetTempFileNameA(temp,"stc",0,dir)!=0);CHECK(DeleteFileA(dir));CHECK(CreateDirectoryA(dir,NULL));snprintf(exe,sizeof(exe),"%s\\stnc-core.exe",dir);CHECK(CopyFileA("build\\stnc-core.exe",exe,TRUE));snprintf(path,sizeof(path),"%s\\config.json",dir);f=fopen(path,"wb");CHECK(f!=NULL);fputs("{\"peer\":\"127.0.0.1\",\"port\":1,\"root_peer\":\"127.0.0.1\",\"root_peer_port\":1,\"mining_enabled\":false,\"mining_backend\":\"automatic\",\"mining_cpu_limit_percent\":2,\"stratum_host\":\"127.0.0.1\",\"stratum_port\":1}",f);fclose(f);
 memset(&si,0,sizeof(si));si.cb=sizeof(si);snprintf(cmd,sizeof(cmd),"\"%s\" gui",exe);CHECK(CreateProcessA(exe,cmd,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,dir,&si,&pi));started=1;child_pid=pi.dwProcessId;for(i=0;i<200&&!app;++i){EnumWindows(find_window,0);Sleep(50);}CHECK(app!=NULL);Sleep(1000);
 CHECK(GetWindowRect(app,&r));CHECK((r.right-r.left)<=640&&(r.bottom-r.top)<=420);CHECK(GetMenu(app)!=NULL);CHECK(GetDlgItem(app,300)==NULL&&GetDlgItem(app,301)==NULL&&GetDlgItem(app,302)==NULL&&GetDlgItem(app,303)==NULL&&GetDlgItem(app,304)==NULL&&GetDlgItem(app,305)==NULL);
 /* Current GUI menu IDs: Address 2001, Identity 2002, Contract 2003, Mining 2008. */
 menu_command(2001);CHECK(wait_visible(102)==0);CHECK(stays_visible(102,2500)==0);
 menu_command(2002);CHECK(wait_visible(103)==0);CHECK(stays_visible(103,2500)==0);
 menu_command(2008);CHECK(wait_visible(107)==0);CHECK(wait_visible(108)==0);CHECK(stays_visible(107,2500)==0);CHECK(stays_visible(108,2500)==0);
 menu_command(2003);CHECK(wait_visible(113)==0);CHECK(wait_visible(114)==0);CHECK(wait_visible(112)==0);CHECK(stays_visible(113,2500)==0);CHECK(stays_visible(114,2500)==0);CHECK(stays_visible(112,2500)==0);
 CHECK(wait_control(107,FALSE)==0);menu_command(2001);click(102);CHECK(wait_control(107,TRUE)==0);menu_command(2008);click(107);CHECK(wait_control(107,FALSE)==0);CHECK(wait_control(108,TRUE)==0);click(108);CHECK(wait_control(107,TRUE)==0);CHECK(wait_control(108,FALSE)==0);
 menu_command(2002);click(103);menu_command(2003);CHECK(wait_visible(112)==0);snprintf(path,sizeof(path),"%s\\wallet.key",dir);CHECK(GetFileAttributesA(path)!=INVALID_FILE_ATTRIBUTES);snprintf(path,sizeof(path),"%s\\identity.key",dir);CHECK(GetFileAttributesA(path)!=INVALID_FILE_ATTRIBUTES);CHECK(GetExitCodeProcess(pi.hProcess,&code)&&code==STILL_ACTIVE);
cleanup:if(started){PostMessage(app,WM_CLOSE,0,0);if(WaitForSingleObject(pi.hProcess,45000)!=WAIT_OBJECT_0){TerminateProcess(pi.hProcess,1);failed=1;}CloseHandle(pi.hThread);CloseHandle(pi.hProcess);}if(dir[0]){for(i=0;i<sizeof(files)/sizeof(files[0]);++i){snprintf(path,sizeof(path),"%s\\%s",dir,files[i]);DeleteFileA(path);}RemoveDirectoryA(dir);}if(!failed)puts("STNC native GUI smoke tests passed.");return failed;}
