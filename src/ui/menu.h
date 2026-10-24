/* menu.h — Menu update, dispatch, and app-level dialog interface. */
#pragma once

/* ------------------------------------------------------------------ */
/* Resource IDs — keep in sync with wdefshell.r                       */
/* ------------------------------------------------------------------ */
#define kWindowChatID      128
#define kMBARID            128
#define kDialogSettingsID  129
#define kDialogAboutID     130

/* Menu resource IDs */
enum {
    kMenuApple  = 128,
    kMenuApp    = 129,
    kMenuEdit   = 130
};

/* Apple menu items (DAs are appended at runtime after item 2) */
enum {
    kItemAppleDebug = 1
    /* 2 = separator */
    /* 3+ = DAs appended at runtime */
};

/* App menu items */
enum {
    kItemAbout    = 1,
    /* 2 = separator */
    kItemSettings = 3,
    /* 4 = separator */
    kItemQuit     = 5
};

/* Edit menu items */
enum {
    kItemCopy      = 1,
    kItemPaste     = 2,
    kItemCut       = 3,
    kItemSelectAll = 4
};

/* Called once at startup to install the menu bar */
void SetupMenus();

/* Update enable states before MenuSelect */
void UpdateMenus();

/* Dispatch menu selection — returns true if handled */
bool DoMenuCommand(long menuResult);

/* About box */
void ShowAboutBox();
