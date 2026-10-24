#pragma once
/* ═══════════════════════════════════════════════════════════════════════
   api_client.h  —  boilerplate interface for HTTPS streaming on 68k Mac
   ═══════════════════════════════════════════════════════════════════════

   To wire this app to a different HTTPS API, change only three things:

   1. src/config.h           — hostname, model, app name (one file, all defines)
   2. CMakeLists.txt         — project() name (top line)
   3. src/api/<yourclient>.cpp — replace claude_client.cpp with your implementation:
        · ApiSend()           build the request body, call gHttp.PostStream()
        · SSEHandleLine()     parse your API's SSE events, write to gSSE

   Everything else (MacHTTP TLS stack, event loop, chat window, menus)
   is generic and reusable as-is.
   ═══════════════════════════════════════════════════════════════════════ */

/* ── streaming buffer ─────────────────────────────────────────────────────
   Written by ApiSend / SSEHandleLine, drained by main.cpp's event loop.
   68k Mac is single-core cooperative — no locking needed.
     text — UTF-8 text accumulated by SSEHandleLine across multiple chunks
     len  — bytes valid in text; reset to 0 after each drain
     done — set true by the HTTP onComplete callback (not by SSEHandleLine);
            signals the event loop that the full response has arrived       */
struct SSEBuffer {
    char text[8192];       /* UTF-8 response text, drained per event loop tick  */
    int  len;              /* bytes valid in text; reset to 0 after each drain   */
    bool done;             /* set true by HTTP onComplete callback               */
    char error_text[256];  /* non-empty = error; drained by OnStreamDone        */
};
extern SSEBuffer gSSE;

/* ── functions your API client must implement ─────────────────────────────
   ApiSend      — build the HTTP request and call gHttp.PostStream()
   SSEHandleLine — called per complete SSE line; parse and append to gSSE */
void ApiSend(const char* userText, const char* apiKey, const char* context);
void SSEHandleLine(const char* line, int len);

/* ── SSE line accumulator — provided by sse.cpp, do not change ────────────
   Pass each raw HTTP body chunk here; it splits on newlines and calls
   SSEHandleLine() for each complete line.                                 */
void SSEFeed(const char* at, int length);
