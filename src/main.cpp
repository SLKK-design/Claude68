/* main.cpp — cooperative event loop and stream lifecycle for classic Mac. */

#include <Memory.h>
#include <Menus.h>
#include <Fonts.h>
#include <Dialogs.h>
#include <Windows.h>
#include <Resources.h>
#include <TextEdit.h>
#include <Events.h>
#include <Quickdraw.h>

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "HttpClient.h"
#include "Settings.h"
#include "main.h"
#include "chat_window.h"
#include "menu.h"
#include "api_client.h"
#include "utf8_mac.h"
#include "growzone.h"

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */
static const short kWinMinWidth  = 200;
static const short kWinMinHeight = 80;
static const short kWinMargin    = 4;   /* gap from screen edge for grow/drag limits */

/* ------------------------------------------------------------------ */
/* Globals                                                             */
/* ------------------------------------------------------------------ */
AppSettings gSettings;
AppState    gAppState = { false, false };
bool        gDebugMode = false;

HttpClient  gHttp(APP_HOST);

/* ------------------------------------------------------------------ */
/* DoQuit — clean shutdown                                             */
/* ------------------------------------------------------------------ */
void DoQuit()
{
    gHttp.SslFreeAll();
    ExitToShell();
}

/* ------------------------------------------------------------------ */
/* InitApp — classic Mac toolbox init                                  */
/* ------------------------------------------------------------------ */
static void InitApp()
{
    /* Stack/heap setup: SetApplLimit called from HttpClient constructor
       before main() — nothing more needed here.                       */
    MaxApplZone();
    for (int i = 0; i < 10; i++) MoreMasters();
    GrowZoneInstall();  /* 32K emergency reserve, released under heap pressure */

    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(nil);
    InitCursor();
    FlushEvents(everyEvent, 0);
}

/* ------------------------------------------------------------------ */
/* HandleUpdate                                                        */
/* ------------------------------------------------------------------ */
static void HandleUpdate(EventRecord& e)
{
    WindowPtr win = (WindowPtr)e.message;
    if (win == gChatWindow) {
        BeginUpdate(win);
        DrawChatWindow();
        EndUpdate(win);
    }
}

/* ------------------------------------------------------------------ */
/* HandleActivate                                                      */
/* ------------------------------------------------------------------ */
static void HandleActivate(EventRecord& e)
{
    WindowPtr win = (WindowPtr)e.message;
    bool activate = (e.modifiers & activeFlag) != 0;
    if (win == gChatWindow) HandleChatActivate(activate);
}

/* ------------------------------------------------------------------ */
/* HandleSuspendResume                                                 */
/* ------------------------------------------------------------------ */
static void HandleSuspendResume(EventRecord& e)
{
    bool resume = (e.message & resumeFlag) != 0;
    if (gChatWindow && FrontWindow() == gChatWindow) HandleChatActivate(resume);
}

