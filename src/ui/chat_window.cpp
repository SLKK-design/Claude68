/* chat_window.cpp — chat window: styled TE output + TE input + blinker.
   gChatOutputTE  — TEStylNew — word-wrap, text selection, bold runs.
   gChatInputTE   — TENew     — single-line message input.              */

#include "chat_window.h"
#include "../config.h"
#include "helpers.h"
#include "Settings.h"
#include "../main.h"
#include "api_client.h"
#include <Windows.h>
#include <Controls.h>
#include <Fonts.h>
#include <TextEdit.h>
#include <Quickdraw.h>
#include <Scrap.h>
#include <Memory.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Globals                                                             */
/* ------------------------------------------------------------------ */
WindowPtr  gChatWindow  = nil;
TEHandle   gChatInputTE = nil;

static TEHandle         gChatOutputTE       = nil;
static TEHandle         gFocusTE            = nil;  /* which TE currently has keyboard focus */
static ControlHandle    gChatScrollBar      = nil;
static ControlActionUPP gChatScrollUPP      = nil;
static bool             gChatHasPlaceholder = true;
static bool             gFirstMsg           = true;
static bool             gStyleBold          = false;  /* current TE style run state */
static short            gStyleSize          = 12;
static bool             sPendingStyleRecalc = false;  // §6 PERF: batch TECalText deferred until style changes
static Rect             gSendBtnRect        = {0,0,0,0};

static const short kChatInputH     = 52;   /* minimum input area height (chat bottom) */
static const short kWelcomeInputH  = 40;   /* minimum input box height on the welcome screen */
static const short kScrollBarWidth = 15;
static const short kChatPadX       = 30;
static const short kChatPadTop     = 8;
static const short kMaxInputLines  = 15;
static const short kMaxInputH      = 210; // §13 MEMORY: cap on dynamic input area height
static short       gInputH         = kChatInputH;
static short       gWelcomeInputH  = kWelcomeInputH;

// §13 MEMORY: single definition of the initial placeholder string
static const char kInputPlaceholder[] = APP_INPUT_PLACEHOLDER;

/* Streaming blink: 8-step bounce through 5 sprite frames (0→1→2→3→4→3→2→1→…) */
enum StreamBlinkState { kBlinkOff=0, kBlink1=1, kBlink2=2, kBlink3=3, kBlink4=4,
                        kBlink5=5, kBlink6=6, kBlink7=7, kBlink8=8 }; // §14 ARCH: named enum type
static StreamBlinkState gStreamBlink = kBlinkOff; // §14 ARCH: typed instead of int
static short gSentinelPos   = -1;   /* offset of \r+space sentinel in output TE; -1 = inactive */
static long  gDebugStartTick = 0;

static struct { SInt16 geneva, times, zapf, symbol; } gFont;

// §12 PERF: FreeMem cache — updated at most every 30 ticks (~500 ms)
static long sCachedFreeK   = 0;
static long sCacheFreeTick = 0;

/* 27×28 1-bit Claude asterisk — native size from header.png, no scaling.
   Big-endian rows; bit 31 = leftmost pixel; only bits 31..5 are used.  */
static const short kClaudeIconW = 27, kClaudeIconH = 28;
static const UInt32 kClaudeIconBits[28] = {
    0x00030000,
    0x00070000,
    0x0E070000,
    0x0F070C00,
    0x0F871C00,
    0x07C73C00,
    0x03E77800,
    0x71F77000,
    0x7CFBF0C0,
    0x7E7FE3C0,
    0x3FBFEFC0,
    0x0FFFFF00,
    0x03FFFC00,
    0x01FFF000,
    0x007FFFE0,
    0xFFFFFFE0,
    0x7FFFF800,
    0x007FFC00,
    0x01FFFF00,
    0x03BFF7C0,
    0x073779C0,
    0x0E773C00,
    0x0C671E00,
    0x00E70E00,
    0x01C70600,
    0x01C70000,
    0x01870000,
    0x00070000
};

/* 5-frame 12×11 sprite sheet — bounce animation for streaming indicator.
   rowBytes=2; bit 15 = leftmost pixel; 4 right-hand bits are padding.
   Bounce order: frames 0→1→2→3→4→3→2→1 (kBlink1…kBlink8).           */
static const UInt16 kAnimFrames[5][11] = {
    { 0x0000, 0x0000, 0x0000, 0x0000, 0x0200, 0x0700, 0x0200, 0x0000, 0x0000, 0x0000, 0x0000 }, /* 0 */
    { 0x0000, 0x0000, 0x0200, 0x0200, 0x0200, 0x1FC0, 0x0200, 0x0200, 0x0200, 0x0000, 0x0000 }, /* 1 */
    { 0x0000, 0x0200, 0x1240, 0x0A80, 0x0700, 0x3FE0, 0x0700, 0x0A80, 0x1240, 0x0200, 0x0000 }, /* 2 */
    { 0x0200, 0x2220, 0x1240, 0x0A80, 0x0700, 0x7FF0, 0x0700, 0x0A80, 0x1240, 0x2220, 0x0200 }, /* 3 */
    { 0x4710, 0x6230, 0x3260, 0x1AC0, 0x0F80, 0x0700, 0x0F80, 0x1AC0, 0x3260, 0x6230, 0x4710 }, /* 4 */
};


