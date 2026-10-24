/* Settings.cpp — API key settings dialog. */
#include "config.h"
#include "Settings.h"
#include "menu.h"
#include "helpers.h"

#include <Dialogs.h>
#include <Windows.h>
#include <TextEdit.h>
#include <Scrap.h>
#include <Events.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Logo pixel data — claude16.png converted to 16×16 1-bit bitmap     */
/* ------------------------------------------------------------------ */
static const uint8_t kLogoData[32] = {
    0x00,0x00,  /* ................ */
    0x11,0x00,  /* ...X...X........ */
    0x19,0x08,  /* ...XX..X....X... */
    0x49,0x18,  /* .X..X..X...XX... */
    0x2D,0x30,  /* ..X.XX.X..XX.... */
    0x15,0x60,  /* ...X.X.X.XX..... */
    0x0F,0xC6,  /* ....XXXX.XX...XX */
    0x07,0xF8,  /* .....XXXXXXXX... */
    0x7F,0xE0,  /* .XXXXXXXXXX..... */
    0x07,0xDE,  /* .....XXXXX.XXXX. */
    0x0F,0xE0,  /* ....XXXXXXX..... */
    0x12,0xD0,  /* ...X..X.XX.X.... */
    0x25,0x68,  /* ..X.X.X.X.XX.X.. */
    0x49,0x30,  /* .X..X..X..XX.... */
    0x11,0x10,  /* ...X...X...X.... */
    0x00,0x00,  /* ................ */
};

/* ------------------------------------------------------------------ */
/* DrawSettingsItem — UserItem proc for items 3, 4, 6                 */
/* ------------------------------------------------------------------ */
static pascal void DrawSettingsItem(DialogPtr dlg, short item)
{
    short type; Handle h; Rect r;
    GetDialogItem(dlg, item, &type, &h, &r);
    SetPort(dlg);
    EraseRect(&r);

    if (item == 3) {
        /* ── Logo + title ── */
        /* "Claude 68" in Times 14 — measure first for centering */
        static SInt16 sTimesID = 0;
        if (!sTimesID) GetFNum((ConstStr255Param)"\pTimes", &sTimesID);
        TextFont(sTimesID); TextSize(16); TextFace(0);
        Str255 titlePs; CStrToPStr(APP_NAME, titlePs);
        short textW  = StringWidth(titlePs);
        short kGap   = 6;                           /* gap between logo and text */
        short totalW = 16 + kGap + textW;           /* logo + gap + text */
        short startX = r.left + (r.right - r.left - totalW) / 2;

        /* Logo: 16×16, vertically centered in the 22px row */
        BitMap bm;
        bm.baseAddr = (Ptr)kLogoData;
        bm.rowBytes = 2;
        SetRect(&bm.bounds, 0, 0, 16, 16);
        Rect dst = { (short)(r.top + 3), startX,
                     (short)(r.top + 19), (short)(startX + 16) };
        CopyBits(&bm, &((GrafPtr)dlg)->portBits, &bm.bounds, &dst, srcCopy, nil);

        FontInfo fi; GetFontInfo(&fi);
        MoveTo((short)(startX + 16 + kGap), (short)(r.top + 3 + fi.ascent));
        DrawString(titlePs);
        TextFont(0); TextFace(0); TextSize(0);

    } else if (item == 4) {
        /* ── Label block: "API Key" in system font (matches edit field) ── */
        TextFont(0); TextSize(0); TextFace(0);
        FontInfo fi; GetFontInfo(&fi);
        short lineH = fi.ascent + fi.descent + fi.leading;
        MoveTo(r.left, (short)(r.top + fi.ascent));
        DrawString("\pAPI Key");
        MoveTo(r.left, (short)(r.top + fi.ascent + lineH));
        SetGenevaFont(12, 0);
        DrawString("\pGet yours at: console.anthropic.com/settings/api-keys");
        TextFont(0); TextSize(0); TextFace(0);
    }
}

/* ------------------------------------------------------------------ */
/* §15 DEDUP: activate/deactivate the dialog TE based on a flag       */
/* ------------------------------------------------------------------ */
static void SetDialogTEActive(DialogPtr dlg, bool active)
{
    TEHandle te = ((DialogPeek)dlg)->textH;
    if (!te) return;
    if (active) TEActivate(te);
    else        TEDeactivate(te);
}

