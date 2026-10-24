/* about_dialog.cpp — About box dialog. */
#include "menu.h"
#include <Dialogs.h>
#include <Windows.h>
#include <Events.h>
#include <Fonts.h>

void ShowAboutBox()
{
    /* Stack-allocate dialog record to avoid heap pressure (avoids dsMemFullErr) */
    DialogRecord dlgRec = {};
    DialogPtr dlg = GetNewDialog(kDialogAboutID, &dlgRec, (WindowPtr)-1);
    if (!dlg) return;
    InitCursor();
    ShowWindow(dlg);

    short item = 0;
    while (item != 1)
        ModalDialog(nil, &item);

    DisposeDialog(dlg);
}