/* ------------------------------------------------------------------ */
/* Private helpers                                                     */
/* ------------------------------------------------------------------ */

static void ClearPlaceholder()
{
    if (!gChatHasPlaceholder) return;
    TESetSelect(0, 32767, gChatInputTE);
    TEDelete(gChatInputTE);
    gChatHasPlaceholder = false;
}

static void SwitchFocus(TEHandle newFocus)
{
    if (gFocusTE == newFocus) return;
    if (gFocusTE) TEDeactivate(gFocusTE);
    gFocusTE = newFocus;
    if (gFocusTE) TEActivate(gFocusTE);
}

/* Reference line height — Geneva 12 */
static short TELineH()
{
    TextFont(gFont.geneva); TextFace(0); TextSize(12);
    FontInfo fi; GetFontInfo(&fi);
    return (short)(fi.ascent + fi.descent + fi.leading + 1);
}

/* Actual content height in pixels using TEGetPoint on the last character.
   nLines * lineH overcounts because gap lines are only 4px, not lineH px. */
static short ChatMaxScrollPx()
{
    if (!gChatOutputTE) return 0;
    short teLen = (*gChatOutputTE)->teLength;
    short lineH = TELineH();
    short destTop = (*gChatOutputTE)->destRect.top;
    short viewH   = (*gChatOutputTE)->viewRect.bottom - (*gChatOutputTE)->viewRect.top;
    short contentBottom;
    if (teLen > 0) {
        Point endPt = TEGetPoint(teLen, gChatOutputTE);
        contentBottom = (short)(endPt.v + lineH);
    } else {
        contentBottom = (short)(destTop + lineH);
    }
    short maxPx = (short)(contentBottom - destTop - viewH);
    return maxPx < 0 ? 0 : maxPx;
}

static void UpdateChatScrollBar()
{
    if (!gChatScrollBar || !gChatOutputTE) return;
    short lineH     = TELineH();
    short maxScroll = ChatMaxScrollPx();
    short curScroll = (short)((*gChatOutputTE)->viewRect.top - (*gChatOutputTE)->destRect.top);
    if (curScroll < 0)         curScroll = 0;
    if (curScroll > maxScroll) curScroll = maxScroll;
    short maxVal = (short)(maxScroll / lineH);
    short curVal = (short)(curScroll / lineH);
    SetControlMaximum(gChatScrollBar, maxVal);
    SetControlValue(gChatScrollBar,   curVal);
}

static void AutoScrollChatToBottom()
{
    if (!gChatOutputTE) return;
    short maxScroll = ChatMaxScrollPx();
    short curScroll = (short)((*gChatOutputTE)->viewRect.top - (*gChatOutputTE)->destRect.top);
    if (maxScroll > curScroll)
        TEScroll(0, (short)(-(maxScroll - curScroll)), gChatOutputTE);
}

static pascal void ChatScrollAction(ControlHandle, short part)
{
    if (!gChatOutputTE) return;
    short lineH     = TELineH();
    short viewH     = (*gChatOutputTE)->viewRect.bottom - (*gChatOutputTE)->viewRect.top;
    short visLines  = (short)(viewH / lineH);
    short maxScroll = ChatMaxScrollPx();
    short curScroll = (short)((*gChatOutputTE)->viewRect.top - (*gChatOutputTE)->destRect.top);
    short delta     = (short)(ScrollPartDelta(part, 1, visLines) * lineH);
    short newScroll = (short)(curScroll + delta);
    if (newScroll < 0)         newScroll = 0;
    if (newScroll > maxScroll) newScroll = maxScroll;
    short dv = (short)(newScroll - curScroll);
    if (dv == 0) return;
    TEScroll(0, (short)(-dv), gChatOutputTE);  /* shifts pixels + redraws exposed strip */
    UpdateChatScrollBar();
    GrafPtr sv; GetPort(&sv); SetPort(gChatWindow);
    DrawControls(gChatWindow);
    SetPort(sv);
}

static void DrawStreamIndicator();   /* forward declaration */

