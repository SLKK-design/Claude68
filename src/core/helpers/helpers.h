/* helpers.h — shared drawing, dialog, and string utilities */
#pragma once

#include <Quickdraw.h>
#include <Dialogs.h>
#include <Controls.h>
#include <Fonts.h>
#include <Events.h>
#include <Menus.h>
#include <Scrap.h>
#include <TextEdit.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Font helpers                                                        */
/* ------------------------------------------------------------------ */
void SetGenevaFont(short size = 12, short face = 0);

/* ------------------------------------------------------------------ */
/* Dialog helpers                                                      */
/* ------------------------------------------------------------------ */
ControlHandle GetDialogControl(DialogPtr dlg, short itemID);
/* Draw a 3px rounded ring around the default button (Mac HIG visual indicator) */
void DrawDefaultButtonRing(DialogPtr dlg, short itemID);

/* ------------------------------------------------------------------ */
/* String utilities                                                    */
/* ------------------------------------------------------------------ */
void CStrToPStr(const char* cstr, unsigned char* pstr);
void PStrToCStr(const unsigned char* pstr, char* cstr, int maxLen);

/* ------------------------------------------------------------------ */
/* Dialog event helpers                                                */
/* ------------------------------------------------------------------ */
/* Install a custom draw UPP for each of the listed user-item slots in the dialog */
void SetUserItems(DialogPtr dlg, const short* items, int count, UserItemUPP drawUPP);
void DisableDialogMenus();
void RestoreDialogMenus();
bool DispatchDialogMenu(DialogPtr dlg, EventRecord& e);
bool DragDialog(DialogPtr dlg, EventRecord& e, WindowPtr hitWin);

/* ------------------------------------------------------------------ */
/* Clipboard helpers                                                   */
/* ------------------------------------------------------------------ */
/* Fetch clipboard TEXT scrap, strip CR/LF/TAB, write into out[0..maxLen-1].
   Returns the number of clean bytes written (not NUL-terminated). */
long GetClipboardTextFiltered(char* out, long maxLen); // §2 DEDUP: clipboard fetch with CR/LF/TAB stripped

/* ------------------------------------------------------------------ */
/* Scroll helpers                                                      */
/* ------------------------------------------------------------------ */
/* Map a scrollbar part code to a signed scroll delta.
   part: inUpButton=20, inDownButton=21, inPageUp=22, inPageDown=23.
   Returns lineAmt (up), -lineAmt (down), pageAmt (up page), -pageAmt (down page). */
short ScrollPartDelta(short part, short lineAmt, short pageAmt);

// §10 ARCH: scroll bar part codes (Inside Macintosh)
enum {
    kScrollPartUpButton   = 20,
    kScrollPartDownButton = 21,
    kScrollPartPageUp     = 22,
    kScrollPartPageDown   = 23
};

/* Create a standard scrollbar control on port, initially at val within [minVal, maxVal] */
ControlHandle MakeScrollBar(GrafPtr port, const Rect& r,
                            short val = 0, short minVal = 0, short maxVal = 0);
