#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
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
    PARTICIPANT,ROLE,ADD_PARTICIPANT,REMOVE_PARTICIPANT,PARTICIPANTS,TERMS,SAVE_DRAFT,
    COPY_ADDRESS };
enum { PAGE_OVERVIEW,PAGE_WALLET_ADDRESS,PAGE_WALLET_IDENTITY,PAGE_WALLET_CONTRACT,
    PAGE_SEND,PAGE_RECEIVE,PAGE_CONTRACTS,PAGE_ACTIVITY,PAGE_SETTINGS_MINING,PAGE_COUNT };
static const char *pages[]={"Overview","Wallet - Address","Wallet - Identity","Wallet - Contract","Send","Receive","Tools - Contracts","Activity","Settings - Mining"};
typedef struct control {HWND window;int page,id,x,y,w,h;} control;
static struct {
    HWND window,nav,title,status,result,refresh,sync_text,sync_bar;
    HFONT font,title_font,hero_font;
    control controls[48];size_t count;
    CRITICAL_SECTION lock;HANDLE wake,thread;volatile LONG stopping;
    int ready,busy,failed,page;stnc_client_request *pending;stnc_client_snapshot snapshot;
    char message[4096];stnc_contract_participant_input participants[32];size_t participant_count;
} ui;

static HWND widget(const char *class_name,const char *text,DWORD style,int id,int page,int x,int y,int w,int h)
{
    HWND window;
    window=CreateWindowExA(strcmp(class_name,"EDIT")==0?WS_EX_CLIENTEDGE:0,class_name,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,ui.window,(HMENU)(INT_PTR)id,GetModuleHandle(NULL),NULL);
    SendMessage(window,WM_SETFONT,(WPARAM)ui.font,TRUE);
    if(page>=0&&ui.count<48){control *c=&ui.controls[ui.count++];c->window=window;c->page=page;c->id=id;c->x=x;c->y=y;c->w=w;c->h=h;}
    return window;
}
static HWND field(int id){return GetDlgItem(ui.window,id);}
static void label(const char *text,int page,int x,int y,int w){widget("STATIC",text,0,0,page,x,y,w,22);}
static void edit(int id,int page,int x,int y,int w,int h,int multiline)
{HWND window=widget("EDIT","",WS_TABSTOP|(multiline?ES_MULTILINE|ES_WANTRETURN|ES_AUTOVSCROLL|WS_VSCROLL:ES_AUTOHSCROLL),id,page,x,y,w,h);SendMessage(window,EM_SETLIMITTEXT,id==TERMS?65536:id==AMOUNT?20:id==CREATED?20:70,0);}
static void button(const char *text,int id,int page,int x,int y,int w){widget("BUTTON",text,WS_TABSTOP|BS_PUSHBUTTON,id,page,x,y,w,30);}
static void combo(int id,int page,int x,int y,int w,const char *const *items,size_t count){size_t i;HWND window=widget("COMBOBOX","",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,id,page,x,y,w,180);for(i=0;i<count;++i)SendMessageA(window,CB_ADDSTRING,0,(LPARAM)items[i]);SendMessage(window,CB_SETCURSEL,0,0);}

