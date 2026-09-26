#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "stnc_gui.h"
#include "stnc_client.h"

#define UPDATED (WM_APP+1)
#define STOPPED (WM_APP+2)

enum { NAV=100,REFRESH=101,CREATE_WALLET=102,CREATE_IDENTITY=103,SEND=104,DESTINATION=105,AMOUNT=106,
       MINING_ON=107,MINING_OFF=108,CPU=109,CPU_SAVE=110,COPY_ADDRESS=111 };
enum { M_HOME=2000,M_WALLET_ADDRESS,M_WALLET_IDENTITY,M_WALLET_CONTRACT,M_SEND,M_RECEIVE,
       M_CONTRACTS,M_ACTIVITY,M_MINING };
enum { PAGE_OVERVIEW,PAGE_WALLET_ADDRESS,PAGE_WALLET_IDENTITY,PAGE_WALLET_CONTRACT,
       PAGE_SEND,PAGE_RECEIVE,PAGE_CONTRACTS,PAGE_ACTIVITY,PAGE_MINING,PAGE_COUNT };

typedef struct control { HWND window; int page; } control;
static struct {
    HWND window,nav,title,body,result,sync_text,sync_bar;
    HWND overview[12];
    HFONT font,title_font,hero_font,section_font;
    control controls[40]; size_t count;
    CRITICAL_SECTION lock; HANDLE wake,thread; volatile LONG stopping;
    int ready,busy,failed,page; stnc_client_request *pending; stnc_client_snapshot snapshot;
    char message[4096];
} ui;

static HWND make_control(const char *klass,const char *text,DWORD style,int id,int page,int x,int y,int w,int h)
{
    HWND window=CreateWindowExA(strcmp(klass,"EDIT")==0?WS_EX_CLIENTEDGE:0,klass,text,
        WS_CHILD|WS_VISIBLE|style,x,y,w,h,ui.window,(HMENU)(INT_PTR)id,GetModuleHandle(NULL),NULL);
    SendMessage(window,WM_SETFONT,(WPARAM)ui.font,TRUE);
    if(page>=0&&ui.count<40){ui.controls[ui.count].window=window;ui.controls[ui.count].page=page;++ui.count;}
    return window;
}
static HWND field(int id){return GetDlgItem(ui.window,id);}
static HWND page_label(const char *text,int page,int x,int y,int w,int h){return make_control("STATIC",text,0,0,page,x,y,w,h);}
static HWND page_button(const char *text,int id,int page,int x,int y,int w){return make_control("BUTTON",text,WS_TABSTOP|BS_PUSHBUTTON,id,page,x,y,w,30);}
static HWND page_edit(int id,int page,int x,int y,int w,int h){return make_control("EDIT","",WS_TABSTOP|ES_AUTOHSCROLL,id,page,x,y,w,h);}

static HMENU build_menu(void)
{
    HMENU bar=CreateMenu(),wallet=CreatePopupMenu(),send=CreatePopupMenu(),tools=CreatePopupMenu(),settings=CreatePopupMenu();
    AppendMenuA(bar,MF_STRING,M_HOME,"Overview");
    AppendMenuA(wallet,MF_STRING,M_WALLET_ADDRESS,"Address");
    AppendMenuA(wallet,MF_STRING,M_WALLET_IDENTITY,"Identity");
    AppendMenuA(wallet,MF_STRING,M_WALLET_CONTRACT,"Contract");
    AppendMenuA(bar,MF_POPUP,(UINT_PTR)wallet,"Wallet");
    AppendMenuA(send,MF_STRING,M_SEND,"Send");
    AppendMenuA(send,MF_STRING,M_RECEIVE,"Receive");
    AppendMenuA(bar,MF_POPUP,(UINT_PTR)send,"Send / Receive");
    AppendMenuA(tools,MF_STRING,M_CONTRACTS,"Contracts");
    AppendMenuA(bar,MF_POPUP,(UINT_PTR)tools,"Tools");
    AppendMenuA(bar,MF_STRING,M_ACTIVITY,"Activity");
    AppendMenuA(settings,MF_STRING,M_MINING,"Mining");
    AppendMenuA(bar,MF_POPUP,(UINT_PTR)settings,"Settings");
    return bar;
}

