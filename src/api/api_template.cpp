/* api_template.cpp — Starter template for a new HTTPS streaming API.
   ════════════════════════════════════════════════════════════════════
   HOW TO USE:
     1. Copy this file to src/api/ with a new name (e.g. openai_client.cpp)
     2. Delete src/api/claude_client.cpp from the project
     3. Fill in the two functions below
     4. Update src/config.h with your API_ENDPOINT, API_MODEL, etc.
   ════════════════════════════════════════════════════════════════════

   The contract this file must fulfill (see src/core/api_client.h):
   ─────────────────────────────────────────────────────────────────
   · gSSE.text / gSSE.len — your SSEHandleLine appends text here;
     main.cpp drains it into the chat window each event loop tick.
   · gSSE.done  — set to true when the stream ends or on error.
   · ApiSend()       — called when the user sends a message.
   · SSEHandleLine() — called once per complete SSE line.
   ─────────────────────────────────────────────────────────────────

   SSE format reference:
     Each HTTP body chunk is split on newlines by core/sse.cpp and
     delivered here as a single null-terminated line.
     Typical SSE stream:
       event: eventname
       data: {"key":"value"}
       <blank line>
   ════════════════════════════════════════════════════════════════ */

#include "api_client.h"
#include "HttpClient.h"
#include "../config.h"
#include <string.h>
#include <stdio.h>

/* ── Shared streaming buffer — define it once, here ─────────────── */
SSEBuffer gSSE = {};

extern class HttpClient gHttp;   /* defined in main.cpp */

/* ── SSEHandleLine ───────────────────────────────────────────────── */
/* Called for every complete SSE line. Parse your API's event format
   and append response text to gSSE.

   Example for OpenAI (gpt-4o):
     Lines look like:  data: {"choices":[{"delta":{"content":"Hi"}}]}
     End-of-stream:    data: [DONE]

   Example for Anthropic (Claude):
     Lines look like:  data: {"type":"content_block_delta","delta":{"type":"text_delta","text":"Hi"}}
     End-of-stream:    data: {"type":"message_stop"}
*/
void SSEHandleLine(const char* line, int len)
{
    /* Skip non-data lines */
    if (len < 6 || memcmp(line, "data: ", 6) != 0) return;
    const char* json = line + 6;

    /* ── REPLACE BELOW with your API's SSE parsing ─────────────── */

    /* End-of-stream signal */
    if (strstr(json, "\"message_stop\"") || strncmp(json, "[DONE]", 6) == 0) {
        gSSE.done = true;
        return;
    }

    /* Extract text field — change needle to match your API's key   */
    const char* p = strstr(json, "\"text\":\"");       /* Claude */
    /* const char* p = strstr(json, "\"content\":\""); */ /* OpenAI */
    if (!p) return;
    p += 8;   /* skip past `"text":"` — adjust offset for your key  */

    /* Copy until closing quote, handling JSON escapes.
       NOTE: \uXXXX escapes are skipped here for simplicity. If your
       API emits them (smart quotes, emoji), copy the full decoder
       from claude_client.cpp (HexVal/EncodeUTF8 + the 'u' branch). */
    while (*p && *p != '"' && gSSE.len < (int)sizeof(gSSE.text) - 2) {
        if (*p == '\\') {
            p++;
            if (!*p) break;   /* line ends in a lone backslash — stop */
            if      (*p == '"')  gSSE.text[gSSE.len++] = '"';
            else if (*p == '\\') gSSE.text[gSSE.len++] = '\\';
            else if (*p == 'n')  gSSE.text[gSSE.len++] = '\n';
            else if (*p == 'r')  { /* skip */ }
            else if (*p == 'u')  { /* \uXXXX — skip the 4 hex digits */
                int k;
                for (k = 0; k < 4 && p[1]; k++) p++;
            }
            else                 gSSE.text[gSSE.len++] = *p;
            p++;
        } else {
            gSSE.text[gSSE.len++] = *p++;
        }
    }
    gSSE.text[gSSE.len] = '\0';

    /* ── END of API-specific parsing ──────────────────────────── */
}

/* ── ApiSend ─────────────────────────────────────────────────────── */
/* Build the request body and start the streaming POST.
   apiKey  — the value the user typed in the Settings dialog.
   context — last ~1500 chars of the chat transcript (may be ""); pass
             it to your API however fits (system prompt, prior turn…). */
void ApiSend(const char* userText, const char* apiKey, const char* context)
{
    if (!userText || !userText[0]) return;

    /* JSON-escape userText (handles quotes, backslashes, newlines).
       NOTE: this only handles plain ASCII. If your app needs to send
       accented characters or symbols typed on a Mac keyboard, copy the
       full MacRoman → UTF-8 escape loop from claude_client.cpp instead. */
    static char escaped[2048];
    int  ei = 0;
    for (int i = 0, n = (int)strlen(userText);
         i < n && ei < (int)sizeof(escaped) - 8; i++)
    {
        char c = userText[i];
        if      (c == '"' || c == '\\') { escaped[ei++] = '\\'; escaped[ei++] = c; }
        else if (c == '\n')             { escaped[ei++] = '\\'; escaped[ei++] = 'n'; }
        else if (c != '\r')             { escaped[ei++] = c; }
    }
    escaped[ei] = '\0';

    /* Escape the conversation context the same way (optional — drop
       this block if your API doesn't take prior-turn context). */
    static char escapedCtx[2048];
    int ci = 0;
    if (context) {
        for (int i = 0, n = (int)strlen(context);
             i < n && ci < (int)sizeof(escapedCtx) - 8; i++)
        {
            char c = context[i];
            if      (c == '"' || c == '\\') { escapedCtx[ci++] = '\\'; escapedCtx[ci++] = c; }
            else if (c == '\n' || c == '\r'){ escapedCtx[ci++] = '\\'; escapedCtx[ci++] = 'n'; }
            else                            { escapedCtx[ci++] = c; }
        }
    }
    escapedCtx[ci] = '\0';

    /* ── REPLACE BELOW with your API's request JSON format ──────── */
    static char body[8192];
    int bp = snprintf(body, sizeof(body),
        "{"
          "\"model\":\"" API_MODEL "\","
          "\"max_tokens\":%d,"
          "\"stream\":true,"
          "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]"
        "}",
        API_MAX_TOKENS, escaped);
    /* snprintf returns the *wanted* length — clamp before reusing bp
       in further snprintf(body + bp, sizeof(body) - bp, …) calls. */
    if (bp >= (int)sizeof(body)) bp = (int)sizeof(body) - 1;
    (void)bp; (void)escapedCtx;
    /* ── END of API-specific request format ─────────────────────── */

    gSSE.len           = 0;
    gSSE.done          = false;
    gSSE.error_text[0] = '\0';

    gHttp.SetAuthorization(apiKey);
    gHttp.SetExtraHeaders(API_EXTRA_HEADERS);   /* e.g. "anthropic-version: …\r\n" or "" */
    gHttp.PostStream(API_ENDPOINT, body,
        [](const char* at, int len2) { SSEFeed(at, len2); },
        [](HttpResponse& r) {
            gSSE.done = true;
            if (!r.Success || r.StatusCode < 200 || r.StatusCode >= 300) {
                snprintf(gSSE.error_text, sizeof(gSSE.error_text),
                         "[Error: HTTP %u]", r.StatusCode);
            }
        });
}