static void layout(void)
{
    RECT r;size_t i;int width,height,footer=54;
    GetClientRect(ui.window,&r);width=r.right;height=r.bottom;
    MoveWindow(ui.nav,18,74,176,height-footer-92,TRUE);
    MoveWindow(ui.title,218,22,width-370,38,TRUE);MoveWindow(ui.refresh,width-128,22,104,30,TRUE);
    MoveWindow(ui.status,218,76,width-242,ui.page==PAGE_CONTRACTS?64:ui.page==PAGE_SEND||ui.page==PAGE_RECEIVE?100:height-footer-300,TRUE);
    MoveWindow(ui.result,218,height-footer-148,width-242,116,TRUE);
    MoveWindow(ui.sync_text,18,height-footer+10,300,24,TRUE);MoveWindow(ui.sync_bar,330,height-footer+12,width-354,18,TRUE);
    for(i=0;i<ui.count;++i){control *c=&ui.controls[i];ShowWindow(c->window,c->page==ui.page?SW_SHOW:SW_HIDE);MoveWindow(c->window,c->x,c->y,c->w<0?width-c->x-24:c->w,c->h,TRUE);}
}
static void render(void)
{
    stnc_client_snapshot s;char text[8192],balance[64]="unavailable",height[64]="unavailable",actor[65]="",message[4096],sync[256];
    int ready,busy,failed;size_t i;
    EnterCriticalSection(&ui.lock);s=ui.snapshot;ready=ui.ready;busy=ui.busy;failed=ui.failed;memcpy(message,ui.message,sizeof(message));LeaveCriticalSection(&ui.lock);
    SetWindowTextA(ui.title,pages[ui.page]);SetWindowTextA(ui.result,message);
    if(s.wallet.balance_available)snprintf(balance,sizeof(balance),"%" PRIu64 " STNC",s.wallet.accepted_balance);
    if(s.chain.available&&s.network.chain_connected)snprintf(height,sizeof(height),"%" PRIu64,s.chain.height);
    if(!ready)snprintf(text,sizeof(text),failed?"Core initialization failed. See Activity and stnc-core.log.":"Connecting to STN Chain and qualifying network state...");
    else switch(ui.page){
    case PAGE_OVERVIEW:
        snprintf(text,sizeof(text),"STN CHAIN\r\nDistributed trust for verifiable intelligence, signed releases, certifications, contracts, and permanent records.\r\n\r\nBALANCE\r\n%s\r\n\r\nWALLET\r\n%s\r\n%s\r\n\r\nCHAIN\r\n%s  |  Height %s  |  Peer %s\r\n\r\nMINING\r\n%s  |  %s  |  %" PRIu64 " H/s\r\n\r\nIDENTITY\r\n%s\r\n\r\nConsensus determines accepted state. Core presents evidence and accepted Chain results.",balance,
            s.wallet.key_valid?"Ready":"Wallet not configured",s.wallet.key_valid?s.wallet.address:"Create a wallet under Wallet - Address.",
            s.network.chain_connected?"Connected":"Disconnected",height,s.peer.connected?s.peer.host:"none",
            s.config.mining_enabled?"Enabled":"Disabled",s.mining.running?"Running":"Idle",s.mining.hashrate_hps,
            s.identity.valid?s.identity.address:"Not configured");break;
    case PAGE_WALLET_ADDRESS:
        snprintf(text,sizeof(text),"Wallet Address\r\n\r\n%s\r\n\r\nAccepted balance: %s\r\n\r\nThe wallet is the economic address used to hold and transfer STNC. Private key material is never displayed here.",s.wallet.key_valid?s.wallet.address:"No wallet exists.",balance);break;
    case PAGE_WALLET_IDENTITY:
        for(i=0;i<32&&s.identity.valid;++i)snprintf(actor+2*i,3,"%02x",(unsigned int)s.identity.public_key[i]);
        snprintf(text,sizeof(text),"Core Identity\r\n\r\nStatus: %s\r\nSTN identity: %s\r\nActor public key: %s\r\n\r\nIdentity is stored separately from the economic wallet. Creating an identity does not grant authority; authority requires accepted Chain evidence.",s.identity.valid?"Ready":s.identity.present?"Invalid stored identity":"Not configured",s.identity.valid?s.identity.address:"unavailable",s.identity.valid?actor:"unavailable");break;
    case PAGE_WALLET_CONTRACT:
        snprintf(text,sizeof(text),"Contract Participation\r\n\r\nWallet: %s\r\nIdentity: %s\r\n\r\nContracts use the Core identity and accepted authority evidence; the economic wallet remains separate. Contract creation and lifecycle tools are under Tools - Contracts.",s.wallet.key_valid?s.wallet.address:"unavailable",s.identity.valid?s.identity.address:"unavailable");break;
    case PAGE_SEND:snprintf(text,sizeof(text),"Send STNC\r\nAvailable: %s\r\n\r\nEnter a destination wallet and whole-unit amount. Core constructs and signs the transfer; Chain determines acceptance.",balance);break;
    case PAGE_RECEIVE:snprintf(text,sizeof(text),"Receive STNC\r\n\r\nGive the sender this wallet address:\r\n%s\r\n\r\nAccepted balance: %s",s.wallet.key_valid?s.wallet.address:"Create a wallet first.",balance);break;
    case PAGE_CONTRACTS:snprintf(text,sizeof(text),"Contract Tools\r\nLookup accepted Contract summary state or prepare a canonical local draft. Full participant/terms retrieval and authority-backed actions remain unavailable until Chain exposes the required read interfaces.");break;
    case PAGE_ACTIVITY:{size_t used=0;text[0]=0;for(i=0;i<s.activity_count&&i<5;++i){int n=snprintf(text+used,sizeof(text)-used,"%s\r\n\r\n",s.activity[i]);if(n<0||(size_t)n>=sizeof(text)-used)break;used+=(size_t)n;}if(!s.activity_count)snprintf(text,sizeof(text),"No recent activity. Full history is stored in stnc-core.log beside the executable.");break;}
    default:snprintf(text,sizeof(text),"Mining Settings\r\n\r\nEnabled: %s\r\nRunning: %s\r\nBackend: %s\r\nCPU limit: %u%%\r\nStratum: %s:%u (%s)\r\nHashrate: %" PRIu64 " H/s\r\n\r\nMining may only be enabled when a valid wallet exists.",s.config.mining_enabled?"yes":"no",s.mining.running?"yes":"no",s.mining.running?stnc_mining_backend_name(s.mining.active_backend):s.config.mining_backend,s.config.mining_cpu_limit_percent,s.config.stratum_host,(unsigned int)s.config.stratum_port,s.stratum_connected?"connected":"disconnected",s.mining.hashrate_hps);break;
    }
    SetWindowTextA(ui.status,text);
    if(!ready)snprintf(sync,sizeof(sync),"Starting Core...");else if(!s.network.chain_connected)snprintf(sync,sizeof(sync),"Disconnected from STN Chain");else if(s.network.peer_current)snprintf(sync,sizeof(sync),"Synchronized  |  Height %s",height);else snprintf(sync,sizeof(sync),"Synchronizing  |  Accepted height %s",height);
    SetWindowTextA(ui.sync_text,sync);ShowWindow(ui.sync_bar,ready&&s.network.chain_connected&&!s.network.peer_current?SW_SHOW:SW_HIDE);SendMessage(ui.sync_bar,PBM_SETMARQUEE,TRUE,0);
    EnableWindow(ui.refresh,ready&&!busy&&!ui.stopping);for(i=0;i<ui.count;++i)EnableWindow(ui.controls[i].window,ready&&!busy&&!ui.stopping);
    ShowWindow(field(CREATE_WALLET),ui.page==PAGE_WALLET_ADDRESS&&!s.wallet.present?SW_SHOW:SW_HIDE);
    ShowWindow(field(CREATE_IDENTITY),ui.page==PAGE_WALLET_IDENTITY&&!s.identity.present?SW_SHOW:SW_HIDE);
    EnableWindow(field(SEND),ready&&!busy&&s.wallet.key_valid&&!ui.stopping);EnableWindow(field(MINING_ON),ready&&!busy&&s.wallet.key_valid&&!ui.stopping);
}
static DWORD WINAPI runtime(LPVOID unused){stnc_client_snapshot snapshot;uint64_t next_refresh=0;int initialized;(void)unused;initialized=stnc_core_init()==0;EnterCriticalSection(&ui.lock);ui.ready=initialized;ui.failed=!initialized;snprintf(ui.message,sizeof(ui.message),initialized?"Core ready. Full history: stnc-core.log beside the executable.":"Initialization failed. Check configuration and application directory access.");LeaveCriticalSection(&ui.lock);PostMessage(ui.window,UPDATED,0,0);while(initialized&&!InterlockedCompareExchange(&ui.stopping,0,0)){stnc_client_request *request;char message[4096];EnterCriticalSection(&ui.lock);request=ui.pending;ui.pending=NULL;LeaveCriticalSection(&ui.lock);if(request){stnc_client_execute(request,message,sizeof(message));free(request);EnterCriticalSection(&ui.lock);memcpy(ui.message,message,sizeof(message));ui.busy=0;LeaveCriticalSection(&ui.lock);next_refresh=0;}if(stnc_core_tick()!=0)break;if(GetTickCount64()>=next_refresh){if(stnc_client_read(&snapshot)==0){EnterCriticalSection(&ui.lock);ui.snapshot=snapshot;LeaveCriticalSection(&ui.lock);}next_refresh=GetTickCount64()+2000;PostMessage(ui.window,UPDATED,0,0);}WaitForSingleObject(ui.wake,100);}if(initialized){stnc_core_request_stop();stnc_core_shutdown();}PostMessage(ui.window,STOPPED,0,0);return 0;}
static void queue(stnc_client_request *request){EnterCriticalSection(&ui.lock);if(!ui.ready||ui.busy||ui.stopping){LeaveCriticalSection(&ui.lock);free(request);return;}ui.pending=request;ui.busy=1;snprintf(ui.message,sizeof(ui.message),"Working...");LeaveCriticalSection(&ui.lock);SetEvent(ui.wake);render();}
static void show_error(const char *message){MessageBoxA(ui.window,message,"STNC Core",MB_OK|MB_ICONWARNING);}
static void command(int id)
{
    stnc_client_request *request;uint64_t units;char text[128];
    if(id==COPY_ADDRESS){stnc_client_snapshot s;EnterCriticalSection(&ui.lock);s=ui.snapshot;LeaveCriticalSection(&ui.lock);if(s.wallet.key_valid&&OpenClipboard(ui.window)){HGLOBAL mem;char *copy;EmptyClipboard();mem=GlobalAlloc(GMEM_MOVEABLE,strlen(s.wallet.address)+1);if(mem){copy=(char *)GlobalLock(mem);memcpy(copy,s.wallet.address,strlen(s.wallet.address)+1);GlobalUnlock(mem);SetClipboardData(CF_TEXT,mem);}CloseClipboard();}return;}
    if(id==ADD_PARTICIPANT){uint8_t decoded[32];stnc_contract_participant_input *p;if(ui.participant_count>=32){show_error("A Contract supports at most 32 participants.");return;}p=&ui.participants[ui.participant_count];GetWindowTextA(field(PARTICIPANT),p->public_key,sizeof(p->public_key));if(stnc_contract_actor_decode(p->public_key,decoded)!=0){show_error("Enter 64 lowercase hex characters from the participant Identity page.");return;}p->role=(uint16_t)(SendMessage(field(ROLE),CB_GETCURSEL,0,0)+1);snprintf(text,sizeof(text),"%s  [%u]",p->public_key,(unsigned int)p->role);SendMessageA(field(PARTICIPANTS),LB_ADDSTRING,0,(LPARAM)text);++ui.participant_count;return;}
    if(id==REMOVE_PARTICIPANT){LRESULT selected=SendMessage(field(PARTICIPANTS),LB_GETCURSEL,0,0);if(selected>=0&&(size_t)selected<ui.participant_count){size_t index=(size_t)selected;memmove(ui.participants+index,ui.participants+index+1,(ui.participant_count-index-1)*sizeof(ui.participants[0]));--ui.participant_count;SendMessage(field(PARTICIPANTS),LB_DELETESTRING,(WPARAM)selected,0);}return;}
    request=(stnc_client_request *)calloc(1,sizeof(*request));if(!request){show_error("Insufficient memory.");return;}
    switch(id){case REFRESH:request->operation=STNC_CLIENT_REFRESH;break;case CREATE_WALLET:request->operation=STNC_CLIENT_CREATE_WALLET;break;case CREATE_IDENTITY:request->operation=STNC_CLIENT_CREATE_IDENTITY;break;case MINING_ON:request->operation=STNC_CLIENT_MINING_ON;break;case MINING_OFF:request->operation=STNC_CLIENT_MINING_OFF;break;case CPU_SAVE:request->operation=STNC_CLIENT_CPU_LIMIT;request->cpu_limit=(unsigned int)SendMessage(field(CPU),CB_GETCURSEL,0,0)+1;break;case LOOKUP:request->operation=STNC_CLIENT_CONTRACT_LOOKUP;GetWindowTextA(field(CONTRACT_ADDRESS),request->address,sizeof(request->address));break;
    case SEND:{char confirmation[256];request->operation=STNC_CLIENT_SEND;GetWindowTextA(field(DESTINATION),request->address,sizeof(request->address));GetWindowTextA(field(AMOUNT),request->units,sizeof(request->units));if(!stnc_client_wallet_address_valid(request->address)||stnc_client_parse_units(request->units,&units)!=0){show_error("Enter a canonical stnw0_ destination and a positive whole-unit amount.");free(request);return;}snprintf(confirmation,sizeof(confirmation),"Send %" PRIu64 " STNC to:\n%s\n\nChain determines acceptance.",units,request->address);if(MessageBoxA(ui.window,confirmation,"Confirm transfer",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)!=IDYES){free(request);return;}request->confirmed=1;break;}
    case SAVE_DRAFT:{OPENFILENAMEA dialog;char path[1024]="";wchar_t *wide;int count,utf8;request->operation=STNC_CLIENT_SAVE_DRAFT;request->draft.type=(uint16_t)(SendMessage(field(TYPE),CB_GETCURSEL,0,0)+1);GetWindowTextA(field(CREATED),text,sizeof(text));if(strcmp(text,"0")==0)request->draft.created_at=0;else if(stnc_client_parse_units(text,&request->draft.created_at)!=0){show_error("Creation value must be an unsigned whole number.");free(request);return;}request->draft.participant_count=ui.participant_count;memcpy(request->draft.participants,ui.participants,sizeof(ui.participants));count=GetWindowTextLengthW(field(TERMS));wide=(wchar_t *)calloc((size_t)count+1,sizeof(wchar_t));if(!wide){free(request);return;}GetWindowTextW(field(TERMS),wide,count+1);utf8=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide,count,(char *)request->terms,sizeof(request->terms),NULL,NULL);free(wide);if(count&&utf8<=0){show_error("Terms must fit within 65,536 UTF-8 bytes.");free(request);return;}request->draft.terms_length=(size_t)utf8;memset(&dialog,0,sizeof(dialog));dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=ui.window;dialog.lpstrFilter="Canonical STCT draft (*.stct)\0*.stct\0All files\0*.*\0";dialog.lpstrFile=path;dialog.nMaxFile=sizeof(path);dialog.lpstrDefExt="stct";dialog.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;if(!GetSaveFileNameA(&dialog)){free(request);return;}memcpy(request->path,path,sizeof(path));break;}default:free(request);return;}queue(request);
}
static LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam){switch(message){case WM_GETMINMAXINFO:{MINMAXINFO *info=(MINMAXINFO *)lparam;info->ptMinTrackSize.x=980;info->ptMinTrackSize.y=720;return 0;}case WM_SIZE:if(ui.nav)layout();return 0;case WM_COMMAND:if(LOWORD(wparam)==NAV&&HIWORD(wparam)==LBN_SELCHANGE){ui.page=(int)SendMessage(ui.nav,LB_GETCURSEL,0,0);layout();render();return 0;}if(HIWORD(wparam)==BN_CLICKED)command(LOWORD(wparam));return 0;case UPDATED:render();return 0;case STOPPED:if(ui.stopping){DestroyWindow(window);return 0;}EnterCriticalSection(&ui.lock);ui.ready=0;ui.failed=1;LeaveCriticalSection(&ui.lock);render();return 0;case WM_CLOSE:InterlockedExchange(&ui.stopping,1);SetEvent(ui.wake);SetWindowTextA(window,"STNC Core - stopping services...");render();if(WaitForSingleObject(ui.thread,0)==WAIT_OBJECT_0)DestroyWindow(window);return 0;case WM_DESTROY:PostQuitMessage(0);return 0;default:return DefWindowProcA(window,message,wparam,lparam);}}
int stnc_gui_run(void)
{
    WNDCLASSA cls;MSG msg;size_t i;char created[32];int exit_code=0;INITCOMMONCONTROLSEX common;
    const char *types[]={"Generic","Work Offer","Contributor Agreement","Policy","Organizational Decision","Service Agreement"};const char *roles[]={"Participant","Issuer","Recipient","Approver","Attestor"};const char *limits[]={"1%","2%"};
    FreeConsole();common.dwSize=sizeof(common);common.dwICC=ICC_PROGRESS_CLASS;InitCommonControlsEx(&common);memset(&ui,0,sizeof(ui));InitializeCriticalSection(&ui.lock);ui.wake=CreateEvent(NULL,FALSE,FALSE,NULL);if(!ui.wake){DeleteCriticalSection(&ui.lock);return 1;}
    ui.font=CreateFontA(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");ui.title_font=CreateFontA(-27,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");ui.hero_font=CreateFontA(-34,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");
    memset(&cls,0,sizeof(cls));cls.lpfnWndProc=window_proc;cls.hInstance=GetModuleHandle(NULL);cls.lpszClassName="STNCCoreWindow";cls.hCursor=LoadCursor(NULL,IDC_ARROW);cls.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassA(&cls);ui.window=CreateWindowExA(WS_EX_CONTROLPARENT,cls.lpszClassName,"STNC Core - STN Chain",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1160,840,NULL,NULL,cls.hInstance,NULL);if(!ui.window){exit_code=1;goto cleanup;}
    ui.nav=widget("LISTBOX","",WS_TABSTOP|LBS_NOTIFY|WS_BORDER,NAV,-1,0,0,0,0);for(i=0;i<PAGE_COUNT;++i)SendMessageA(ui.nav,LB_ADDSTRING,0,(LPARAM)pages[i]);SendMessage(ui.nav,LB_SETCURSEL,0,0);
    ui.title=widget("STATIC","Overview",0,0,-1,0,0,0,0);SendMessage(ui.title,WM_SETFONT,(WPARAM)ui.title_font,TRUE);ui.refresh=widget("BUTTON","Refresh",WS_TABSTOP,REFRESH,-1,0,0,0,0);
    ui.status=widget("EDIT","",ES_MULTILINE|ES_READONLY|WS_VSCROLL|WS_TABSTOP,0,-1,0,0,0,0);ui.result=widget("EDIT","Starting Core...",ES_MULTILINE|ES_READONLY|WS_VSCROLL|WS_TABSTOP,0,-1,0,0,0,0);
    ui.sync_text=widget("STATIC","Starting Core...",0,0,-1,0,0,0,0);ui.sync_bar=widget(PROGRESS_CLASSA,"",PBS_MARQUEE,0,-1,0,0,0,0);
    button("Create Wallet",CREATE_WALLET,PAGE_WALLET_ADDRESS,230,300,160);button("Create Identity",CREATE_IDENTITY,PAGE_WALLET_IDENTITY,230,300,180);
    label("Pay to",PAGE_SEND,230,190,180);edit(DESTINATION,PAGE_SEND,230,216,-1,28,0);label("Amount",PAGE_SEND,230,258,180);edit(AMOUNT,PAGE_SEND,230,284,250,28,0);button("Review and Send",SEND,PAGE_SEND,230,330,180);
    button("Copy Address",COPY_ADDRESS,PAGE_RECEIVE,230,260,150);
    edit(CONTRACT_ADDRESS,PAGE_CONTRACTS,230,156,620,28,0);button("Look up",LOOKUP,PAGE_CONTRACTS,866,155,100);label("Local draft type",PAGE_CONTRACTS,230,202,280);combo(TYPE,PAGE_CONTRACTS,230,228,310,types,6);label("Creation value",PAGE_CONTRACTS,560,202,240);edit(CREATED,PAGE_CONTRACTS,560,228,250,28,0);snprintf(created,sizeof(created),"%" PRIu64,(uint64_t)time(NULL));SetWindowTextA(field(CREATED),created);label("Participant actor public key",PAGE_CONTRACTS,230,270,360);edit(PARTICIPANT,PAGE_CONTRACTS,230,296,570,28,0);combo(ROLE,PAGE_CONTRACTS,812,296,180,roles,5);button("Add participant",ADD_PARTICIPANT,PAGE_CONTRACTS,230,336,170);button("Remove selected",REMOVE_PARTICIPANT,PAGE_CONTRACTS,412,336,170);widget("LISTBOX","",WS_TABSTOP|WS_BORDER|WS_VSCROLL|LBS_NOTIFY,PARTICIPANTS,PAGE_CONTRACTS,230,376,-1,74);label("Terms",PAGE_CONTRACTS,230,458,200);edit(TERMS,PAGE_CONTRACTS,230,484,-1,90,1);button("Save canonical draft",SAVE_DRAFT,PAGE_CONTRACTS,230,588,210);
    button("Mining ON",MINING_ON,PAGE_SETTINGS_MINING,230,360,145);button("Mining OFF",MINING_OFF,PAGE_SETTINGS_MINING,390,360,145);combo(CPU,PAGE_SETTINGS_MINING,560,360,90,limits,2);button("Save CPU limit",CPU_SAVE,PAGE_SETTINGS_MINING,668,358,180);
    layout();render();ShowWindow(ui.window,SW_SHOW);UpdateWindow(ui.window);ui.thread=CreateThread(NULL,0,runtime,NULL,0,NULL);if(!ui.thread){DestroyWindow(ui.window);exit_code=1;goto cleanup;}while(GetMessage(&msg,NULL,0,0)>0){if(!IsDialogMessage(ui.window,&msg)){TranslateMessage(&msg);DispatchMessage(&msg);}}WaitForSingleObject(ui.thread,INFINITE);CloseHandle(ui.thread);
cleanup:free(ui.pending);CloseHandle(ui.wake);DeleteCriticalSection(&ui.lock);DeleteObject(ui.font);DeleteObject(ui.title_font);DeleteObject(ui.hero_font);return exit_code;
}