/* ------------------------------------------------------------------ */
/* DrawInputArea — rounded container + Send button + reposition TE    */
/* ------------------------------------------------------------------ */
static void DrawInputArea(const Rect& ctr)
{
    FrameRoundRect(&ctr, 14, 14);

    short btnH = (short)(ctr.bottom - ctr.top - 6);
    SetRect(&gSendBtnRect,
        (short)(ctr.right - 62), (short)(ctr.top  + 3),
        (short)(ctr.right -  4), (short)(ctr.top  + 3 + btnH));
    PaintRoundRect(&gSendBtnRect, 10, 10);

    TextFont(0); TextFace(0); TextSize(12);
    FontInfo bfi; GetFontInfo(&bfi);
    const char* lbl = "Send";
    short lw = TextWidth(lbl, 0, 4);
    short lx = (short)(gSendBtnRect.left + (gSendBtnRect.right  - gSendBtnRect.left - lw) / 2);
    short ly = (short)(gSendBtnRect.top  + (gSendBtnRect.bottom - gSendBtnRect.top + bfi.ascent - bfi.descent) / 2);
    TextMode(srcXor); MoveTo(lx, ly); DrawText(lbl, 0, 4); TextMode(srcOr);

    if (gChatInputTE) {
        short inLeft  = (short)(ctr.left  + 10);
        short inRight = (short)(gSendBtnRect.left - 4);
        /* Vertically center content — equal padding above and below */
        short teLineH  = (*gChatInputTE)->lineHeight;
        if (teLineH < 10) teLineH = 14;
        short teLines  = (*gChatInputTE)->nLines;
        if (teLines < 1) teLines = 1;
        short contentH = (short)(teLines * teLineH);
        short availH   = (short)(ctr.bottom - ctr.top);
        short vPad     = (short)((availH - contentH) / 2);
        if (vPad < 3) vPad = 3;
        short inTop    = (short)(ctr.top    + vPad);
        short inBot    = (short)(ctr.bottom - vPad);
        SetRect(&(*gChatInputTE)->viewRect, inLeft, inTop, inRight, inBot);
        SetRect(&(*gChatInputTE)->destRect, inLeft, inTop, inRight, 32767);
        TEUpdate(&gChatWindow->portRect, gChatInputTE);
    }
}

/* ------------------------------------------------------------------ */
/* DrawChatWindow                                                      */
/* ------------------------------------------------------------------ */
void DrawChatWindow()
{
    if (!gChatWindow) return;
    GrafPtr savedPort; GetPort(&savedPort);
    SetPort(gChatWindow);
    ClipRect(&gChatWindow->portRect);   /* always reset clip so nothing bleeds */

    short lineH    = TELineH();         // §8 PERF: compute once; reuse throughout this function
    (void)lineH;                        /* suppress unused-variable warning when not referenced below */
    short winRight = gChatWindow->portRect.right;
    short winBot   = gChatWindow->portRect.bottom;

    EraseRect(&gChatWindow->portRect);

    if (gFirstMsg) {
        /* Welcome state: centered title + input */
        HideControl(gChatScrollBar);

        TextFont(gFont.times); TextFace(0); TextSize(30);
        FontInfo tfi; GetFontInfo(&tfi);
        short titleH   = (short)(tfi.ascent + tfi.descent);
        short gap      = 18;

        /* Input: vertically centered in the window */
        short inputTop = (short)((winBot - gWelcomeInputH) / 2);
        short minInput = (short)(titleH + gap + 10);
        if (inputTop < minInput) inputTop = minInput;

        /* Title: 2/3 of the way from window top to input top */
        short groupTop = (short)(inputTop * 2 / 3 - 15);
        if (groupTop < 10) groupTop = 10;

        /* Title: [asterisk icon]  APP_NAME */
        static const char kTitleText[] = APP_NAME;
        short wTitle = TextWidth(kTitleText, 0, (short)(sizeof(kTitleText) - 1));
        short kGap   = 6;
        short totalW = (short)(kClaudeIconW + kGap + wTitle);
        short cx  = (short)(winRight / 2 - totalW / 2);
        short bly = (short)(groupTop + tfi.ascent);
        short iconTop = (short)(groupTop + (titleH - kClaudeIconH) / 2);

        /* 1. Asterisk bitmap — 1:1, no scaling */
        {
            BitMap bm;
            bm.baseAddr = (Ptr)kClaudeIconBits;
            bm.rowBytes = 4;
            SetRect(&bm.bounds, 0, 0, kClaudeIconW, kClaudeIconH);
            Rect srcR; SetRect(&srcR, 0, 0, kClaudeIconW, kClaudeIconH);
            Rect dstR; SetRect(&dstR, cx, iconTop,
                               (short)(cx + kClaudeIconW), (short)(iconTop + kClaudeIconH));
            CopyBits(&bm, &gChatWindow->portBits, &srcR, &dstR, srcCopy, nil);
        }
        /* 2. APP_NAME */
        MoveTo((short)(cx + kClaudeIconW + kGap), bly);
        DrawText(kTitleText, 0, (short)(sizeof(kTitleText) - 1));

        if (gAppState.warmingUp) {
            /* Plain status text — no border, Geneva 12, centered where input would be */
            static const char kMsg[] = APP_WARMUP_MSG;
            TextFont(gFont.geneva); TextFace(0); TextSize(12);
            FontInfo sfi; GetFontInfo(&sfi);
            short tw = TextWidth(kMsg, 0, (short)(sizeof(kMsg) - 1));
            short tx = (short)(winRight / 2 - tw / 2);
            short ty = (short)(inputTop + (gWelcomeInputH + sfi.ascent) / 2);
            MoveTo(tx, ty);
            DrawText(kMsg, 0, (short)(sizeof(kMsg) - 1));
        } else {
            Rect ctr;
            SetRect(&ctr, kChatPadX, inputTop,
                    (short)(winRight - 20), (short)(inputTop + gWelcomeInputH));
            DrawInputArea(ctr);
        }

    } else {
        /* Chat state: output TE + blinker + input at bottom */
        ShowControl(gChatScrollBar);
        short sepY = (short)(winBot - gInputH);

        if (gChatOutputTE)
            TEUpdate(&gChatWindow->portRect, gChatOutputTE);

        DrawStreamIndicator();

        Rect ctr;
        SetRect(&ctr, kChatPadX, (short)(sepY + 6), (short)(winRight - 20), (short)(winBot - 14));
        DrawInputArea(ctr);
    }

    UpdateChatScrollBar();
    DrawControls(gChatWindow);
    SetPort(savedPort);
}

