#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include "stnc_gui.h"
#include "stnc_client.h"

#define UPDATED (WM_APP+1)
#define STOPPED (WM_APP+2)
enum { NAV=100,REFRESH,CREATE_WALLET,CREATE_IDENTITY,SEND,DESTINATION,AMOUNT,
    MINING_ON,MINING_OFF,CPU,CPU_SAVE,LOOKUP,CONTRACT_ADDRESS,TYPE,CREATED,
    PARTICIPANT,ROLE,ADD_PARTICIPANT,REMOVE_PARTICIPANT,PARTICIPANTS,TERMS,SAVE_DRAFT };
static const char *pages[]={"Overview","Wallet","Send","Contracts","Mining","Network","Activity","Identity"};
typedef struct control {HWND window;int page,id,x,y,w,h;} control;
static struct {
    HWND window,nav,title,status,result,refresh;
    HFONT font,title_font;
    control controls[40];size_t count;
    CRITICAL_SECTION lock;
    HANDLE wake,thread;
    volatile LONG stopping;
    int ready,busy,failed,page;
    stnc_client_request *pending;
    stnc_client_snapshot snapshot;
    char message[4096];
    stnc_contract_participant_input participants[32];size_t participant_count;
} ui;

static HWND widget(const char *class_name,const char *text,DWORD style,int id,int page,int x,int y,int w,int h)
{
    HWND control_window;
    if(strcmp(class_name,"EDIT")==0)
        control_window=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|style,x,y,w,h,
            ui.window,(HMENU)(INT_PTR)id,GetModuleHandle(NULL),NULL);
    else control_window=CreateWindowExA(0,
        class_name,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,ui.window,(HMENU)(INT_PTR)id,GetModuleHandle(NULL),NULL);
    SendMessage(control_window,WM_SETFONT,(WPARAM)ui.font,TRUE);
    if(page>=0&&ui.count<40){control *c=&ui.controls[ui.count++];c->window=control_window;c->page=page;c->id=id;c->x=x;c->y=y;c->w=w;c->h=h;}
    return control_window;
}
static HWND field(int id){return GetDlgItem(ui.window,id);}
static void label(const char *text,int page,int x,int y,int w)
{widget("STATIC",text,0,0,page,x,y,w,22);}
static void edit(int id,int page,int x,int y,int w,int h,int multiline)
{
    HWND window=widget("EDIT","",WS_TABSTOP|(multiline?ES_MULTILINE|ES_WANTRETURN|ES_AUTOVSCROLL|WS_VSCROLL:ES_AUTOHSCROLL),id,page,x,y,w,h);
    SendMessage(window,EM_SETLIMITTEXT,id==TERMS?65536:id==AMOUNT?20:id==CREATED?20:70,0);
}
static void button(const char *text,int id,int page,int x,int y,int w)
{widget("BUTTON",text,WS_TABSTOP|BS_PUSHBUTTON,id,page,x,y,w,30);}
static void combo(int id,int page,int x,int y,int w,const char *const *items,size_t count)
{
    size_t i;HWND window=widget("COMBOBOX","",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,id,page,x,y,w,180);
    for(i=0;i<count;++i)SendMessageA(window,CB_ADDSTRING,0,(LPARAM)items[i]);
    SendMessage(window,CB_SETCURSEL,0,0);
}
static void layout(void)
{
    RECT r;size_t i;int width,height;
    GetClientRect(ui.window,&r);width=r.right;height=r.bottom;
    MoveWindow(ui.nav,16,24,152,height-48,TRUE);
    MoveWindow(ui.title,190,20,width-340,36,TRUE);
    MoveWindow(ui.refresh,width-134,20,112,30,TRUE);
    MoveWindow(ui.status,190,66,width-212,ui.page==3?58:ui.page==2?80:height-270,TRUE);
    MoveWindow(ui.result,190,height-182,width-212,160,TRUE);
    for(i=0;i<ui.count;++i){control *c=&ui.controls[i];
        ShowWindow(c->window,c->page==ui.page?SW_SHOW:SW_HIDE);
        MoveWindow(c->window,c->x,c->y,c->w<0?width-c->x-22:c->w,c->h,TRUE);
    }
}
static void render(void)
{
    stnc_client_snapshot s;char text[8192],balance[64]="unavailable",height[64]="unavailable",actor[65];
    int ready,busy,failed;size_t i;char message[4096];
    EnterCriticalSection(&ui.lock);s=ui.snapshot;ready=ui.ready;busy=ui.busy;failed=ui.failed;
    memcpy(message,ui.message,sizeof(message));LeaveCriticalSection(&ui.lock);
    SetWindowTextA(ui.title,pages[ui.page]);SetWindowTextA(ui.result,message);
    if(s.wallet.balance_available)snprintf(balance,sizeof(balance),"%" PRIu64 " units",s.wallet.accepted_balance);
    if(s.chain.available&&s.network.chain_connected)snprintf(height,sizeof(height),"%" PRIu64,s.chain.height);
    if(!ready)snprintf(text,sizeof(text),failed?"Core initialization failed. See the result below and the log file.":"Starting Core services and qualifying network connections...");
    else switch(ui.page){
    case 0:
        snprintf(text,sizeof(text),"CHAIN\r\nConnection: %s\r\nAccepted height: %s\r\nSynchronization: %s\r\nSelected peer: %s\r\n\r\nWALLET\r\nStatus: %s\r\nAccepted balance: %s\r\n\r\nMINING\r\nPreference: %s\r\nRuntime: %s\r\n\r\nIDENTITY\r\n%s\r\n\r\nChain determines accepted state. Network and mining observations are evidence.",
            s.network.chain_connected?"connected":"disconnected",height,
            s.network.peer_current?"last peer comparison matches accepted Chain tip":
                s.network.chain_state_available?"peer comparison pending or diverged; see Activity":"accepted state unavailable",
            s.peer.connected?s.peer.host:"none",s.wallet.key_valid?"valid":s.wallet.present?"invalid stored wallet":"no wallet",
            balance,s.config.mining_enabled?"ON":"OFF",s.mining.running?"running":"idle",
            s.identity.valid?s.identity.address:"No valid Core identity");break;
    case 1:
        snprintf(text,sizeof(text),"Wallet status: %s\r\n\r\nAddress:\r\n%s\r\n\r\nAccepted balance: %s\r\n\r\nA wallet is required for transfers and background mining.",
            s.wallet.key_valid?"valid":s.wallet.present?"invalid (creation is disabled)":"no wallet exists",s.wallet.key_valid?s.wallet.address:"unavailable",balance);break;
    case 2:snprintf(text,sizeof(text),"Accepted balance: %s\r\nEnter a destination and whole units. Review the confirmation before submitting.",balance);break;
    case 3:snprintf(text,sizeof(text),"Drafts are local evidence. Lookup reports accepted summary state.\r\nSigning/submission actions unavailable: accepted authority and full Contract detail interfaces pending.");break;
    case 4:snprintf(text,sizeof(text),"Mining preference: %s\r\nRunning: %s\r\nConfigured backend: %s\r\nActive backend: %s\r\nCPU limit: %u%%\r\nStratum: %s:%u (%s)\r\nHashrate: %" PRIu64 " H/s\r\nWork passes: %" PRIu64 "\r\nHash attempts: %" PRIu64 "\r\nSolutions found: %" PRIu64 "\r\n\r\nAccepted participation totals: unavailable from current Core interfaces.\r\nCPU fallback is supported; GPU/USB backends are not implemented.",
            s.config.mining_enabled?"ON":"OFF",s.mining.running?"yes":"no",s.config.mining_backend,
            s.mining.running?stnc_mining_backend_name(s.mining.active_backend):"none",s.config.mining_cpu_limit_percent,
            s.config.stratum_host,(unsigned int)s.config.stratum_port,s.stratum_connected?"connected":"disconnected",
            s.mining.hashrate_hps,s.mining.passes,s.mining.attempts,s.mining.solutions);break;
    case 5:snprintf(text,sizeof(text),"Chain endpoint: %s:%u\r\nRPC: %s\r\nAccepted height: %s\r\nP2P: %s\r\nSelected peer: %s:%u\r\nPeer latency: %" PRIu64 " ms\r\nCandidates: %zu\r\nQualified peers: %zu\r\nRoot capabilities: 0x%08x\r\n\r\nPeer observations do not independently determine accepted state.",
            s.config.peer,(unsigned int)s.config.port,s.network.chain_connected?"connected":"disconnected",height,
            s.network.p2p_connected?"connected":"disconnected",s.peer.connected?s.peer.host:"none",(unsigned int)s.peer.port,
            s.peer.latency_ms,s.network.candidate_count,s.network.qualified_count,(unsigned int)s.network.root_peer_capabilities);break;
    case 6:{size_t used=0;text[0]=0;for(i=0;i<s.activity_count&&i<5;++i){int n=snprintf(text+used,sizeof(text)-used,"%s\r\n\r\n",s.activity[i]);if(n<0||(size_t)n>=sizeof(text)-used)break;used+=(size_t)n;}
        if(!s.activity_count)snprintf(text,sizeof(text),"No recent activity. Full history is stored in stnc-core.log beside the executable.");break;}
    default:
        for(i=0;i<32;++i)snprintf(actor+2*i,3,"%02x",(unsigned int)s.identity.public_key[i]);
        snprintf(text,sizeof(text),"Identity status: %s\r\n\r\nSTN identity address:\r\n%s\r\n\r\nActor public key:\r\n%s\r\n\r\nStored separately from the economic wallet.\r\nAuthority requires a scoped grant accepted by Chain; creating an identity grants no authority.",
            s.identity.valid?"valid":s.identity.present?"invalid (creation is disabled)":"not created",
            s.identity.valid?s.identity.address:"unavailable",s.identity.valid?actor:"unavailable");break;
    }
    SetWindowTextA(ui.status,text);
    EnableWindow(ui.refresh,ready&&!busy&&!ui.stopping);
    for(i=0;i<ui.count;++i)EnableWindow(ui.controls[i].window,ready&&!busy&&!ui.stopping);
    EnableWindow(field(CREATE_WALLET),ready&&!busy&&!s.wallet.present&&!ui.stopping);
    EnableWindow(field(CREATE_IDENTITY),ready&&!busy&&!s.identity.present&&!ui.stopping);
    EnableWindow(field(SEND),ready&&!busy&&s.wallet.key_valid&&!ui.stopping);
    EnableWindow(field(MINING_ON),ready&&!busy&&s.wallet.key_valid&&!ui.stopping);
}
static DWORD WINAPI runtime(LPVOID unused)
{
    stnc_client_snapshot snapshot;uint64_t next_refresh=0;int initialized;(void)unused;
    initialized=stnc_core_init()==0;
    EnterCriticalSection(&ui.lock);ui.ready=initialized;ui.failed=!initialized;
    snprintf(ui.message,sizeof(ui.message),initialized?"Core ready. Full history: stnc-core.log beside the executable.":"Initialization failed. Check configuration and access to the application directory.");
    LeaveCriticalSection(&ui.lock);PostMessage(ui.window,UPDATED,0,0);
    while(initialized&&!InterlockedCompareExchange(&ui.stopping,0,0)){
        stnc_client_request *request;char message[4096];
        EnterCriticalSection(&ui.lock);request=ui.pending;ui.pending=NULL;LeaveCriticalSection(&ui.lock);
        if(request!=NULL){
            stnc_client_execute(request,message,sizeof(message));free(request);
            EnterCriticalSection(&ui.lock);memcpy(ui.message,message,sizeof(message));ui.busy=0;LeaveCriticalSection(&ui.lock);
            next_refresh=0;
        }
        if(stnc_core_tick()!=0)break;
        if(GetTickCount64()>=next_refresh){
            if(stnc_client_read(&snapshot)==0){EnterCriticalSection(&ui.lock);ui.snapshot=snapshot;LeaveCriticalSection(&ui.lock);}
            next_refresh=GetTickCount64()+2000;PostMessage(ui.window,UPDATED,0,0);
        }
        WaitForSingleObject(ui.wake,100);
    }
    if(initialized){stnc_core_request_stop();stnc_core_shutdown();}
    PostMessage(ui.window,STOPPED,0,0);return 0;
}
static void queue(stnc_client_request *request)
{
    EnterCriticalSection(&ui.lock);
    if(!ui.ready||ui.busy||ui.stopping){LeaveCriticalSection(&ui.lock);free(request);return;}
    ui.pending=request;ui.busy=1;snprintf(ui.message,sizeof(ui.message),"Working...");
    LeaveCriticalSection(&ui.lock);SetEvent(ui.wake);render();
}
static void show_error(const char *message)
{MessageBoxA(ui.window,message,"STNC Core",MB_OK|MB_ICONWARNING);}
static void command(int id)
{
    stnc_client_request *request;uint64_t units;char text[128];
    if(id==ADD_PARTICIPANT){
        uint8_t decoded[32];stnc_contract_participant_input *p;
        if(ui.participant_count>=32){show_error("A Contract supports at most 32 participants.");return;}
        p=&ui.participants[ui.participant_count];GetWindowTextA(field(PARTICIPANT),p->public_key,sizeof(p->public_key));
        if(stnc_contract_actor_decode(p->public_key,decoded)!=0){show_error("Enter the participant actor public key: 64 lowercase hex characters from their Identity page.");return;}
        p->role=(uint16_t)(SendMessage(field(ROLE),CB_GETCURSEL,0,0)+1);
        snprintf(text,sizeof(text),"%s  [%u]",p->public_key,(unsigned int)p->role);
        SendMessageA(field(PARTICIPANTS),LB_ADDSTRING,0,(LPARAM)text);++ui.participant_count;return;
    }
    if(id==REMOVE_PARTICIPANT){
        LRESULT selected=SendMessage(field(PARTICIPANTS),LB_GETCURSEL,0,0);
        if(selected>=0&&(size_t)selected<ui.participant_count){
            size_t index=(size_t)selected;memmove(ui.participants+index,ui.participants+index+1,(ui.participant_count-index-1)*sizeof(ui.participants[0]));
            --ui.participant_count;SendMessage(field(PARTICIPANTS),LB_DELETESTRING,(WPARAM)selected,0);
        }return;
    }
    request=(stnc_client_request *)calloc(1,sizeof(*request));if(request==NULL){show_error("Insufficient memory.");return;}
    switch(id){
    case REFRESH:request->operation=STNC_CLIENT_REFRESH;break;
    case CREATE_WALLET:request->operation=STNC_CLIENT_CREATE_WALLET;break;
    case CREATE_IDENTITY:request->operation=STNC_CLIENT_CREATE_IDENTITY;break;
    case MINING_ON:request->operation=STNC_CLIENT_MINING_ON;break;
    case MINING_OFF:request->operation=STNC_CLIENT_MINING_OFF;break;
    case CPU_SAVE:request->operation=STNC_CLIENT_CPU_LIMIT;request->cpu_limit=(unsigned int)SendMessage(field(CPU),CB_GETCURSEL,0,0)+1;break;
    case LOOKUP:request->operation=STNC_CLIENT_CONTRACT_LOOKUP;GetWindowTextA(field(CONTRACT_ADDRESS),request->address,sizeof(request->address));break;
    case SEND:{char confirmation[256];
        request->operation=STNC_CLIENT_SEND;GetWindowTextA(field(DESTINATION),request->address,sizeof(request->address));
        GetWindowTextA(field(AMOUNT),request->units,sizeof(request->units));
        if(!stnc_client_wallet_address_valid(request->address)||stnc_client_parse_units(request->units,&units)!=0){show_error("Enter a canonical stnw0_ destination and a positive whole-unit amount.");free(request);return;}
        snprintf(confirmation,sizeof(confirmation),"Submit a transfer of %" PRIu64 " units to:\n%s\n\nChain determines acceptance.",units,request->address);
        if(MessageBoxA(ui.window,confirmation,"Confirm transfer",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)!=IDYES){free(request);return;}
        request->confirmed=1;break;}
    case SAVE_DRAFT:{
        OPENFILENAMEA dialog;char path[1024]="";wchar_t *wide;int count,utf8;
        request->operation=STNC_CLIENT_SAVE_DRAFT;request->draft.type=(uint16_t)(SendMessage(field(TYPE),CB_GETCURSEL,0,0)+1);
        GetWindowTextA(field(CREATED),text,sizeof(text));
        if(strcmp(text,"0")==0)request->draft.created_at=0;
        else if(stnc_client_parse_units(text,&request->draft.created_at)!=0){show_error("Creation value must be an unsigned whole number.");free(request);return;}
        request->draft.participant_count=ui.participant_count;
        memcpy(request->draft.participants,ui.participants,sizeof(ui.participants));
        count=GetWindowTextLengthW(field(TERMS));wide=(wchar_t *)calloc((size_t)count+1,sizeof(wchar_t));
        if(wide==NULL){free(request);return;}
        GetWindowTextW(field(TERMS),wide,count+1);
        utf8=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide,count,(char *)request->terms,sizeof(request->terms),NULL,NULL);free(wide);
        if(count&&utf8<=0){show_error("Terms must fit within 65,536 UTF-8 bytes.");free(request);return;}
        request->draft.terms_length=(size_t)utf8;
        memset(&dialog,0,sizeof(dialog));dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=ui.window;
        dialog.lpstrFilter="Canonical STCT draft (*.stct)\0*.stct\0All files\0*.*\0";dialog.lpstrFile=path;dialog.nMaxFile=sizeof(path);
        dialog.lpstrDefExt="stct";dialog.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
        if(!GetSaveFileNameA(&dialog)){free(request);return;}
        memcpy(request->path,path,sizeof(path));break;}
    default:free(request);return;
    }
    queue(request);
}
static LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam)
{
    switch(message){
    case WM_GETMINMAXINFO:{MINMAXINFO *info=(MINMAXINFO *)lparam;info->ptMinTrackSize.x=1050;info->ptMinTrackSize.y=850;return 0;}
    case WM_SIZE:if(ui.nav)layout();return 0;
    case WM_COMMAND:
        if(LOWORD(wparam)==NAV&&HIWORD(wparam)==LBN_SELCHANGE){ui.page=(int)SendMessage(ui.nav,LB_GETCURSEL,0,0);layout();render();return 0;}
        if(HIWORD(wparam)==BN_CLICKED)command(LOWORD(wparam));return 0;
    case UPDATED:render();return 0;
    case STOPPED:
        if(ui.stopping){DestroyWindow(window);return 0;}
        EnterCriticalSection(&ui.lock);ui.ready=0;ui.failed=1;LeaveCriticalSection(&ui.lock);render();return 0;
    case WM_CLOSE:
        InterlockedExchange(&ui.stopping,1);SetEvent(ui.wake);
        SetWindowTextA(window,"STNC Core - stopping services...");render();
        if(WaitForSingleObject(ui.thread,0)==WAIT_OBJECT_0)DestroyWindow(window);return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    default:return DefWindowProcA(window,message,wparam,lparam);
    }
}
int stnc_gui_run(void)
{
    WNDCLASSA cls;MSG msg;size_t i;char created[32];int exit_code=0;
    const char *types[]={"Generic","Work Offer","Contributor Agreement","Policy","Organizational Decision","Service Agreement"};
    const char *roles[]={"Participant","Issuer","Recipient","Approver","Attestor"};
    const char *limits[]={"1%","2%"};
    FreeConsole();
    memset(&ui,0,sizeof(ui));InitializeCriticalSection(&ui.lock);
    ui.wake=CreateEvent(NULL,FALSE,FALSE,NULL);if(ui.wake==NULL){DeleteCriticalSection(&ui.lock);return 1;}
    ui.font=CreateFontA(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");
    ui.title_font=CreateFontA(-25,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");
    memset(&cls,0,sizeof(cls));cls.lpfnWndProc=window_proc;cls.hInstance=GetModuleHandle(NULL);cls.lpszClassName="STNCCoreWindow";
    cls.hCursor=LoadCursor(NULL,IDC_ARROW);cls.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassA(&cls);
    ui.window=CreateWindowExA(WS_EX_CONTROLPARENT,cls.lpszClassName,"STNC Core",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1120,920,NULL,NULL,cls.hInstance,NULL);
    if(ui.window==NULL){exit_code=1;goto cleanup;}
    ui.nav=widget("LISTBOX","",WS_TABSTOP|LBS_NOTIFY|WS_BORDER,NAV,-1,0,0,0,0);
    for(i=0;i<8;++i)SendMessageA(ui.nav,LB_ADDSTRING,0,(LPARAM)pages[i]);SendMessage(ui.nav,LB_SETCURSEL,0,0);
    ui.title=widget("STATIC","Overview",0,0,-1,0,0,0,0);SendMessage(ui.title,WM_SETFONT,(WPARAM)ui.title_font,TRUE);
    ui.refresh=widget("BUTTON","Refresh",WS_TABSTOP,REFRESH,-1,0,0,0,0);
    ui.status=widget("EDIT","",ES_MULTILINE|ES_READONLY|WS_VSCROLL|WS_TABSTOP,0,-1,0,0,0,0);
    ui.result=widget("EDIT","Starting Core...",ES_MULTILINE|ES_READONLY|WS_VSCROLL|WS_TABSTOP,0,-1,0,0,0,0);
    button("Create Wallet",CREATE_WALLET,1,202,350,160);
    label("Destination wallet",2,190,164,300);edit(DESTINATION,2,190,190,-1,28,0);
    label("Amount (whole units)",2,190,234,300);edit(AMOUNT,2,190,260,260,28,0);button("Review and send",SEND,2,190,310,180);
    edit(CONTRACT_ADDRESS,3,190,136,660,28,0);button("Look up",LOOKUP,3,862,135,100);
    label("Local draft type",3,190,182,280);combo(TYPE,3,190,208,310,types,6);
    label("Creation value (Unix seconds)",3,530,182,340);edit(CREATED,3,530,208,300,28,0);
    snprintf(created,sizeof(created),"%" PRIu64,(uint64_t)time(NULL));SetWindowTextA(field(CREATED),created);
    label("Participant actor public key (Identity page)",3,190,248,360);edit(PARTICIPANT,3,190,274,610,28,0);
    combo(ROLE,3,812,274,180,roles,5);button("Add participant",ADD_PARTICIPANT,3,190,314,170);button("Remove selected",REMOVE_PARTICIPANT,3,372,314,170);
    widget("LISTBOX","",WS_TABSTOP|WS_BORDER|WS_VSCROLL|LBS_NOTIFY,PARTICIPANTS,3,190,354,-1,78);
    label("Terms (UTF-8, up to 65,536 bytes)",3,190,442,440);edit(TERMS,3,190,468,-1,100,1);
    button("Save canonical draft",SAVE_DRAFT,3,190,582,210);
    button("Mining ON",MINING_ON,4,202,450,145);button("Mining OFF",MINING_OFF,4,362,450,145);
    combo(CPU,4,530,450,90,limits,2);button("Save CPU limit",CPU_SAVE,4,638,448,180);
    button("Create Identity",CREATE_IDENTITY,7,202,360,180);
    layout();render();ShowWindow(ui.window,SW_SHOW);UpdateWindow(ui.window);
    ui.thread=CreateThread(NULL,0,runtime,NULL,0,NULL);
    if(ui.thread==NULL){DestroyWindow(ui.window);exit_code=1;goto cleanup;}
    while(GetMessage(&msg,NULL,0,0)>0){if(!IsDialogMessage(ui.window,&msg)){TranslateMessage(&msg);DispatchMessage(&msg);}}
    WaitForSingleObject(ui.thread,INFINITE);CloseHandle(ui.thread);
cleanup:
    free(ui.pending);CloseHandle(ui.wake);DeleteCriticalSection(&ui.lock);DeleteObject(ui.font);DeleteObject(ui.title_font);return exit_code;
}
