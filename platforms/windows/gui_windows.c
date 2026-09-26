/* Windows STNC Core frontend.
 *
 * gui_btc.c owns the frontend. This wrapper provides one Win32 visibility
 * policy for page controls so periodic Core refreshes cannot paint over or
 * incorrectly hide interactive controls.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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
     * existence of a key file. A stale/invalid key must not remove recovery.
     */
    if(id==CREATE_WALLET&&ui.page==P_WALLET_ADDRESS&&!ui.snap.wallet.key_valid)
        command=SW_SHOW;
    if(id==CREATE_IDENTITY&&ui.page==P_WALLET_IDENTITY&&!ui.snap.identity.valid)
        command=SW_SHOW;

    result=ShowWindow(h,command);

    /* Page controls always sit above the page text surface. SetWindowText on
     * the status surface occurs every second and must never obscure controls.
     */
    if(command!=SW_HIDE&&command!=SW_MINIMIZE)
        SetWindowPos(h,HWND_TOP,0,0,0,0,
            SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);

    return result;
}
