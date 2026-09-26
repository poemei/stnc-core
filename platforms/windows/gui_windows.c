/* Windows STNC Core frontend.
 *
 * The implementation lives in gui_btc.c so the build keeps one GUI entry
 * point.  The main page text is a STATIC child that occupies the same client
 * area as each page's interactive controls.  STATIC controls do not clip
 * sibling windows by default, so a SetWindowText()/repaint during the periodic
 * Core refresh could paint over buttons, edits and combo boxes even though the
 * controls were still present and visible.
 *
 * Add WS_CLIPSIBLINGS to SS_LEFT before compiling the frontend.  Page controls
 * then remain visible across every periodic status repaint.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#undef SS_LEFT
#define SS_LEFT (0x00000000L | WS_CLIPSIBLINGS)

#include "gui_btc.c"