/* ------------------------------------------------------------------ */
/* AppendToOutput — insert text into output TE with given style       */
/* ------------------------------------------------------------------ */
/* isBold=false, fontSize=12 → inherits initial style, no TESetStyle call needed.
   Any other combination applies a style run with redraw=true so TECalText reflows. */
static void AppendToOutput(const char* text, int len, bool isBold, short fontSize)
{
    if (!gChatOutputTE || len <= 0) return;
    GrafPtr sv; GetPort(&sv); SetPort(gChatWindow);
    ClipRect(&(*gChatOutputTE)->viewRect);   /* keep TE drawing inside output area */
    /* TE hard limit is 32767 chars — trim the oldest 4 KB when close so a
       long session never overflows the short-based TE record arithmetic. */
    if ((long)(*gChatOutputTE)->teLength + len > 30000) {
        TESetSelect(0, 4096, gChatOutputTE);
        TEDelete(gChatOutputTE);
        if (gSentinelPos >= 0)
            gSentinelPos = (short)(gSentinelPos > 4096 ? gSentinelPos - 4096 : 0);
    }
    /* While streaming, insert before the sentinel space so it stays at the
       end of the response line (inline blinker). Otherwise append normally. */
    short start = (gSentinelPos >= 0)
        ? gSentinelPos                       /* before the trailing space */
        : (*gChatOutputTE)->teLength;
    TESetSelect(start, start, gChatOutputTE);
    TEInsert(text, len, gChatOutputTE);
    short end = (short)(start + len);
    if (gSentinelPos >= 0) gSentinelPos += len;   /* sentinel shifted forward
                                                     (>= 0 matches the insert-
                                                     position test above) */
    /* Only call TESetStyle when style actually changes — streaming chunks
       (Geneva 12 plain) skip this entirely, keeping the main loop fast.
       TESetStyle(false) defers reflow; flush pending recalc if style changes. */
    if (isBold != gStyleBold || fontSize != gStyleSize) {
        if (sPendingStyleRecalc) {                          // §6 PERF: flush deferred recalc before style switch
            TECalText(gChatOutputTE);
            sPendingStyleRecalc = false;
        }
        TextStyle ts; ts.tsFont = gFont.geneva; ts.tsFace = isBold ? bold : 0; ts.tsSize = fontSize;
        TESetSelect(start, end, gChatOutputTE);
        TESetStyle(doFont | doFace | doSize, &ts, false, gChatOutputTE);
        TECalText(gChatOutputTE);                           // §6 PERF: mandatory reflow after style change
        gStyleBold = isBold;
        gStyleSize = fontSize;
    } else {
        sPendingStyleRecalc = true;                         // §6 PERF: defer TECalText — same style, batch it
    }
    TESetSelect(end, end, gChatOutputTE);
    AutoScrollChatToBottom();
    UpdateChatScrollBar();
    ClipRect(&gChatWindow->portRect);        /* restore full clip */
    SetPort(sv);
}

/* ------------------------------------------------------------------ */
/* ChatLog — append chat text (isBold=true for user messages)         */
/* ------------------------------------------------------------------ */
void ChatLog(const char* text, int len, bool isBold)
{
    AppendToOutput(text, len, isBold, 12);
    if (gChatWindow && gChatOutputTE) {
        GrafPtr sv; GetPort(&sv); SetPort(gChatWindow);
        Rect vr = (*gChatOutputTE)->viewRect;
        ClipRect(&vr);                       /* clip to output area only */
        TEUpdate(&vr, gChatOutputTE);
        ClipRect(&gChatWindow->portRect);    /* restore */
        SetPort(sv);
    }
}

