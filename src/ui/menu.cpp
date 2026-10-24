/* menu.cpp — Menu setup and command dispatch. */
#include "menu.h"
#include "Settings.h"
#include "../main.h"
#include "chat_window.h"
#include "helpers.h"

#include <Menus.h>
#include <Devices.h>
#include <Scrap.h>
#include <Events.h>
#include <TextEdit.h>

extern bool        gDebugMode;
extern void        DoQuit();   /* defined in main.cpp */

/* ------------------------------------------------------------------ */
/* SetupMenus — install MBAR 128                                      */
/* ------------------------------------------------------------------ */
void SetupMenus()
{
    Handle mbar = GetNewMBar(kMBARID);
    if (mbar) { SetMenuBar(mbar); DisposeHandle(mbar); }

    /* Apple menu desk accessories */
    MenuHandle appleMH = GetMenuHandle(kMenuApple);
    if (appleMH) AppendResMenu(appleMH, 'DRVR');

    DrawMenuBar();
}

/* ------------------------------------------------------------------ */
/* UpdateMenus                                                         */
/* ------------------------------------------------------------------ */
void UpdateMenus()
{
    MenuHandle appleMH = GetMenuHandle(kMenuApple);
    if (appleMH) CheckItem(appleMH, kItemAppleDebug, gDebugMode);
}

/* ------------------------------------------------------------------ */
/* DoMenuCommand                                                       */
/* ------------------------------------------------------------------ */
bool DoMenuCommand(long menuResult)
{
    short mID   = (short)(menuResult >> 16);
    short mItem = (short)(menuResult & 0xFFFF);
    bool  handled = false;

    switch (mID) {
        case kMenuApple:
            if (mItem == kItemAppleDebug) {
                gDebugMode = !gDebugMode;
                gSettings.debugMode = gDebugMode ? 1 : 0;
                SaveSettings(gSettings);
            } else if (mItem > 2) {   /* items 1-2 are static; DAs start at 3 */
                Str255 name;
                GetMenuItemText(GetMenuHandle(kMenuApple), mItem, name);
                OpenDeskAcc(name);
            }
            handled = true;
            break;

        case kMenuApp:
            switch (mItem) {
                case kItemAbout:
                    ShowAboutBox();
                    handled = true;
                    break;
                case kItemSettings:
                    ShowSettingsDialog(gSettings);
                    handled = true;
                    break;
                case kItemQuit:
                    DoQuit();
                    handled = true;
                    break;
            }
            break;

        case kMenuEdit: {
            TEHandle te = GetChatFocusTE();
            switch (mItem) {
                case kItemCopy:
                    if (te) { TECopy(te); ZeroScrap(); TEToScrap(); } handled = true; break;
                case kItemCut:
                    if (te) { TECut(te);  ZeroScrap(); TEToScrap(); } handled = true; break;
                case kItemPaste:
                    if (te) {
                        // §2 DEDUP: use shared clipboard helper
                        char buf[512];
                        long cl = GetClipboardTextFiltered(buf, (long)sizeof(buf));
                        TEDelete(te);  /* replace selection only, not all text */
                        if (cl > 0) TEInsert(buf, cl, te);
                    }
                    handled = true; break;
                case kItemSelectAll:
                    if (te) TESetSelect(0, 32767, te);
                    handled = true; break;
            }
            break;
        }
    }

    HiliteMenu(0);
    return handled;
}

