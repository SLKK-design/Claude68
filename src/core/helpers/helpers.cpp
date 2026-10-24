/* helpers.cpp — shared drawing, dialog, and string utilities */
#include "helpers.h"
#include "menu.h"

/* ------------------------------------------------------------------ */
/* SetGenevaFont                                                       */
/* ------------------------------------------------------------------ */
void SetGenevaFont(short size, short face)
{
    static SInt16 sFontID = 0;
    if (!sFontID) GetFNum((ConstStr255Param)"\pGeneva", &sFontID);
    TextFont(sFontID);
    TextSize(size);
    TextFace(face);
}

/* ------------------------------------------------------------------ */
/* Dialog helpers                                                      */
/* ------------------------------------------------------------------ */
ControlHandle GetDialogControl(DialogPtr dlg, short itemID)
{
    short  t;
    Handle h;
    Rect   r;
    GetDialogItem(dlg, itemID, &t, &h, &r);
    return (ControlHandle)h;
}

void DrawDefaultButtonRing(DialogPtr dlg, short itemID)
{
    DialogItemType t; Handle h; Rect r;
    GetDialogItem(dlg, itemID, &t, &h, &r);
    SetPort(dlg);
    InsetRect(&r, -4, -4);
    PenSize(3, 3);
    bool disabled = ((**(ControlHandle)h).contrlHilite == 255);
    PenPat(disabled ? &qd.ltGray : &qd.black);
    FrameRoundRect(&r, 16, 16);
    PenNormal();
}

/* ------------------------------------------------------------------ */
/* CStrToPStr / PStrToCStr                                            */
/* ------------------------------------------------------------------ */
void CStrToPStr(const char* cstr, unsigned char* pstr)
{
    int len = cstr ? (int)strlen(cstr) : 0;
    if (len > 254) len = 254;
    pstr[0] = (unsigned char)len;
    if (len) memcpy(pstr + 1, cstr, len);
}

void PStrToCStr(const unsigned char* pstr, char* cstr, int maxLen)
{
    int len = (int)pstr[0];
    if (len >= maxLen) len = maxLen - 1;
    memcpy(cstr, pstr + 1, len);
    cstr[len] = '\0';
}

/* ------------------------------------------------------------------ */
/* GetClipboardTextFiltered                                            */
/* ------------------------------------------------------------------ */
// §2 DEDUP: clipboard fetch with CR/LF/TAB stripped
// §3 MEMORY: DisposeHandle always called — even on empty / error scrap
long GetClipboardTextFiltered(char* out, long maxLen)
{
    Handle sh = NewHandle(0);
    if (!sh) return 0;
    long offset;
    long len = GetScrap(sh, 'TEXT', &offset);
    long cleanLen = 0;
    if (len > 0 && out && maxLen > 0) {
        HLock(sh);
        for (long i = 0; i < len && cleanLen < maxLen - 1; i++) {
            char ch = (*sh)[i];
            if (ch != '\r' && ch != '\n' && ch != '\t')
                out[cleanLen++] = ch;
        }
        HUnlock(sh);
    }
    DisposeHandle(sh); // §3 MEMORY: always freed
    return cleanLen;
}

/* ------------------------------------------------------------------ */
/* ScrollPartDelta                                                     */
/* ------------------------------------------------------------------ */
short ScrollPartDelta(short part, short lineAmt, short pageAmt)
{
    switch (part) {
        case kScrollPartUpButton:   return -lineAmt;  // §10 ARCH: named constants
        case kScrollPartDownButton: return  lineAmt;
        case kScrollPartPageUp:     return -pageAmt;
        case kScrollPartPageDown:   return  pageAmt;
    }
    return 0;
}

ControlHandle MakeScrollBar(GrafPtr port, const Rect& r,
                            short val, short minVal, short maxVal)
{
    return NewControl(port, &r, "\p", true, val, minVal, maxVal,
                      16 /* scrollBarProc */, 0L);
}

/* ------------------------------------------------------------------ */
/* Dialog event helpers                                                */
/* ------------------------------------------------------------------ */
void SetUserItems(DialogPtr dlg, const short* items, int count, UserItemUPP drawUPP)
{
    for (int i = 0; i < count; i++) {
        short  t;
        Handle h;
        Rect   r;
        GetDialogItem(dlg, items[i], &t, &h, &r);
        SetDialogItem(dlg, items[i], userItem, (Handle)drawUPP, &r);
    }
}

/* Disable all app menu items while a modal dialog is open (Quit stays enabled).
   Restore them when the dialog closes. */
static void ApplyDialogMenus(bool enable)
{
    MenuHandle appMH = GetMenuHandle(kMenuApp);
    if (appMH) {
        short total = CountMenuItems(appMH);
        for (short it = 1; it <= total; it++) {
            if (enable) EnableItem(appMH, it);
            else if (it != kItemQuit) DisableItem(appMH, it);
        }
    }
    DrawMenuBar();
}

void DisableDialogMenus() { ApplyDialogMenus(false); }
void RestoreDialogMenus() { ApplyDialogMenus(true); }

bool DispatchDialogMenu(DialogPtr dlg, EventRecord& e)
{
    long  mr    = MenuSelect(e.where);
    short mID   = (short)(mr >> 16);
    short mItem = (short)(mr & 0xFFFF);
    HiliteMenu(0);

    if (mID == kMenuApp && mItem == kItemQuit) ExitToShell();

    if (mID == kMenuEdit) {
        TEHandle te = ((DialogPeek)dlg)->textH;
        if (te) {
            if (mItem == 1) {
                TECopy(te); ZeroScrap(); TEToScrap();
            } else if (mItem == 2) {
                // §2 DEDUP: use shared clipboard helper
                char buf[512];
                long cleanLen = GetClipboardTextFiltered(buf, (long)sizeof(buf));
                TESetSelect(0, 32767, te); TEDelete(te);
                if (cleanLen > 0) TEInsert(buf, cleanLen, te);
            }
        }
    }
    return false;
}

bool DragDialog(DialogPtr dlg, EventRecord& e, WindowPtr hitWin)
{
    if (hitWin != (WindowPtr)dlg) return false;
    Rect sb = qd.screenBits.bounds;
    DragWindow((WindowPtr)dlg, e.where, &sb);
    return true;
}

