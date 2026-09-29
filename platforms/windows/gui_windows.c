/* Windows STNC Core frontend. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "stnc_profile.h"

#undef SS_LEFT
#define SS_LEFT (0x00000000L | WS_CLIPSIBLINGS)

static BOOL stnc_gui_show_window(HWND h,int command);
static BOOL stnc_gui_set_window_text(HWND h,const char *text);
#define ShowWindow stnc_gui_show_window
#define SetWindowTextA stnc_gui_set_window_text
#include "gui_btc.c"
#undef SetWindowTextA
#undef ShowWindow

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
    if(h==ui.title){stnc_profile_info profile;char caption[128];if(stnc_profile_active(&profile)==0){snprintf(caption,sizeof(caption),"STNC Core - %s",profile.name);SetWindowTextA(ui.w,caption);}}
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