/* ------------------------------------------------------------------ */
/* DebugLog — append debug line (only when gDebugMode)                */
/* ------------------------------------------------------------------ */
void DebugLog(const char* func, const char* msg)
{
    if (!gDebugMode) return;
    long now = TickCount();
    if (gDebugStartTick == 0) gDebugStartTick = now;
    long elapsed10 = (now - gDebugStartTick) * 10L / 60L;
    // §12 PERF: refresh FreeMem cache at most every 30 ticks (~500 ms)
    if (now - sCacheFreeTick > 30 || sCacheFreeTick == 0) {
        sCachedFreeK   = FreeMem() / 1024L;
        sCacheFreeTick = now;
    }
    long freeK = sCachedFreeK;
    char line[80];
    snprintf(line, sizeof(line), "%4ldK %4ld.%lds %-11.11s %s",
             freeK, elapsed10 / 10L, elapsed10 % 10L, func, msg);
    line[79] = '\0';
    ChatLog(line, (int)strlen(line), false);
    ChatLog("\r", 1, false);
}

/* ------------------------------------------------------------------ */
/* HandleChatSend — read input, append to output, call API            */
/* ------------------------------------------------------------------ */
static void HandleChatSend()
{
    if (!gChatInputTE) return;
    if (gChatHasPlaceholder) return;   /* never send placeholder text */
    int len = (*gChatInputTE)->teLength;
    if (len <= 0) return;

    HLock((*gChatInputTE)->hText);
    char* raw = *((*gChatInputTE)->hText);
    char  inputBuf[512];
    int   copyLen = len < (int)sizeof(inputBuf) - 1 ? len : (int)sizeof(inputBuf) - 1;
    memcpy(inputBuf, raw, copyLen);
    inputBuf[copyLen] = '\0';
    HUnlock((*gChatInputTE)->hText);

    /* Capture last 1500 chars of output TE as conversation context */
    char context[1536] = {};
    if (gChatOutputTE) {
        short teLen    = (*gChatOutputTE)->teLength;
        short ctxStart = (short)(teLen > 1500 ? teLen - 1500 : 0);
        short ctxLen   = (short)(teLen - ctxStart);
        if (ctxLen > 0) {
            HLock((*gChatOutputTE)->hText);
            memcpy(context, *((*gChatOutputTE)->hText) + ctxStart,
                   ctxLen < (short)(sizeof(context) - 1) ? ctxLen : (short)(sizeof(context) - 1));
            HUnlock((*gChatOutputTE)->hText);
        }
    }

    TESetSelect(0, 32767, gChatInputTE);
    TEDelete(gChatInputTE);
    gInputH = kChatInputH;  /* reset input area to minimum height */
    const char kReplyPlaceholder[] = "Write a message\xC9";
    TEInsert(kReplyPlaceholder, (int)sizeof(kReplyPlaceholder) - 1, gChatInputTE);
    gChatHasPlaceholder = true;

    /* 12px blank row — before every question, including the first */
    ChatLog("\r", 1, false);
    gFirstMsg = false;

    /* Bold question with prompt prefix; trailing \r is plain so the
       response inherits plain style (insertion takes style from char to its left). */
    ChatLog("> ", 2, true);
    ChatLog(inputBuf, copyLen, true);
    ChatLog("\r", 1, false);

    /* Sentinel space anchors the blinker; response text inserts before it. */
    AppendToOutput(" ", 1, false, 12);
    gSentinelPos = (short)((*gChatOutputTE)->teLength - 1);

    /* Redraw then show blinker before the blocking HTTP call */
    {
        GrafPtr sv; GetPort(&sv); SetPort(gChatWindow);
        DrawChatWindow();
        SetPort(sv);
    }
    gAppState.streaming = true;
    gStreamBlink        = kBlink1;
    {
        GrafPtr sv; GetPort(&sv); SetPort(gChatWindow);
        DrawStreamIndicator();
        SetPort(sv);
    }

    ApiSend(inputBuf, gSettings.apiKey, context);
}

/* ------------------------------------------------------------------ */
/* CopyChatToClipboard — copy all output text to clipboard            */
/* ------------------------------------------------------------------ */
void CopyChatToClipboard()
{
    if (!gChatOutputTE || !gChatWindow) return;
    GrafPtr sv; GetPort(&sv);
    SetPort(gChatWindow);
    Rect clip; SetRect(&clip, 0, 0, 0, 0);  /* empty clip — suppress TE drawing */
    ClipRect(&clip);
    short end = (*gChatOutputTE)->teLength;
    TESetSelect(0, end, gChatOutputTE);
    TECopy(gChatOutputTE);
    ZeroScrap(); TEToScrap();
    TESetSelect(end, end, gChatOutputTE);
    ClipRect(&gChatWindow->portRect);
    SetPort(sv);
}