/* ------------------------------------------------------------------ */
/* HandleMouseDown                                                     */
/* ------------------------------------------------------------------ */
static void HandleMouseDown(EventRecord& e)
{
    WindowPtr hitWin;
    short part = FindWindow(e.where, &hitWin);

    switch (part) {
        case inMenuBar:
            UpdateMenus();
            DoMenuCommand(MenuSelect(e.where));
            break;

        case inSysWindow:
            SystemClick(&e, hitWin);
            break;

        case inDrag: {
            Rect limit = qd.screenBits.bounds;
            InsetRect(&limit, 4, 4);
            DragWindow(hitWin, e.where, &limit);
            break;
        }

        case inGrow: {
            Rect limit;
            SetRect(&limit, kWinMinWidth, kWinMinHeight,
                    (short)(qd.screenBits.bounds.right  - kWinMargin),
                    (short)(qd.screenBits.bounds.bottom - kWinMargin));
            long newSize = GrowWindow(hitWin, e.where, &limit);
            if (newSize) {
                short newW = (short)(newSize & 0xFFFF);
                short newH = (short)(newSize >> 16);
                SizeWindow(hitWin, newW, newH, true);
                SetPort(hitWin);
                ClipRect(&hitWin->portRect);
                EraseRect(&hitWin->portRect);
                ResizeChatControls();
                DrawChatWindow();
            }
            break;
        }

        case inZoomIn:
        case inZoomOut:
            if (TrackBox(hitWin, e.where, part)) {
                ZoomWindow(hitWin, part, true);
                SetPort(hitWin);
                ClipRect(&hitWin->portRect);
                EraseRect(&hitWin->portRect);
                ResizeChatControls();
                DrawChatWindow();
            }
            break;

        case inGoAway:
            if (TrackGoAway(hitWin, e.where)) DoQuit();
            break;

        case inContent: {
            if (hitWin != FrontWindow()) {
                SelectWindow(hitWin);
            } else {
                Point localPt = e.where;
                SetPort(hitWin);
                GlobalToLocal(&localPt);
                HandleChatClick(hitWin, localPt);
            }
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* HandleKeyDown                                                       */
/* ------------------------------------------------------------------ */
static void HandleKeyDown(EventRecord& e)
{
    char   c       = (char)(e.message & charCodeMask);
    short  mods    = e.modifiers;

    if (mods & cmdKey) {
        if ((mods & shiftKey) && (c == 'C' || c == 'c')) {
            CopyChatToClipboard();
            return;
        }
        UpdateMenus();
        DoMenuCommand(MenuKey(c));
        return;
    }

    if (FrontWindow() == gChatWindow) HandleChatKey(c, mods);
}

/* ------------------------------------------------------------------ */
/* DrainPendingText — move gSSE.text into chat output TE             */
/* Converts UTF-8 → MacRoman, \n → \r                                */
/* ------------------------------------------------------------------ */
static void DrainPendingText()
{
    if (gSSE.len <= 0) return;

    static char macBuf[8192];
    int  out = 0;
    int  in  = 0;
    int  rem = gSSE.len;

    while (in < rem && out < (int)sizeof(macBuf) - 2) {
        unsigned char c = (unsigned char)gSSE.text[in];
        if (c == '\n') {
            macBuf[out++] = '\r';   /* Mac line ending */
            in++;
            continue;
        }
        if (c == '\r') { in++; continue; } /* skip bare CR */

        if (c < 0x80) {
            macBuf[out++] = (char)c;
            in++;
        } else {
            /* Hold back an incomplete UTF-8 sequence at the buffer end —
               the rest of it arrives in the next SSE chunk. */
            int need = (c < 0xC0) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
            if (rem - in < need) break;
            unsigned long cp;
            int skip = ReadUTF8(gSSE.text + in, rem - in, &cp);
            unsigned char mr = ToMacRoman(cp);
            if (mr) macBuf[out++] = (char)mr;
            in += skip;
        }
    }

    if (out > 0) ChatLog(macBuf, out, false);

    /* Keep any unconsumed tail (partial UTF-8 sequence or macBuf full) */
    if (in < rem) {
        memmove(gSSE.text, gSSE.text + in, (size_t)(rem - in));
        gSSE.len = rem - in;
    } else {
        gSSE.len = 0;
    }
}

/* ------------------------------------------------------------------ */
/* OnStreamDone — called when SSE stream completes                    */
/* ------------------------------------------------------------------ */
static void OnStreamDone()
{
    /* Drop any held-back partial UTF-8 tail — no more chunks are coming,
       and a stale gSSE.len > 0 would keep the event loop spinning. */
    gSSE.len = 0;
    /* Drain error message if API client wrote one */
    if (gSSE.error_text[0]) {
        ChatLog(gSSE.error_text, (int)strlen(gSSE.error_text), false);
        ChatLog("\r", 1, false);
        gSSE.error_text[0] = '\0';
    }
    /* Append final newline after the response */
    ChatLog("\r", 1, false);
    gAppState.streaming = false;
    DebugLog("stream", "done");
    /* Reset TE geometry (destRect.bottom drifts from accumulated TEScroll
       calls during streaming) and redraw so input field stays visible. */
    ResizeChatControls();
    DrawChatWindow();
}

/* ------------------------------------------------------------------ */
/* IdleTicketRefresh — proactive TLS ticket refresh during idle        */
/* Server tickets live ~24h. If ours is >20h old and the user has been */
/* idle ≥30s, re-handshake now so the next message doesn't eat a       */
/* stale-reject + full handshake on slow hardware.                     */
/* ------------------------------------------------------------------ */
static long gLastEventTick = 0;

static void IdleTicketRefresh()
{
    static long sLastAttemptTick = 0;
    if (gAppState.streaming) return;
    long now = TickCount();
    if (now - gLastEventTick   < 30L * 60L) return;       /* user idle < 30 s        */
    if (sLastAttemptTick && now - sLastAttemptTick < 5L * 60L * 60L) return; /* ≥5 min between tries */
    if (gHttp.TicketAgeSeconds() < 20L * 3600L) return;   /* fresh, or none (-1)     */
    sLastAttemptTick = now;
    DebugLog("ticket", "idle refresh (>20h old)");
    gHttp.ForceReconnect();   /* force a real handshake, not an Idle no-op */
    if (!gHttp.WarmUp(APP_HOSTNAME) && gHttp.ResumeRejected())
        gHttp.WarmUp(APP_HOSTNAME);
}

/* ------------------------------------------------------------------ */
/* StreamYield — yield hook during blocking HTTP ops                  */
/* The main event loop is blocked inside ApiSend→PostStream while a   */
/* response streams, so cancel keys must be caught here. Esc or Cmd-. */
/* aborts the stream; other pending keys are discarded (the net layer */
/* would swallow them anyway).                                        */
/* ------------------------------------------------------------------ */
static void StreamYield()
{
    ChatStreamBlinkUpdate();
    if (!gAppState.streaming) return;

    EventRecord e;
    while (GetNextEvent(keyDownMask, &e)) {
        char c = (char)(e.message & charCodeMask);
        if (c == 0x1B || ((e.modifiers & cmdKey) && c == '.')) {
            gHttp.CancelStream();
            /* onComplete is skipped on cancel, so finish the stream here:
               OnStreamDone() drains error_text and resets the UI. */
            snprintf(gSSE.error_text, sizeof(gSSE.error_text), "[canceled]");
            gSSE.done = true;
            DebugLog("stream", "canceled by user");
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */
extern void LogBuildTime(void); /* defined in build_stamp.cpp, always recompiled */

int main()
{
    InitApp();

    /* Load settings */
    LoadSettings(gSettings);
    gDebugMode = gSettings.debugMode != 0;

    /* First-run: show settings dialog only if no API key found */
    if (!gSettings.apiKey[0]) {
        ShowSettingsDialog(gSettings);
    }

    /* Register yield hook — keeps stream blinker alive during TLS I/O */
    gHttp.SetYieldHook(StreamYield);  /* blink indicator + Esc/Cmd-. cancel */

    /* Set up menus */
    SetupMenus();

    /* Set warmingUp before ChatWindowCreate so its initial draw shows
       "Connecting…" immediately — no separate redraw needed. */
    if (gSettings.apiKey[0]) {
        gAppState.warmingUp = true;
    }

    /* Create combined chat window */
    ChatWindowCreate();
    LogBuildTime();

    /* Warm up TLS connection if we have an API key */
    if (gAppState.warmingUp) {
        DebugLog("WarmUp", "start");
        gHttp.SetAuthorization(gSettings.apiKey);
        if (!gHttp.WarmUp(APP_HOSTNAME) && gHttp.ResumeRejected()) {
            /* Server rejected a stale session ticket (normal after Cloudflare
               key rotation; the handshake already cleared it). Retry once —
               this time with a full handshake, which is what warmup exists
               to absorb. Genuine network failures (DNS/TCP timeout) skip the
               retry; PostStream reconnects from the Dead state on the first
               message anyway. */
            DebugLog("WarmUp", "retry full handshake");
            gHttp.WarmUp(APP_HOSTNAME);
        }
        gAppState.warmingUp = false;
        DrawChatWindow();   /* reveal input field after connect */
    }

    /* ---------------------------------------------------------------- */
    /* Main event loop                                                   */
    /* ---------------------------------------------------------------- */
    for (;;) {
        /* Blink cursor in input TE */
        ChatIdle();
        GrowZoneIdle();  /* log + re-arm memory reserve if it was consumed */

        /* Process HTTP/TLS thread */
        gHttp.ProcessRequests();
        ChatStreamBlinkUpdate();   /* tick-based blink update after HTTP returns */

        /* Drain SSE text into chat output */
        if (gSSE.len > 0) DrainPendingText();

        if (gSSE.done && gAppState.streaming) {
            gSSE.done = false;
            OnStreamDone();
        }

        /* sleep=0 keeps the loop spinning while text is arriving or streaming;
           sleep=4 ticks yields to other apps during idle. */
        EventRecord e;
        long sleep = gSSE.len > 0 || gAppState.streaming ? 0L : 4L;
        bool gotEvent = WaitNextEvent(everyEvent, &e, sleep, nil);
        if (!gotEvent) {
            IdleTicketRefresh();
            continue;
        }
        gLastEventTick = TickCount();

        switch (e.what) {
            case updateEvt:
                HandleUpdate(e);
                break;
            case mouseDown:
                HandleMouseDown(e);
                break;
            case keyDown:
            case autoKey:
                HandleKeyDown(e);
                break;
            case activateEvt:
                HandleActivate(e);
                break;
            case osEvt:
                if (((e.message >> 24) & 0xFF) == suspendResumeMessage)
                    HandleSuspendResume(e);
                break;
        }
    }
    return 0;
}
