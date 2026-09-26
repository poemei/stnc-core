/* Windows STNC Core frontend.
 *
 * gui_btc.c owns the frontend. This wrapper provides the Win32 sibling
 * clipping and visibility policy required by the periodically refreshed page
 * text surface. Interactive controls must remain visible and usable while the
 * Core snapshot is repainted.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* gui_btc.c creates the page text surface as SS_LEFT. Without
 * WS_CLIPSIBLINGS that STATIC control can repaint across sibling buttons,
 * edits and combo boxes every time SetWindowText() refreshes it. The controls
 * still exist, which is why they briefly appear and then seem to disappear.
 */
#undef SS_LEFT
#define SS_LEFT (0x00000000L | WS_CLIPSIBLINGS)

static BOOL stnc_gui_show_window(HWND h,int command);
#define ShowWindow stnc_gui_show_window
#include "gui_btc.c"
#undef ShowWindow

static BOOL stnc_gui_show_window(HWND h,int command)
{
    int id;
    BOOL result;

    if(!h)return FALSE;
    id=GetDlgCtrlID(h);

    /* Creation is based on a usable canonical object, not merely the
     * existence of a stale or invalid key file.
     */
    if(id==CREATE_WALLET&&ui.page==P_WALLET_ADDRESS&&!ui.snap.wallet.key_valid)
        command=SW_SHOW;
    if(id==CREATE_IDENTITY&&ui.page==P_WALLET_IDENTITY&&!ui.snap.identity.valid)
        command=SW_SHOW;

    result=ShowWindow(h,command);

    /* Keep visible page controls above the shared page text surface. */
    if(command!=SW_HIDE&&command!=SW_MINIMIZE)
        SetWindowPos(h,HWND_TOP,0,0,0,0,
            SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);

    return result;
}
