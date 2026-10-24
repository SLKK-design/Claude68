/* chat_window.h — combined chat + debug console window. */
#pragma once
#include <Windows.h>
#include <TextEdit.h>
#include <Events.h>

extern WindowPtr gChatWindow;
extern TEHandle  gChatInputTE;

/* Open the combined chat window (call once at startup) */
void ChatWindowCreate();

/* Append chat text: bold=true for user messages, false for Claude output */
void ChatLog(const char* text, int len, bool bold);

/* Append debug line (only when gDebugMode is true) */
void DebugLog(const char* func, const char* msg);

/* Redraw window contents */
void DrawChatWindow();

/* Handle mouse click in window content area */
void HandleChatClick(WindowPtr win, Point localPt);

/* Handle key event routed to input TE */
void HandleChatKey(char c, short mods);

/* Returns the currently focused TE (output or input) — used by menu.cpp */
TEHandle GetChatFocusTE();

/* Activate / deactivate focused TE */
void HandleChatActivate(bool activate);

/* Blink the cursor in the focused TE — call every event loop iteration */
void ChatIdle();

/* Tick-based blink update — call from main loop after gHttp.ProcessRequests() */
void ChatStreamBlinkUpdate();

/* Reposition scrollbar / output TE / input TE / Send button.
   Call after window resize or after the input area grows/shrinks. */
void ResizeChatControls();

/* Copy all buffered lines to clipboard */
void CopyChatToClipboard();

/* Convenience wrappers over DebugLog — active only when gDebugMode is true.
   DEBUG_LOG  — literal string message.
   DEBUG_LOGF — printf-style formatted message (truncated to 79 chars). */
#define DEBUG_LOG(fn, msg)          DebugLog(fn, msg)
#define DEBUG_LOGF(fn, fmt, ...)    \
    do { char _pbuf[80]; snprintf(_pbuf, sizeof(_pbuf), fmt, ##__VA_ARGS__); \
         DebugLog(fn, _pbuf); } while(0)