/* ------------------------------------------------------------------ */
/* ResizeChatControls — reposition controls after window resize       */
/* ------------------------------------------------------------------ */
void ResizeChatControls()
{
    if (!gChatWindow) return;
    short sepY = (short)(gChatWindow->portRect.bottom - gInputH);

    if (gChatScrollBar) {
        MoveControl(gChatScrollBar,
            (short)(gChatWindow->portRect.right - kScrollBarWidth),
            (short)(gChatWindow->portRect.top  - 1));
        SizeControl(gChatScrollBar,
            (short)(kScrollBarWidth + 2),
            (short)(sepY - gChatWindow->portRect.top + 3));
    }

    if (gChatOutputTE) {
        short outRight  = (short)(gChatWindow->portRect.right - kScrollBarWidth - 4);
        short curScroll = (short)((*gChatOutputTE)->viewRect.top - (*gChatOutputTE)->destRect.top);
        Rect vr; SetRect(&vr, kChatPadX, kChatPadTop, outRight, (short)(sepY - 4));
        Rect dr; SetRect(&dr, kChatPadX, (short)(kChatPadTop - curScroll), outRight, 32767);
        (*gChatOutputTE)->viewRect = vr;
        (*gChatOutputTE)->destRect = dr;
        (*gChatOutputTE)->destRect.bottom = 32767; // §11 MEMORY: reset after TEScroll accumulation to prevent drift
        TECalText(gChatOutputTE);
    }
}

/* ------------------------------------------------------------------ */
/* ChatWindowCreate — open window once at startup                     */
/* ------------------------------------------------------------------ */
void ChatWindowCreate()
{
    if (gChatWindow) return;

    GetFNum((ConstStr255Param)"\pGeneva",        &gFont.geneva);
    GetFNum((ConstStr255Param)"\pTimes",         &gFont.times);
    GetFNum((ConstStr255Param)"\pZapf Dingbats", &gFont.zapf);
    GetFNum((ConstStr255Param)"\pSymbol",        &gFont.symbol);

    short scrW = qd.screenBits.bounds.right;
    short scrH = qd.screenBits.bounds.bottom;
    short winW = (short)(scrW * 2 / 3);
    short winH = (short)(scrH * 2 / 3);
    short left = (short)((scrW - winW) / 2);
    short top  = (short)((scrH - winH) / 2);
    if (top < 44) top = 44;
    Rect r; SetRect(&r, left, top, (short)(left + winW), (short)(top + winH));
    gChatWindow = NewWindow(nil, &r, APP_NAME_PSTR, true, zoomDocProc, (WindowPtr)-1, true, 0L);
    SetPort(gChatWindow);

    short sepY = (short)(gChatWindow->portRect.bottom - gInputH);

    /* Scrollbar */
    Rect sb;
    SetRect(&sb,
        (short)(gChatWindow->portRect.right - kScrollBarWidth), (short)(gChatWindow->portRect.top - 1),
        (short)(gChatWindow->portRect.right + 1),               (short)(sepY + 1));
    gChatScrollBar = MakeScrollBar(gChatWindow, sb);
    gChatScrollUPP = NewControlActionUPP(ChatScrollAction);

    /* Output TE — styled, for word-wrap + bold + variable font size.
       TEStyleNew reads its initial style run from the port at creation time,
       so set port font BEFORE calling it — not after.                    */
    {
        short outRight = (short)(gChatWindow->portRect.right - kScrollBarWidth - 4);
        Rect outR; SetRect(&outR, kChatPadX, kChatPadTop, outRight, (short)(sepY - 4));
        Rect destR; SetRect(&destR, kChatPadX, kChatPadTop, outRight, 32767);
        TextFont(gFont.geneva); TextFace(0); TextSize(12);   /* prime port first */
        gChatOutputTE = TEStyleNew(&outR, &destR);
    }

    /* Input TE */
    {
        short ctrTop  = (short)(sepY + 6);
        short ctrBot  = (short)(gChatWindow->portRect.bottom - 14);
        short inLeft  = (short)(kChatPadX + 10);
        short inRight = (short)(gChatWindow->portRect.right - 20 - 52 - 8);
        Rect inR;    SetRect(&inR,    inLeft, (short)(ctrTop + 5), inRight, (short)(ctrBot - 5));
        Rect destR;  SetRect(&destR,  inLeft, (short)(ctrTop + 5), inRight, 32767);
        gChatInputTE = TENew(&inR, &destR);
        if (gChatInputTE) {
            (*gChatInputTE)->txFont = gFont.geneva;
            (*gChatInputTE)->txSize = 12;
            TEInsert(kInputPlaceholder, (int)sizeof(kInputPlaceholder) - 1, gChatInputTE);
            gChatHasPlaceholder = true;
            TEActivate(gChatInputTE);
            gFocusTE = gChatInputTE;
        }
    }

    DrawChatWindow();
}

