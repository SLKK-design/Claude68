/*
    App resources — menus, windows, dialogs, icon.

    To rename for a new project, change APP_NAME below (keep in sync with config.h).
    The about-box strings (DITL 130) and icon data (ICN#) are also project-specific.
*/

/* ── STEP 1: Change these to match src/config.h ─────────────────────── */
#define APP_NAME           "Claude 68"    /* must match APP_NAME in src/config.h   */
#define APP_VERSION_STRING "Version 1.0  \0xD4  System 7\0xC9 9"
#define APP_AUTHOR_STRING  "by Claude Code"
/* ─────────────────────────────────────────────────────────────────── */

#include "Processes.r"
#include "Menus.r"
#include "Windows.r"
#include "MacTypes.r"
#include "Dialogs.r"
#include "Finder.r"

/* ------------------------------------------------------------------ */
/* Menus                                                               */
/* ------------------------------------------------------------------ */

resource 'MENU' (128) {
    128, textMenuProc;
    allEnabled, enabled;
    apple;
    {
        "Debug mode", noIcon, noKey, noMark, plain;   /* item 1 */
        "-", noIcon, noKey, noMark, plain;            /* item 2 — DAs follow at runtime */
    }
};

resource 'MENU' (129) {
    129, textMenuProc;
    allEnabled, enabled;
    "Claude";
    {
        "About " APP_NAME, noIcon, noKey, noMark, plain;   /* item 1 */
        "-", noIcon, noKey, noMark, plain;
        "Settings\0xC9", noIcon, noKey, noMark, plain;      /* item 3 */
        "-", noIcon, noKey, noMark, plain;
        "Quit", noIcon, "Q", noMark, plain;                /* item 5 */
    }
};

resource 'MENU' (130) {
    130, textMenuProc;
    allEnabled, enabled;
    "Edit";
    {
        "Copy",       noIcon, "C", noMark, plain;   /* item 1 */
        "Paste",      noIcon, "V", noMark, plain;   /* item 2 */
        "Cut",        noIcon, "X", noMark, plain;   /* item 3 */
        "Select All", noIcon, "A", noMark, plain;   /* item 4 */
    }
};

resource 'MBAR' (128) {
    { 128, 129, 130 };
};

/* ------------------------------------------------------------------ */
/* Chat window                                                         */
/* ------------------------------------------------------------------ */

resource 'WIND' (128) {
    {0, 0, 280, 440}, noGrowDocProc;
    invisible;
    goAway;
    0, APP_NAME;
    centerMainScreen;
};

/* ------------------------------------------------------------------ */
/* Settings dialog (DLOG 129)                                          */
/* Items: 1=Save, 2=Cancel, 3=title/logo, 4=API Key label, 5=edit     */
/* ------------------------------------------------------------------ */

resource 'DLOG' (129) {
    {0, 0, 190, 420},
    dBoxProc,
    invisible,
    noGoAway,
    0x0,
    129,
    "",
    centerMainScreen
};

resource 'DITL' (129) {
    {
        /* 1: Save button */
        {142, 314, 162, 402},
        Button { enabled, "Save" };

        /* 2: Cancel button */
        {142, 210, 162, 298},
        Button { enabled, "Cancel" };

        /* 3: title row — logo + app name in Times 16, drawn by proc */
        {18, 20, 42, 400},
        UserItem { disabled };

        /* 4: label block — "API Key" bold + help text regular, drawn by proc */
        {52, 20, 80, 400},
        UserItem { disabled };

        /* 5: API key edit field — 20px gap above and below */
        {100, 20, 118, 400},
        EditText { enabled, "" };
    }
};

/* ------------------------------------------------------------------ */
/* About dialog (DLOG 130)                                             */
/* ------------------------------------------------------------------ */

resource 'DLOG' (130) {
    {0, 0, 130, 320},
    movableDBoxProc,
    invisible,
    noGoAway,
    0x0,
    130,
    "About",
    centerMainScreen
};

resource 'DITL' (130) {
    {
        /* 1: OK button */
        {100, 220, 120, 310},
        Button { enabled, "OK" };

        /* 2-5: static text */
        {10, 16, 26, 310},
        StaticText { disabled, APP_NAME " for Classic Macintosh" };

        {30, 16, 46, 310},
        StaticText { disabled, APP_VERSION_STRING };

        {50, 16, 66, 310},
        StaticText { disabled, "Made with Retro68" };

        {72, 16, 88, 310},
        StaticText { disabled, APP_AUTHOR_STRING };
    }
};

/* ------------------------------------------------------------------ */
/* SIZE resource                                                        */
/* ------------------------------------------------------------------ */

