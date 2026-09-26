#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
static DWORD child_pid;
static HWND app;
static BOOL CALLBACK find_window(HWND window,LPARAM unused)
{DWORD pid=0;char name[64];(void)unused;GetWindowThreadProcessId(window,&pid);GetClassNameA(window,name,sizeof(name));if(pid==child_pid&&strcmp(name,"STNCCoreWindow")==0)app=window;return TRUE;}
static int wait_enabled(int id)
{ULONGLONG until=GetTickCount64()+45000;while(GetTickCount64()<until){if(app&&IsWindowVisible(app)&&IsWindowEnabled(GetDlgItem(app,id)))return 0;Sleep(50);}return 1;}
static void click(int id){SendMessage(app,WM_COMMAND,MAKEWPARAM(id,BN_CLICKED),(LPARAM)GetDlgItem(app,id));}
static void page(int index)
{SendMessage(GetDlgItem(app,100),LB_SETCURSEL,index,0);SendMessage(app,WM_COMMAND,MAKEWPARAM(100,LBN_SELCHANGE),(LPARAM)GetDlgItem(app,100));}
static int capture(const char *path)
{
    RECT r;HDC dc,memory;HBITMAP bitmap;HGDIOBJ old;BITMAPINFO info;BITMAPFILEHEADER header;
    void *pixels;FILE *file;DWORD size;int rc=1;
    GetClientRect(app,&r);dc=GetDC(app);memory=CreateCompatibleDC(dc);
    memset(&info,0,sizeof(info));info.bmiHeader.biSize=sizeof(info.bmiHeader);info.bmiHeader.biWidth=r.right;
    info.bmiHeader.biHeight=-r.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0);if(!bitmap){DeleteDC(memory);ReleaseDC(app,dc);return 1;}
    old=SelectObject(memory,bitmap);PrintWindow(app,memory,PW_CLIENTONLY);
    size=(DWORD)r.right*(DWORD)r.bottom*4;memset(&header,0,sizeof(header));header.bfType=0x4d42;
    header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);header.bfSize=header.bfOffBits+size;
    file=fopen(path,"wb");if(file){if(fwrite(&header,1,sizeof(header),file)==sizeof(header)&&
        fwrite(&info.bmiHeader,1,sizeof(info.bmiHeader),file)==sizeof(info.bmiHeader)&&fwrite(pixels,1,size,file)==size)rc=0;fclose(file);}
    SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(app,dc);return rc;
}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"GUI smoke test failed: %d\n",__LINE__);failed=1;goto cleanup;}}while(0)
int main(void)
{
    char temporary[MAX_PATH],directory[MAX_PATH]={0},path[MAX_PATH],exe[MAX_PATH],command[MAX_PATH+16];
    STARTUPINFOA start;PROCESS_INFORMATION process={0};FILE *file;DWORD exit_code;int failed=0,started=0;
    const char *files[]={"stnc-core.exe","config.json","wallet.key","identity.key","stnc-core.log"};size_t i;
    CHECK(GetTempPathA(sizeof(temporary),temporary)>0);
    CHECK(GetTempFileNameA(temporary,"stc",0,directory)!=0);CHECK(DeleteFileA(directory));CHECK(CreateDirectoryA(directory,NULL));
    snprintf(exe,sizeof(exe),"%s\\stnc-core.exe",directory);CHECK(CopyFileA("build\\stnc-core.exe",exe,TRUE));
    snprintf(path,sizeof(path),"%s\\config.json",directory);file=fopen(path,"wb");CHECK(file!=NULL);
    fputs("{\"peer\":\"127.0.0.1\",\"port\":1,\"root_peer\":\"127.0.0.1\",\"root_peer_port\":1,\"mining_enabled\":false,\"mining_backend\":\"automatic\",\"mining_cpu_limit_percent\":2,\"stratum_host\":\"127.0.0.1\",\"stratum_port\":1}",file);fclose(file);
    memset(&start,0,sizeof(start));memset(&process,0,sizeof(process));start.cb=sizeof(start);
    snprintf(command,sizeof(command),"\"%s\" gui",exe);
    CHECK(CreateProcessA(exe,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,directory,&start,&process));started=1;child_pid=process.dwProcessId;
    for(i=0;i<200&&!app;++i){EnumWindows(find_window,0);Sleep(50);}CHECK(app!=NULL);CHECK(wait_enabled(101)==0);
    CHECK(IsWindowEnabled(GetDlgItem(app,102)));CHECK(!IsWindowEnabled(GetDlgItem(app,104)));
    CHECK(!IsWindowEnabled(GetDlgItem(app,107)));CHECK(IsWindowEnabled(GetDlgItem(app,103)));
    page(2);click(103);CHECK(wait_enabled(101)==0);CHECK(!IsWindowEnabled(GetDlgItem(app,103)));
    page(1);click(102);CHECK(wait_enabled(101)==0);CHECK(!IsWindowEnabled(GetDlgItem(app,102)));
    CHECK(IsWindowEnabled(GetDlgItem(app,104))&&IsWindowEnabled(GetDlgItem(app,107)));
    page(8);click(107);CHECK(wait_enabled(101)==0);CHECK(GetExitCodeProcess(process.hProcess,&exit_code)&&exit_code==STILL_ACTIVE);
    click(108);CHECK(wait_enabled(101)==0);CHECK(GetExitCodeProcess(process.hProcess,&exit_code)&&exit_code==STILL_ACTIVE);
    for(i=0;i<9;++i){page((int)i);CHECK(IsWindow(app));}
    page(0);CHECK(capture("build\\gui-overview.bmp")==0);
    page(6);CHECK(capture("build\\gui-contracts.bmp")==0);
    /* Full log must be concurrently readable. */
    snprintf(path,sizeof(path),"%s\\stnc-core.log",directory);file=fopen(path,"rb");CHECK(file!=NULL);fclose(file);
cleanup:
    if(started){PostMessage(app,WM_CLOSE,0,0);if(WaitForSingleObject(process.hProcess,45000)!=WAIT_OBJECT_0){TerminateProcess(process.hProcess,1);failed=1;}
        CloseHandle(process.hThread);CloseHandle(process.hProcess);}
    if(directory[0]){for(i=0;i<sizeof(files)/sizeof(files[0]);++i){snprintf(path,sizeof(path),"%s\\%s",directory,files[i]);DeleteFileA(path);}RemoveDirectoryA(directory);}
    if(!failed)puts("STNC native GUI smoke tests passed.");return failed;
}