/* ------------------------------------------------------------------ */
/* HandleChatClick                                                     */
/* ------------------------------------------------------------------ */
void HandleChatClick(WindowPtr win, Point lp)
{
    if (!gChatWindow || win != gChatWindow) return;
    SetPort(gChatWindow);

    /* Scrollbar */
    ControlHandle ctl; short part = FindControl(lp, gChatWindow, &ctl);
    if (ctl == gChatScrollBar && part) {
        if (part == kControlIndicatorPart) {
            /* Thumb drag — no action proc, apply delta on release */
            short oldVal = GetControlValue(gChatScrollBar);
            TrackControl(gChatScrollBar, lp, nil);
            short newVal = GetControlValue(gChatScrollBar);
            short delta  = (short)((newVal - oldVal) * TELineH());
            if (delta != 0 && gChatOutputTE) {
                TEScroll(0, (short)(-delta), gChatOutputTE);  /* shifts pixels + redraws exposed strip */
                UpdateChatScrollBar();
                DrawControls(gChatWindow);
            }
        } else {
            /* Arrows / pages — ChatScrollAction handles all scrolling */
            TrackControl(gChatScrollBar, lp, gChatScrollUPP);
        }
        return;
    }

    /* Send button */
    if (PtInRect(lp, &gSendBtnRect)) {
        InvertRoundRect(&gSendBtnRect, 10, 10);
        while (StillDown()) { /* wait */ }
        Point cur; GetMouse(&cur);
        InvertRoundRect(&gSendBtnRect, 10, 10);
        if (PtInRect(cur, &gSendBtnRect)) HandleChatSend();
        return;
    }

    /* Click in output TE — text selection */
    if (gChatOutputTE) {
        Rect outR = (*gChatOutputTE)->viewRect;
        if (PtInRect(lp, &outR) && (*gChatOutputTE)->teLength > 0) {
            SwitchFocus(gChatOutputTE);
            TEClick(lp, false, gChatOutputTE);
            while (StillDown()) {
                Point cur; GetMouse(&cur);
                TEClick(cur, true, gChatOutputTE);
            }
            TEUpdate(&outR, gChatOutputTE);
            /* No text selected — single click — return focus to input */
            if ((*gChatOutputTE)->selStart == (*gChatOutputTE)->selEnd)
                SwitchFocus(gChatInputTE);
            return;
        }
    }

    /* Click in input TE */
    if (gChatInputTE) {
        Rect inR = (*gChatInputTE)->viewRect;
        if (PtInRect(lp, &inR)) {
            SwitchFocus(gChatInputTE);
            ClearPlaceholder();
            TEClick(lp, false, gChatInputTE);
            while (StillDown()) {
                Point cur; GetMouse(&cur);
                TEClick(cur, true, gChatInputTE);
            }
            Rect r = (*gChatInputTE)->viewRect;
            TEUpdate(&r, gChatInputTE);
        }
    }
}

/* ------------------------------------------------------------------ */
/* UpdateChatInputLayout — grow/shrink input area as user types       */
/* Max kMaxInputLines lines; no scroll (use arrow keys for more)      */
/* ------------------------------------------------------------------ */
static void UpdateChatInputLayout()
{
    if (!gChatInputTE || !gChatWindow || gChatHasPlaceholder) return;
    short lineH  = (*gChatInputTE)->lineHeight;
    if (lineH < 10) lineH = 14;
    short nLines = (*gChatInputTE)->nLines;
    if (nLines < 1) nLines = 1;
    if (nLines > kMaxInputLines) nLines = kMaxInputLines;
    if (gFirstMsg) {
        /* Welcome: no outer container padding — grow from gWelcomeInputH by one lineH per extra line */
        short newH = (short)(kWelcomeInputH + (nLines - 1) * lineH);
        if (newH > kMaxInputH) newH = kMaxInputH;
        if (newH == gWelcomeInputH) return;
        gWelcomeInputH = newH;
    } else {
        short newH = (short)(nLines * lineH + 30);  /* 6 top-gap + vPad×2 + 14 bot-gap = 30 */
        if (newH < kChatInputH) newH = kChatInputH;
        if (newH > kMaxInputH)  newH = kMaxInputH;
        if (newH == gInputH) return;
        gInputH = newH;
        ResizeChatControls();
    }
    DrawChatWindow();
}

/* ------------------------------------------------------------------ */
/* HandleChatKey                                                       */
/* ------------------------------------------------------------------ */
void HandleChatKey(char c, short mods)
{
    if (!gChatInputTE) return;
    if (mods & cmdKey) return;

    if (c == 0x0D || c == 0x03) { HandleChatSend(); return; }

    SwitchFocus(gChatInputTE);
    ClearPlaceholder();

    TEKey(c, gChatInputTE);
    UpdateChatInputLayout();

    GrafPtr sv; GetPort(&sv); SetPort(gChatWindow);
    Rect r = (*gChatInputTE)->viewRect; InsetRect(&r, -1, -1);
    TEUpdate(&r, gChatInputTE);
    SetPort(sv);
}