resource 'SIZE' (-1) {
    reserved,
    acceptSuspendResumeEvents,
    reserved,
    canBackground,
    doesActivateOnFGSwitch,
    backgroundAndForeground,
    dontGetFrontClicks,
    ignoreChildDiedEvents,
    is32BitCompatible,
    isHighLevelEventAware,
    onlyLocalHLEvents,
    notStationeryAware,
    dontUseTextEditServices,
    notDisplayManagerAware,
    reserved,
    reserved,
    2 * 1024 * 1024,    /* preferred 2048K — peak observed use ~740K (454K code + 128K stack + heap) */
    1536 * 1024         /* minimum 1536K */
};

/* ------------------------------------------------------------------ */
/* Application icon — replace ICN# data with your own 32×32 bitmap     */
/* ------------------------------------------------------------------ */

data 'Cl68' (0) {
};

resource 'FREF' (128) {
    'APPL', 0, ""
};

resource 'BNDL' (128) {
    'Cl68', 0,
    {
        'FREF', { 0, 128 };
        'ICN#', { 0, 128 }
    }
};

data 'ICN#' (-16455) {
    /* icon */
    $"00000000 00000000 0000C000 0001C000"
    $"0381C000 03C1C300 03E1C700 01F1CF00"
    $"00F9DE00 1C7DDC00 1F3EFC30 1F9FF8F0"
    $"0FEFFBF0 03FFFFC0 00FFFF00 007FFC00"
    $"001FFFF8 3FFFFFF8 1FFFFE00 001FFF00"
    $"007FFFC0 00EFFDF0 01CDDE70 039DCF00"
    $"0319C780 0039C380 0071C180 0071C000"
    $"0061C000 0001C000 00000000 00000000"
    /* mask (non-transparent pixels) */
    $"001FF800 007FFE00 01FFFF80 03FFFFC0"
    $"07FFFFE0 0FFFFFF0 1FFFFFF8 3FFFFFFC"
    $"3FFFFFFC 7FFFFFFE 7FFFFFFE FFFFFFFF"
    $"FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF"
    $"FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF"
    $"FFFFFFFF 7FFFFFFE 7FFFFFFE 3FFFFFFC"
    $"3FFFFFFC 1FFFFFF8 0FFFFFF0 07FFFFE0"
    $"03FFFFC0 01FFFF80 007FFE00 001FF800"
};

data 'ICN#' (128) {
    /* icon */
    $"00000000 00000000 0000C000 0001C000"
    $"0381C000 03C1C300 03E1C700 01F1CF00"
    $"00F9DE00 1C7DDC00 1F3EFC30 1F9FF8F0"
    $"0FEFFBF0 03FFFFC0 00FFFF00 007FFC00"
    $"001FFFF8 3FFFFFF8 1FFFFE00 001FFF00"
    $"007FFFC0 00EFFDF0 01CDDE70 039DCF00"
    $"0319C780 0039C380 0071C180 0071C000"
    $"0061C000 0001C000 00000000 00000000"
    /* mask (non-transparent pixels) */
    $"001FF800 007FFE00 01FFFF80 03FFFFC0"
    $"07FFFFE0 0FFFFFF0 1FFFFFF8 3FFFFFFC"
    $"3FFFFFFC 7FFFFFFE 7FFFFFFE FFFFFFFF"
    $"FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF"
    $"FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF"
    $"FFFFFFFF 7FFFFFFE 7FFFFFFE 3FFFFFFC"
    $"3FFFFFFC 1FFFFFF8 0FFFFFF0 07FFFFE0"
    $"03FFFFC0 01FFFF80 007FFE00 001FF800"
};

data 'ics#' (-16455) {
    /* 16×16 icon */
    $"0000 1100 1908 4918"
    $"2D30 1560 0FC6 07F8"
    $"7FE0 07DE 0FE0 12D0"
    $"2568 4930 1110 0000"
    /* mask — same as icon (white = transparent) */
    $"0000 1100 1908 4918"
    $"2D30 1560 0FC6 07F8"
    $"7FE0 07DE 0FE0 12D0"
    $"2568 4930 1110 0000"
};

data 'ics#' (128) {
    /* 16×16 icon */
    $"0000 1100 1908 4918"
    $"2D30 1560 0FC6 07F8"
    $"7FE0 07DE 0FE0 12D0"
    $"2568 4930 1110 0000"
    /* mask — same as icon (white = transparent) */
    $"0000 1100 1908 4918"
    $"2D30 1560 0FC6 07F8"
    $"7FE0 07DE 0FE0 12D0"
    $"2568 4930 1110 0000"
};