static void center_window(HWND window,int width,int height)
{
    RECT work;int x,y;SystemParametersInfo(SPI_GETWORKAREA,0,&work,0);
    x=work.left+(work.right-work.left-width)/2;y=work.top+(work.bottom-work.top-height)/2;
    SetWindowPos(window,NULL,x,y,width,height,SWP_NOZORDER|SWP_NOACTIVATE);
}

static void layout(void)
{
    RECT r;size_t i;int width,height;
    GetClientRect(ui.window,&r);width=r.right;height=r.bottom;
    MoveWindow(ui.title,28,20,width-56,42,TRUE);
    MoveWindow(ui.body,28,76,width-56,height-142,TRUE);
    MoveWindow(ui.result,28,height-92,width-56,26,TRUE);
    MoveWindow(ui.sync_text,18,height-34,300,20,TRUE);
    MoveWindow(ui.sync_bar,326,height-32,width-344,16,TRUE);
    for(i=0;i<ui.count;++i)ShowWindow(ui.controls[i].window,ui.controls[i].page==ui.page?SW_SHOW:SW_HIDE);
}

static void set_page(int page)
{
    if(page<0||page>=PAGE_COUNT)return;ui.page=page;
    SendMessage(ui.nav,LB_SETCURSEL,page,0);layout();PostMessage(ui.window,UPDATED,0,0);
}

