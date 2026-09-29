/* Windows STNC Core frontend. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "stnc_profile.h"

#define STNC_MENU_NEW_WALLET 2100
#define STNC_MENU_PROFILE_BASE 2200
#define STNC_INPUT_OK 3100
#define STNC_INPUT_CANCEL 3101

static HMENU stnc_profile_menu;
static char stnc_profile_names[STNC_PROFILE_LIST_MAX][STNC_PROFILE_NAME_MAX+1u];
static size_t stnc_profile_name_count;
static WNDPROC stnc_base_window_proc;
static char stnc_contract_identity[70];
static char stnc_contract_seen[STNC_CONTRACT_LIST_MAX][STNC_CONTRACT_LIST_ADDRESS_SIZE+1u];
static size_t stnc_contract_seen_count;
static int stnc_contract_seen_ready;

typedef struct stnc_input_state { HWND window; HWND edit; int done; int accepted; char value[STNC_PROFILE_NAME_MAX+1u]; } stnc_input_state;
static stnc_input_state *stnc_input_current;

static LRESULT CALLBACK stnc_input_proc(HWND w,UINT m,WPARAM wp,LPARAM lp)
{
    (void)lp;
    if(m==WM_COMMAND){
        if(LOWORD(wp)==STNC_INPUT_OK){
            if(stnc_input_current!=NULL){GetWindowTextA(stnc_input_current->edit,stnc_input_current->value,sizeof(stnc_input_current->value));stnc_input_current->accepted=1;stnc_input_current->done=1;}
            DestroyWindow(w);return 0;
        }
        if(LOWORD(wp)==STNC_INPUT_CANCEL){if(stnc_input_current!=NULL)stnc_input_current->done=1;DestroyWindow(w);return 0;}
    }
    if(m==WM_CLOSE){if(stnc_input_current!=NULL)stnc_input_current->done=1;DestroyWindow(w);return 0;}
    return DefWindowProcA(w,m,wp,lp);
}

static int stnc_prompt_name(HWND parent,const char *title,char value[STNC_PROFILE_NAME_MAX+1u])
{
    static int registered;WNDCLASSA c;stnc_input_state state;MSG msg;RECT r;HWND label,ok,cancel;HFONT font;
    memset(&state,0,sizeof(state));memset(&c,0,sizeof(c));
    if(!registered){c.lpfnWndProc=stnc_input_proc;c.hInstance=GetModuleHandleA(NULL);c.lpszClassName="STNCWalletNameDialog";c.hCursor=LoadCursor(NULL,IDC_ARROW);c.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);if(!RegisterClassA(&c)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return 0;registered=1;}
    state.window=CreateWindowExA(WS_EX_DLGMODALFRAME,"STNCWalletNameDialog",title,WS_POPUP|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,390,155,parent,NULL,GetModuleHandleA(NULL),NULL);if(state.window==NULL)return 0;
    font=(HFONT)GetStockObject(DEFAULT_GUI_FONT);label=CreateWindowExA(0,"STATIC","Wallet name",WS_CHILD|WS_VISIBLE,18,18,340,20,state.window,NULL,GetModuleHandleA(NULL),NULL);state.edit=CreateWindowExA(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,18,42,345,24,state.window,NULL,GetModuleHandleA(NULL),NULL);ok=CreateWindowExA(0,"BUTTON","OK",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,197,80,80,28,state.window,(HMENU)(INT_PTR)STNC_INPUT_OK,GetModuleHandleA(NULL),NULL);cancel=CreateWindowExA(0,"BUTTON","Cancel",WS_CHILD|WS_VISIBLE|WS_TABSTOP,283,80,80,28,state.window,(HMENU)(INT_PTR)STNC_INPUT_CANCEL,GetModuleHandleA(NULL),NULL);
    SendMessage(label,WM_SETFONT,(WPARAM)font,TRUE);SendMessage(state.edit,WM_SETFONT,(WPARAM)font,TRUE);SendMessage(ok,WM_SETFONT,(WPARAM)font,TRUE);SendMessage(cancel,WM_SETFONT,(WPARAM)font,TRUE);SendMessage(state.edit,EM_SETLIMITTEXT,STNC_PROFILE_NAME_MAX,0);
    GetWindowRect(parent,&r);SetWindowPos(state.window,HWND_TOP,r.left+(r.right-r.left-390)/2,r.top+(r.bottom-r.top-155)/2,390,155,SWP_SHOWWINDOW);EnableWindow(parent,FALSE);SetFocus(state.edit);stnc_input_current=&state;
    while(!state.done&&GetMessage(&msg,NULL,0,0)>0){if(!IsDialogMessage(state.window,&msg)){TranslateMessage(&msg);DispatchMessage(&msg);}}
    stnc_input_current=NULL;EnableWindow(parent,TRUE);SetForegroundWindow(parent);if(!state.accepted)return 0;memcpy(value,state.value,sizeof(state.value));return 1;
}

static void stnc_profile_menu_load(HMENU wallet)
{
    stnc_profile_info profiles[STNC_PROFILE_LIST_MAX];size_t count=0u,i;stnc_profile_menu=CreatePopupMenu();stnc_profile_name_count=0u;
    AppendMenuA(wallet,MF_SEPARATOR,0,NULL);AppendMenuA(wallet,MF_STRING,STNC_MENU_NEW_WALLET,"New Wallet...");
    if(stnc_profile_list(profiles,STNC_PROFILE_LIST_MAX,&count)==0){for(i=0u;i<count&&i<STNC_PROFILE_LIST_MAX;i++){memcpy(stnc_profile_names[i],profiles[i].name,strlen(profiles[i].name)+1u);AppendMenuA(stnc_profile_menu,MF_STRING,STNC_MENU_PROFILE_BASE+(UINT)i,profiles[i].name);stnc_profile_name_count++;}}
    AppendMenuA(wallet,MF_POPUP,(UINT_PTR)stnc_profile_menu,"Use Wallet");
}

static BOOL stnc_append_menu(HMENU menu,UINT flags,UINT_PTR item,LPCSTR text)
{
    if((flags&MF_POPUP)!=0&&text!=NULL&&strcmp(text,"Wallet")==0)stnc_profile_menu_load((HMENU)item);
    return AppendMenuA(menu,flags,item,text);
}

static LRESULT CALLBACK stnc_profile_window_proc(HWND w,UINT m,WPARAM wp,LPARAM lp);
static ATOM stnc_register_class(const WNDCLASSA *source)
{
    WNDCLASSA c=*source;stnc_base_window_proc=c.lpfnWndProc;c.lpfnWndProc=stnc_profile_window_proc;return RegisterClassA(&c);
}

static int stnc_message_box(HWND w,LPCSTR text,LPCSTR caption,UINT type)
{
    if(text!=NULL&&strcmp(text,"Enter a canonical stnw0_ address and a positive whole-unit amount.")==0)
        text="Please enter a valid STNC wallet address and an amount greater than zero.";
    return MessageBoxA(w,text,caption,type);
}

#undef SS_LEFT
#define SS_LEFT (0x00000000L | WS_CLIPSIBLINGS)
#define AppendMenuA stnc_append_menu
#define RegisterClassA stnc_register_class
#define MessageBoxA stnc_message_box

static BOOL stnc_gui_show_window(HWND h,int command);
static BOOL stnc_gui_set_window_text(HWND h,const char *text);
#define ShowWindow stnc_gui_show_window
#define SetWindowTextA stnc_gui_set_window_text
#include "gui_btc.c"
#undef SetWindowTextA
#undef ShowWindow
#undef MessageBoxA
#undef RegisterClassA
#undef AppendMenuA

static void stnc_set_profile_caption(void)
{
    stnc_profile_info profile;char caption[128];if(ui.w!=NULL&&stnc_profile_active(&profile)==0){snprintf(caption,sizeof(caption),"STNC Core - %s",profile.name);SetWindowTextA(ui.w,caption);}
}

static void stnc_contract_tracking_reset(void)
{
    stnc_contract_identity[0]='\0';stnc_contract_seen_count=0u;stnc_contract_seen_ready=0;memset(stnc_contract_seen,0,sizeof(stnc_contract_seen));
}

static int stnc_contract_was_seen(const char *address)
{
    size_t i;for(i=0u;i<stnc_contract_seen_count;++i)if(strcmp(stnc_contract_seen[i],address)==0)return 1;return 0;
}

static void stnc_contract_tracking_update(void)
{
    size_t i;int arrived=0;char notice[256];const stnc_client_snapshot *s=&ui.snap;
    if(!s->identity.valid){stnc_contract_tracking_reset();return;}
    if(strcmp(stnc_contract_identity,s->identity.address)!=0){stnc_contract_tracking_reset();snprintf(stnc_contract_identity,sizeof(stnc_contract_identity),"%s",s->identity.address);}
    if(!s->network.chain_connected||!s->contracts_available)return;
    if(!stnc_contract_seen_ready){for(i=0u;i<s->contracts.count&&i<STNC_CONTRACT_LIST_MAX;++i)snprintf(stnc_contract_seen[i],sizeof(stnc_contract_seen[i]),"%s",s->contracts.entries[i].address);stnc_contract_seen_count=s->contracts.count<STNC_CONTRACT_LIST_MAX?s->contracts.count:STNC_CONTRACT_LIST_MAX;stnc_contract_seen_ready=1;return;}
    for(i=0u;i<s->contracts.count;++i){const char *address=s->contracts.entries[i].address;if(!stnc_contract_was_seen(address)){if(stnc_contract_seen_count<STNC_CONTRACT_LIST_MAX)snprintf(stnc_contract_seen[stnc_contract_seen_count++],sizeof(stnc_contract_seen[0]),"%s",address);arrived++;}}
    if(arrived>0){snprintf(notice,sizeof(notice),arrived==1?"A new Contract has arrived for this identity.\n\nOpen Tools > Contracts to view it.":"%d new Contracts have arrived for this identity.\n\nOpen Tools > Contracts to view them.",arrived);MessageBoxA(ui.w,notice,"New Contract",MB_OK|MB_ICONINFORMATION);}
}

static void stnc_clear_contract_form_after_success(HWND h,const char *text)
{
    if(h!=field(CONTRACT_RESULT)||text==NULL||strncmp(text,"Contract: ",10)!=0)return;
    if(strstr(text,"Submission: admitted")==NULL&&strstr(text,"Submission: duplicate")==NULL)return;
    SetWindowTextA(field(CONTRACT_PARTICIPANTS),"");SetWindowTextA(field(CONTRACT_TERMS),"");SendMessage(field(CONTRACT_TYPE),CB_SETCURSEL,0,0);
}

static LRESULT CALLBACK stnc_profile_window_proc(HWND w,UINT m,WPARAM wp,LPARAM lp)
{
    UINT id=LOWORD(wp);
    if(m==WM_COMMAND&&id==STNC_MENU_NEW_WALLET){
        char name[STNC_PROFILE_NAME_MAX+1u];stnc_profile_info profile;
        if(stnc_prompt_name(w,"Create STNC Wallet",name)){
            if(!stnc_profile_name_valid(name)||stnc_profile_create(name,&profile)!=0)MessageBoxA(w,"That wallet name is invalid or already exists.","STNC Core",MB_OK|MB_ICONWARNING);
            else {if(stnc_profile_name_count<STNC_PROFILE_LIST_MAX){size_t i=stnc_profile_name_count++;memcpy(stnc_profile_names[i],profile.name,strlen(profile.name)+1u);AppendMenuA(stnc_profile_menu,MF_STRING,STNC_MENU_PROFILE_BASE+(UINT)i,profile.name);DrawMenuBar(w);}stnc_contract_tracking_reset();stnc_set_profile_caption();PostMessage(w,UPDATED,0,0);}
        }
        return 0;
    }
    if(m==WM_COMMAND&&id>=STNC_MENU_PROFILE_BASE&&id<STNC_MENU_PROFILE_BASE+stnc_profile_name_count){
        size_t index=(size_t)(id-STNC_MENU_PROFILE_BASE);stnc_profile_info profile;
        if(stnc_profile_select(stnc_profile_names[index],&profile)!=0)MessageBoxA(w,"The selected wallet or its corresponding identity is unavailable or invalid.","STNC Core",MB_OK|MB_ICONERROR);
        else {stnc_contract_tracking_reset();stnc_set_profile_caption();PostMessage(w,UPDATED,0,0);}
        return 0;
    }
    return stnc_base_window_proc!=NULL?CallWindowProcA(stnc_base_window_proc,w,m,wp,lp):DefWindowProcA(w,m,wp,lp);
}

static BOOL stnc_gui_show_window(HWND h,int command)
{
    int id;BOOL result;if(!h)return FALSE;id=GetDlgCtrlID(h);
    if(id==CONTRACT_LOOKUP_ADDRESS||id==CONTRACT_LOOKUP||id==CONTRACT_LOOKUP_RESULT)command=SW_HIDE;
    if(id==CREATE_WALLET&&ui.page==P_WALLET_ADDRESS&&!ui.snap.wallet.key_valid)command=SW_SHOW;
    if(id==CREATE_IDENTITY&&ui.page==P_WALLET_IDENTITY&&!ui.snap.identity.valid)command=SW_SHOW;
    result=ShowWindow(h,command);
    if(command!=SW_HIDE&&command!=SW_MINIMIZE)SetWindowPos(h,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    return result;
}

static BOOL stnc_gui_set_window_text(HWND h,const char *text)
{
    char out[4096];size_t used=0u,i;
    if(h==ui.title)stnc_set_profile_caption();
    if(h==ui.content)stnc_contract_tracking_update();
    stnc_clear_contract_form_after_success(h,text);
    if(h!=ui.content||ui.page!=P_CONTRACTS)return SetWindowTextA(h,text);
    if(!ui.snap.identity.valid||!ui.snap.network.chain_connected||!ui.snap.contracts_available||ui.snap.contracts.count==0u)return SetWindowTextA(h,"No Contracts associated");
    used=(size_t)snprintf(out,sizeof(out),"Contracts\r\n\r\n");
    for(i=0u;i<ui.snap.contracts.count&&used<sizeof(out);++i){
        const stnc_contract_list_entry *e=&ui.snap.contracts.entries[i];
        int n=snprintf(out+used,sizeof(out)-used,"%s\r\nState: %s   Type: %s   Sequence: %" PRIu64 "\r\n%s",e->address,stnc_contract_state_name(e->state),stnc_contract_type_name(e->type),e->sequence,i+1u<ui.snap.contracts.count?"\r\n":"");
        if(n<0||(size_t)n>=sizeof(out)-used)break;used+=(size_t)n;
    }
    return SetWindowTextA(h,out);
}