/* ------------------------------------------------------------------ */
/* GetChatFocusTE / HandleChatActivate                                */
/* ------------------------------------------------------------------ */
TEHandle GetChatFocusTE()
{
    return gFocusTE ? gFocusTE : gChatInputTE;
}

void HandleChatActivate(bool activate)
{
    TEHandle te = gFocusTE ? gFocusTE : gChatInputTE;
    if (!te) return;
    if (activate) {
        TEActivate(te);
    } else {
        TEDeactivate(te);
    }
}

/* ------------------------------------------------------------------ */
/* DrawStreamIndicator — Zapf Dingbats / Symbol blinker               */
/* ------------------------------------------------------------------ */
static void DrawStreamIndicator()
{
    if (!gChatWindow || !gChatOutputTE) return;
    if (gStreamBlink == kBlinkOff) return;   /* nothing to draw — skip erase to protect text */
    SetPort(gChatWindow);

    static const int kBounce[8] = { 0, 1, 2, 3, 4, 3, 2, 1 };
    int frameIdx = kBounce[gStreamBlink - 1];

    if (gSentinelPos < 0) return;

    short viewTop = (*gChatOutputTE)->viewRect.top;
    short viewBot = (*gChatOutputTE)->viewRect.bottom;

    /* TEGetPoint on sentinel gives pixel-exact position that tracks the text */
    Point pt = TEGetPoint(gSentinelPos, gChatOutputTE);
    short y = (short)(pt.v - 9);
    short x = pt.h;
    if (y < viewTop)      y = viewTop;
    if (y > viewBot - 11) y = (short)(viewBot - 11);

    ClipRect(&(*gChatOutputTE)->viewRect);
    Rect dstR; SetRect(&dstR, x, y, (short)(x + 12), (short)(y + 11));
    EraseRect(&dstR);

    BitMap bm;
    bm.baseAddr = (Ptr)kAnimFrames[frameIdx];
    bm.rowBytes = 2;
    SetRect(&bm.bounds, 0, 0, 12, 11);
    Rect srcR; SetRect(&srcR, 0, 0, 12, 11);
    CopyBits(&bm, &gChatWindow->portBits, &srcR, &dstR, srcCopy, nil);
    ClipRect(&gChatWindow->portRect);
}

/* ------------------------------------------------------------------ */
/* ChatFlushPendingStyle — flush deferred TECalText if needed         */
/* ------------------------------------------------------------------ */
static void ChatFlushPendingStyle()   // §6 PERF: drain deferred TECalText once per event loop tick
{
    if (!sPendingStyleRecalc || !gChatOutputTE) return;
    TECalText(gChatOutputTE);
    sPendingStyleRecalc = false;
}

/* ------------------------------------------------------------------ */
/* ChatIdle — cursor blink for focused TE                             */
/* ------------------------------------------------------------------ */
void ChatIdle()
{
    ChatFlushPendingStyle();  // §6 PERF: flush any deferred TECalText each idle tick
    TEHandle te = gFocusTE ? gFocusTE : gChatInputTE;
    if (te && te != gChatOutputTE) TEIdle(te);  /* never blink cursor in output TE */
}

/* ------------------------------------------------------------------ */
/* ChatStreamBlinkUpdate — called from MacYield during HTTP ops       */
/* ------------------------------------------------------------------ */
void ChatStreamBlinkUpdate()
{
    if (gStreamBlink == kBlinkOff) return;

    static long sLastBlinkTick = 0;
    long now = TickCount();
    if (now - sLastBlinkTick < 6) return;   /* ~6 ticks = 100ms per frame */
    sLastBlinkTick = now;

    GrafPtr sv; GetPort(&sv);
    if (gAppState.streaming) {
        gStreamBlink = (StreamBlinkState)((gStreamBlink % kBlink8) + 1); // §14 ARCH: cast to typed enum
        SetPort(gChatWindow);
        DrawStreamIndicator();
    } else {
        gStreamBlink = kBlinkOff;
        SetPort(gChatWindow);
        /* Remove the trailing sentinel space now that streaming is done */
        if (gSentinelPos >= 0 && gChatOutputTE) {
            TESetSelect(gSentinelPos, (short)(gSentinelPos + 1), gChatOutputTE);
            TEDelete(gChatOutputTE);
            TESetSelect((*gChatOutputTE)->teLength, (*gChatOutputTE)->teLength, gChatOutputTE);
            gSentinelPos = -1;
        }
        DrawChatWindow();    /* full redraw: TEUpdate over-paints the indicator area */
    }
    SetPort(sv);
}

/* ------------------------------------------------------------------ */
/* C-callable bridges for net layer                                    */
/* ------------------------------------------------------------------ */
extern "C" void fast_alloc_log(const char* func, const char* msg) { DebugLog(func, msg); }
extern "C" void uecc_con_log  (const char* func, const char* msg) { DebugLog(func, msg); }