static void render(void)
{
    stnc_client_snapshot s;char text[8192],balance[64]="Unavailable",height[64]="Unavailable",actor[65]="",sync[256];
    int ready,busy,failed;size_t i;
    EnterCriticalSection(&ui.lock);s=ui.snapshot;ready=ui.ready;busy=ui.busy;failed=ui.failed;LeaveCriticalSection(&ui.lock);
    if(s.wallet.balance_available)snprintf(balance,sizeof(balance),"%" PRIu64 " STNC",s.wallet.accepted_balance);
    if(s.chain.available&&s.network.chain_connected)snprintf(height,sizeof(height),"%" PRIu64,s.chain.height);
    if(!ready){SetWindowTextA(ui.title,"STNC Core");SetWindowTextA(ui.body,failed?"Core initialization failed. Open Activity for details.":"Connecting to STN Chain...");}
    else switch(ui.page){
    case PAGE_OVERVIEW:
        SetWindowTextA(ui.title,"STN Chain");
        snprintf(text,sizeof(text),
            "Distributed trust for verifiable intelligence, signed releases, certifications, contracts, and permanent records.\r\n\r\n"
            "WALLET\r\n%s\r\n%s\r\n\r\n"
            "IDENTITY\r\n%s\r\n\r\n"
            "CONTRACTS & RECORDS\r\nVerifiable agreements and permanent Chain records\r\n\r\n"
            "NETWORK\r\n%s  |  Height %s\r\n\r\n"
            "MINING\r\n%s  |  %" PRIu64 " H/s",
            balance,s.wallet.key_valid?s.wallet.address:"Wallet not configured",
            s.identity.valid?s.identity.address:"Identity not configured",
            s.network.chain_connected?"Connected":"Disconnected",height,
            s.mining.running?"Running":"Idle",s.mining.hashrate_hps);
        SetWindowTextA(ui.body,text);break;
    case PAGE_WALLET_ADDRESS:
        SetWindowTextA(ui.title,"Wallet Address");
        snprintf(text,sizeof(text),"Your STNC economic wallet\r\n\r\nAddress\r\n%s\r\n\r\nAccepted balance\r\n%s\r\n\r\nPrivate key material is never displayed.",s.wallet.key_valid?s.wallet.address:"No wallet exists.",balance);SetWindowTextA(ui.body,text);break;
    case PAGE_WALLET_IDENTITY:
        SetWindowTextA(ui.title,"Identity");for(i=0;i<32&&s.identity.valid;++i)snprintf(actor+2*i,3,"%02x",(unsigned int)s.identity.public_key[i]);
        snprintf(text,sizeof(text),"Your STN Core identity\r\n\r\nStatus: %s\r\nIdentity: %s\r\nActor public key: %s\r\n\r\nIdentity is separate from the economic wallet and does not itself grant authority.",s.identity.valid?"Ready":s.identity.present?"Invalid":"Not configured",s.identity.valid?s.identity.address:"Unavailable",s.identity.valid?actor:"Unavailable");SetWindowTextA(ui.body,text);break;
    case PAGE_WALLET_CONTRACT:
        SetWindowTextA(ui.title,"Contract Identity");snprintf(text,sizeof(text),"Contract participation\r\n\r\nWallet: %s\r\nIdentity: %s\r\n\r\nContracts use Core identity and accepted authority evidence while the economic wallet remains separate.",s.wallet.key_valid?s.wallet.address:"Unavailable",s.identity.valid?s.identity.address:"Unavailable");SetWindowTextA(ui.body,text);break;
    case PAGE_SEND:
        SetWindowTextA(ui.title,"Send STNC");snprintf(text,sizeof(text),"Available: %s\r\n\r\nEnter the destination wallet and amount below. Core signs the transfer; STN Chain determines acceptance.",balance);SetWindowTextA(ui.body,text);break;
    case PAGE_RECEIVE:
        SetWindowTextA(ui.title,"Receive STNC");snprintf(text,sizeof(text),"Receive to this wallet\r\n\r\n%s\r\n\r\nAccepted balance: %s",s.wallet.key_valid?s.wallet.address:"Create a wallet first.",balance);SetWindowTextA(ui.body,text);break;
    case PAGE_CONTRACTS:
        SetWindowTextA(ui.title,"Contracts");SetWindowTextA(ui.body,"Create, inspect, and manage STN Chain Contracts.\r\n\r\nContract lifecycle actions remain bounded by accepted identity and authority evidence.\r\n\r\nAdditional Contract workflow controls will appear here as the corresponding Chain read interfaces become available.");break;
    case PAGE_ACTIVITY:
        SetWindowTextA(ui.title,"Activity");text[0]=0;for(i=0;i<s.activity_count&&i<5;++i){size_t used=strlen(text);snprintf(text+used,sizeof(text)-used,"%s%s",s.activity[i],i+1<s.activity_count?"\r\n\r\n":"");}if(!s.activity_count)snprintf(text,sizeof(text),"No recent activity.");SetWindowTextA(ui.body,text);break;
    default:
        SetWindowTextA(ui.title,"Mining");snprintf(text,sizeof(text),"Mining participation\r\n\r\nStatus: %s\r\nBackend: %s\r\nCPU limit: %u%%\r\nStratum: %s:%u (%s)\r\nHashrate: %" PRIu64 " H/s",
            s.mining.running?"Running":"Idle",s.mining.running?stnc_mining_backend_name(s.mining.active_backend):s.config.mining_backend,
            s.config.mining_cpu_limit_percent,s.config.stratum_host,(unsigned int)s.config.stratum_port,s.stratum_connected?"connected":"disconnected",s.mining.hashrate_hps);SetWindowTextA(ui.body,text);break;
    }
    if(!ready)snprintf(sync,sizeof(sync),"Starting Core...");else if(!s.network.chain_connected)snprintf(sync,sizeof(sync),"Disconnected");else if(s.network.peer_current)snprintf(sync,sizeof(sync),"Synchronized  |  Height %s",height);else snprintf(sync,sizeof(sync),"Synchronizing  |  Accepted height %s",height);
    SetWindowTextA(ui.sync_text,sync);ShowWindow(ui.sync_bar,ready&&s.network.chain_connected&&!s.network.peer_current?SW_SHOW:SW_HIDE);SendMessage(ui.sync_bar,PBM_SETMARQUEE,TRUE,0);
    EnableWindow(field(REFRESH),ready&&!busy&&!ui.stopping);EnableWindow(field(SEND),ready&&!busy&&s.wallet.key_valid&&!ui.stopping);EnableWindow(field(MINING_ON),ready&&!busy&&s.wallet.key_valid&&!ui.stopping);
    ShowWindow(field(CREATE_WALLET),ui.page==PAGE_WALLET_ADDRESS&&!s.wallet.present?SW_SHOW:SW_HIDE);
    ShowWindow(field(CREATE_IDENTITY),ui.page==PAGE_WALLET_IDENTITY&&!s.identity.present?SW_SHOW:SW_HIDE);
}