/* ------------------------------------------------------------------ */
/* ShowSettingsDialog                                                  */
/* DLOG 129: 1=Save, 2=Cancel, 3=title/logo, 4=label, 5=edit         */
/* ------------------------------------------------------------------ */
bool ShowSettingsDialog(AppSettings &settings)
{
    DialogPtr dlg = GetNewDialog(kDialogSettingsID, NULL, (WindowPtr)-1);
    if (!dlg) return false;

    UserItemUPP drawUPP = NewUserItemUPP(DrawSettingsItem);
    if (!drawUPP) { DisposeDialog(dlg); return false; }
    static const short kLabelItems[] = { 3, 4 };
    SetUserItems(dlg, kLabelItems, 2, drawUPP);

    /* Pre-fill edit field (item 5) */
    {
        Handle h; short type; Rect r; Str255 ps;
        GetDialogItem(dlg, 5, &type, &h, &r);
        CStrToPStr(settings.apiKey, ps); SetDialogItemText(h, ps);
    }
    SelectDialogItemText(dlg, 5, 0, 32767);
    InitCursor();
    ShowWindow(dlg);

    DrawDefaultButtonRing(dlg, 1);
    DisableDialogMenus();
    bool saved = false;
    short item = 0;
    while (item != 1 && item != 2) {
        EventRecord e;
        WaitNextEvent(everyEvent, &e, 6, nil);

        if (e.what == updateEvt) {
            WindowPtr wp = (WindowPtr)e.message;
            BeginUpdate(wp);
            if (((WindowPeek)wp)->windowKind == dialogKind) {
                DrawDialog((DialogPtr)wp);
                if ((DialogPtr)wp == dlg) DrawDefaultButtonRing(dlg, 1);
            }
            EndUpdate(wp);
            continue;
        }
        if (e.what == osEvt) {
            if (((e.message >> 24) & 0xFF) == suspendResumeMessage)
                SetDialogTEActive(dlg, (e.message & resumeFlag) != 0);
            continue;
        }
        if (e.what == activateEvt) {
            SetDialogTEActive(dlg, (e.modifiers & activeFlag) != 0);
            continue;
        }
        if (e.what == keyDown || e.what == autoKey) {
            char c = (char)(e.message & charCodeMask);
            if (c == 0x0D || c == 0x03) { item = 1; break; }
            TEHandle te = ((DialogPeek)dlg)->textH;
            if (e.modifiers & cmdKey) {
                if ((c == 'v' || c == 'V') && te) {
                    // §2 DEDUP: use shared clipboard helper
                    // Intentional replace-all paste: an API key is always
                    // pasted whole, never inserted at the caret.
                    char buf[512];
                    long cl = GetClipboardTextFiltered(buf, (long)sizeof(buf));
                    TESetSelect(0, 32767, te); TEDelete(te);
                    if (cl > 0) TEInsert(buf, cl, te);
                    continue;
                }
                if ((c == 'a' || c == 'A') && te) { TESetSelect(0, 32767, te); continue; }
                if ((c == 'x' || c == 'X') && te) { TECut(te); ZeroScrap(); TEToScrap(); continue; }
                if ((c == 'c' || c == 'C') && te) { TECopy(te); ZeroScrap(); TEToScrap(); continue; }
            }
        }
        if (e.what == mouseDown) {
            WindowPtr hitWin; short part = FindWindow(e.where, &hitWin);
            if (part == inMenuBar) { DispatchDialogMenu(dlg, e); continue; }
            if (part == inDrag)   { DragDialog(dlg, e, hitWin); continue; }
            if (hitWin != (WindowPtr)dlg) continue;
        }
        DialogSelect(&e, &dlg, &item);
    }
    RestoreDialogMenus();

    if (item == 1) {
        Handle h; short type; Rect r; Str255 ps;
        GetDialogItem(dlg, 5, &type, &h, &r);
        GetDialogItemText(h, ps); PStrToCStr(ps, settings.apiKey, (int)sizeof(settings.apiKey));
        SaveSettings(settings);
        saved = true;
    }

    DisposeDialog(dlg);
    DisposeUserItemUPP(drawUPP);
    return saved;
}