static DWORD WINAPI runtime(LPVOID unused)
{
    stnc_client_snapshot snapshot;uint64_t next=0;int initialized;(void)unused;
    initialized=stnc_core_init()==0;EnterCriticalSection(&ui.lock);ui.ready=initialized;ui.failed=!initialized;LeaveCriticalSection(&ui.lock);PostMessage(ui.window,UPDATED,0,0);
    while(initialized&&!InterlockedCompareExchange(&ui.stopping,0,0)){
        stnc_client_request *request=NULL;char message[4096];
        EnterCriticalSection(&ui.lock);request=ui.pending;ui.pending=NULL;LeaveCriticalSection(&ui.lock);
        if(request){stnc_client_execute(request,message,sizeof(message));free(request);EnterCriticalSection(&ui.lock);ui.busy=0;snprintf(ui.message,sizeof(ui.message),"%s",message);LeaveCriticalSection(&ui.lock);next=0;}
        if(stnc_core_tick()!=0)break;
        if(GetTickCount64()>=next){if(stnc_client_read(&snapshot)==0){EnterCriticalSection(&ui.lock);ui.snapshot=snapshot;LeaveCriticalSection(&ui.lock);}next=GetTickCount64()+2000;PostMessage(ui.window,UPDATED,0,0);}WaitForSingleObject(ui.wake,100);
    }
    if(initialized){stnc_core_request_stop();stnc_core_shutdown();}PostMessage(ui.window,STOPPED,0,0);return 0;
}

static void queue(stnc_client_request *request)
{
    EnterCriticalSection(&ui.lock);if(!ui.ready||ui.busy||ui.stopping){LeaveCriticalSection(&ui.lock);free(request);return;}ui.pending=request;ui.busy=1;LeaveCriticalSection(&ui.lock);SetEvent(ui.wake);render();
}
static void command(int id)
{
    stnc_client_request *request;uint64_t units;char amount[32],address[71],confirm[256];
    if(id==COPY_ADDRESS){stnc_client_snapshot s;EnterCriticalSection(&ui.lock);s=ui.snapshot;LeaveCriticalSection(&ui.lock);if(s.wallet.key_valid&&OpenClipboard(ui.window)){HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,strlen(s.wallet.address)+1);if(memory){char *p=(char *)GlobalLock(memory);memcpy(p,s.wallet.address,strlen(s.wallet.address)+1);GlobalUnlock(memory);EmptyClipboard();SetClipboardData(CF_TEXT,memory);}CloseClipboard();}return;}
    request=(stnc_client_request *)calloc(1,sizeof(*request));if(!request)return;
    switch(id){
    case REFRESH:request->operation=STNC_CLIENT_REFRESH;break;
    case CREATE_WALLET:request->operation=STNC_CLIENT_CREATE_WALLET;break;
    case CREATE_IDENTITY:request->operation=STNC_CLIENT_CREATE_IDENTITY;break;
    case MINING_ON:request->operation=STNC_CLIENT_MINING_ON;break;
    case MINING_OFF:request->operation=STNC_CLIENT_MINING_OFF;break;
    case CPU_SAVE:request->operation=STNC_CLIENT_CPU_LIMIT;request->cpu_limit=(unsigned int)SendMessage(field(CPU),CB_GETCURSEL,0,0)+1;break;
    case SEND:GetWindowTextA(field(DESTINATION),address,sizeof(address));GetWindowTextA(field(AMOUNT),amount,sizeof(amount));if(!stnc_client_wallet_address_valid(address)||stnc_client_parse_units(amount,&units)!=0){MessageBoxA(ui.window,"Enter a canonical stnw0_ destination and a positive whole-unit amount.","STNC Core",MB_OK|MB_ICONWARNING);free(request);return;}snprintf(confirm,sizeof(confirm),"Send %" PRIu64 " STNC to:\n%s",units,address);if(MessageBoxA(ui.window,confirm,"Confirm transfer",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)!=IDYES){free(request);return;}request->operation=STNC_CLIENT_SEND;request->confirmed=1;snprintf(request->address,sizeof(request->address),"%s",address);snprintf(request->units,sizeof(request->units),"%s",amount);break;
    default:free(request);return;
    }queue(request);
}

static LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam)
{
    switch(message){
    case WM_GETMINMAXINFO:{MINMAXINFO *info=(MINMAXINFO *)lparam;info->ptMinTrackSize.x=760;info->ptMinTrackSize.y=520;return 0;}
    case WM_SIZE:if(ui.title)layout();return 0;
    case WM_COMMAND:
        switch(LOWORD(wparam)){
        case M_HOME:set_page(PAGE_OVERVIEW);return 0;case M_WALLET_ADDRESS:set_page(PAGE_WALLET_ADDRESS);return 0;case M_WALLET_IDENTITY:set_page(PAGE_WALLET_IDENTITY);return 0;case M_WALLET_CONTRACT:set_page(PAGE_WALLET_CONTRACT);return 0;case M_SEND:set_page(PAGE_SEND);return 0;case M_RECEIVE:set_page(PAGE_RECEIVE);return 0;case M_CONTRACTS:set_page(PAGE_CONTRACTS);return 0;case M_ACTIVITY:set_page(PAGE_ACTIVITY);return 0;case M_MINING:set_page(PAGE_MINING);return 0;
        case NAV:if(HIWORD(wparam)==LBN_SELCHANGE)set_page((int)SendMessage(ui.nav,LB_GETCURSEL,0,0));return 0;
        default:if(HIWORD(wparam)==BN_CLICKED)command(LOWORD(wparam));return 0;}
    case UPDATED:render();return 0;
    case STOPPED:if(ui.stopping)DestroyWindow(window);else{EnterCriticalSection(&ui.lock);ui.ready=0;ui.failed=1;LeaveCriticalSection(&ui.lock);render();}return 0;
    case WM_CLOSE:InterlockedExchange(&ui.stopping,1);SetEvent(ui.wake);if(WaitForSingleObject(ui.thread,0)==WAIT_OBJECT_0)DestroyWindow(window);return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    default:return DefWindowProcA(window,message,wparam,lparam);}
}

int stnc_gui_run(void)
{
    WNDCLASSA cls;MSG msg;INITCOMMONCONTROLSEX common;int exit_code=0;const char *limits[]={"1%","2%"};
    FreeConsole();memset(&ui,0,sizeof(ui));InitializeCriticalSection(&ui.lock);ui.wake=CreateEvent(NULL,FALSE,FALSE,NULL);if(!ui.wake){DeleteCriticalSection(&ui.lock);return 1;}
    common.dwSize=sizeof(common);common.dwICC=ICC_PROGRESS_CLASS;InitCommonControlsEx(&common);
    ui.font=CreateFontA(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");
    ui.title_font=CreateFontA(-28,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");
    ui.hero_font=CreateFontA(-38,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");
    ui.section_font=CreateFontA(-17,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");
    memset(&cls,0,sizeof(cls));cls.lpfnWndProc=window_proc;cls.hInstance=GetModuleHandle(NULL);cls.lpszClassName="STNCCoreWindow";cls.hCursor=LoadCursor(NULL,IDC_ARROW);cls.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassA(&cls);
    ui.window=CreateWindowExA(WS_EX_CONTROLPARENT,cls.lpszClassName,"STNC Core - STN Chain",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,860,620,NULL,build_menu(),cls.hInstance,NULL);if(!ui.window){exit_code=1;goto cleanup;}
    center_window(ui.window,860,620);
    ui.nav=make_control("LISTBOX","",LBS_NOTIFY,NAV,-1,-100,-100,1,1);SendMessageA(ui.nav,LB_ADDSTRING,0,(LPARAM)"Overview");SendMessageA(ui.nav,LB_ADDSTRING,0,(LPARAM)"Address");SendMessageA(ui.nav,LB_ADDSTRING,0,(LPARAM)"Identity");SendMessageA(ui.nav,LB_ADDSTRING,0,(LPARAM)"Contract");SendMessageA(ui.nav,LB_ADDSTRING,0,(LPARAM)"Send");SendMessageA(ui.nav,LB_ADDSTRING,0,(LPARAM)"Receive");SendMessageA(ui.nav,LB_ADDSTRING,0,(LPARAM)"Contracts");SendMessageA(ui.nav,LB_ADDSTRING,0,(LPARAM)"Activity");SendMessageA(ui.nav,LB_ADDSTRING,0,(LPARAM)"Mining");SendMessage(ui.nav,LB_SETCURSEL,0,0);ShowWindow(ui.nav,SW_HIDE);
    ui.title=make_control("STATIC","STN Chain",0,0,-1,0,0,0,0);SendMessage(ui.title,WM_SETFONT,(WPARAM)ui.title_font,TRUE);
    ui.body=make_control("EDIT","Connecting to STN Chain...",ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,0,-1,0,0,0,0);
    ui.result=make_control("STATIC","",SS_LEFT,0,-1,0,0,0,0);ui.sync_text=make_control("STATIC","Starting Core...",SS_LEFT,0,-1,0,0,0,0);ui.sync_bar=make_control(PROGRESS_CLASSA,"",PBS_MARQUEE,0,-1,0,0,0,0);
    page_button("Create Wallet",CREATE_WALLET,PAGE_WALLET_ADDRESS,28,350,150);page_button("Create Identity",CREATE_IDENTITY,PAGE_WALLET_IDENTITY,28,350,160);
    page_label("Pay to",PAGE_SEND,28,205,120,22);page_edit(DESTINATION,PAGE_SEND,28,230,790,28);page_label("Amount",PAGE_SEND,28,274,120,22);page_edit(AMOUNT,PAGE_SEND,28,299,210,28);page_button("Send",SEND,PAGE_SEND,258,298,110);
    page_button("Copy Address",COPY_ADDRESS,PAGE_RECEIVE,28,260,140);
    page_button("Mining ON",MINING_ON,PAGE_MINING,28,300,130);page_button("Mining OFF",MINING_OFF,PAGE_MINING,172,300,130);
    {HWND combo=make_control("COMBOBOX","",CBS_DROPDOWNLIST,CPU,PAGE_MINING,320,300,80,120);SendMessageA(combo,CB_ADDSTRING,0,(LPARAM)limits[0]);SendMessageA(combo,CB_ADDSTRING,0,(LPARAM)limits[1]);SendMessage(combo,CB_SETCURSEL,1,0);}page_button("Save CPU limit",CPU_SAVE,PAGE_MINING,416,299,150);
    layout();render();ShowWindow(ui.window,SW_SHOW);UpdateWindow(ui.window);ui.thread=CreateThread(NULL,0,runtime,NULL,0,NULL);if(!ui.thread){exit_code=1;DestroyWindow(ui.window);goto cleanup;}
    while(GetMessage(&msg,NULL,0,0)>0){if(!IsDialogMessage(ui.window,&msg)){TranslateMessage(&msg);DispatchMessage(&msg);}}
    WaitForSingleObject(ui.thread,INFINITE);CloseHandle(ui.thread);
cleanup:
    free(ui.pending);CloseHandle(ui.wake);DeleteCriticalSection(&ui.lock);DeleteObject(ui.font);DeleteObject(ui.title_font);DeleteObject(ui.hero_font);DeleteObject(ui.section_font);return exit_code;
}
