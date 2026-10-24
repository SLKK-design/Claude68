/* claude_client.cpp — Anthropic Claude API streaming implementation.
   ─────────────────────────────────────────────────────────────────────
   REPLACE THIS FILE to connect to a different API.
   Contract (declared in src/core/api_client.h):
     · ApiSend()       — build request, call gHttp.PostStream()
     · SSEHandleLine() — parse one SSE line, append text to gSSE
   ─────────────────────────────────────────────────────────────────── */

#include "api_client.h"
#include "HttpClient.h"
#include "../config.h"
#include "utf8_mac.h"

#include <string.h>
#include <stdio.h>

/* ── shared streaming buffer ─────────────────────────────────────── */
SSEBuffer gSSE = {};

/* ── forward declaration — gHttp lives in main.cpp ──────────────── */
class HttpClient;
extern HttpClient gHttp;

/* §5 PERF: file-scope hex table */
static const char kHex[] = "0123456789abcdef";

static int HexVal(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Encode a Unicode code point as UTF-8 (gSSE.text is UTF-8). */
static int EncodeUTF8(unsigned long cp, char out[4])
{
    if (cp < 0x80)    { out[0] = (char)cp; return 1; }
    if (cp < 0x800)   { out[0] = (char)(0xC0 | (cp >> 6));
                        out[1] = (char)(0x80 | (cp & 0x3F)); return 2; }
    if (cp < 0x10000) { out[0] = (char)(0xE0 | (cp >> 12));
                        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        out[2] = (char)(0x80 | (cp & 0x3F)); return 3; }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

/* ── JsonEscape — write src as JSON string content into dst ─────── */
static int JsonEscape(const char* src, char* dst, int dstMax)
{
    int di = 0;
    for (int i = 0; src[i] && di < dstMax - 8; i++) {
        unsigned char c = (unsigned char)src[i];
        if      (c == '"' || c == '\\') { dst[di++] = '\\'; dst[di++] = (char)c; }
        else if (c == '\n')             { dst[di++] = '\\'; dst[di++] = 'n'; }
        else if (c == '\t')             { dst[di++] = '\\'; dst[di++] = 't'; }
        else if (c == '\r')             { dst[di++] = '\\'; dst[di++] = 'n'; }   /* Mac CR → \n */
        else if (c < 0x20)             { dst[di++]='\\'; dst[di++]='u'; dst[di++]='0'; dst[di++]='0';
                                          dst[di++]=kHex[c>>4]; dst[di++]=kHex[c&0xF]; }
        else if (c < 0x80)             { dst[di++] = (char)c; }
        else                           { char u[4]; int n = MacRomanToUTF8(c, u);
                                         for (int k = 0; k < n; k++) dst[di++] = u[k]; }
    }
    dst[di] = '\0';
    return di;
}

/* ── SSEHandleLine — parse one SSE line and append text to gSSE ──── */
void SSEHandleLine(const char* line, int len)
{
    if (len < 6 || memcmp(line, "data: ", 6) != 0) return;
    const char* json = line + 6;

    /* message_stop ends the stream */
    if (strstr(json, "\"message_stop\"")) { gSSE.done = true; return; }

    /* Only process content delta events */
    if (!strstr(json, "\"content_block_delta\"")) return;

    /* Extract "text":"..." — scan for field, copy until closing quote */
    const char* p = strstr(json, "\"text\":\"");
    if (!p) return;
    p += 8;
    while (*p && *p != '"' && gSSE.len < (int)sizeof(gSSE.text) - 2) {
        char ch;
        if (*p == '\\') {
            p++;
            if (!*p) break;
            if      (*p == '"')  ch = '"';
            else if (*p == '\\') ch = '\\';
            else if (*p == 'n')  ch = '\n';
            else if (*p == 'r')  { p++; continue; }
            else if (*p == 'u')  {
                /* \uXXXX — decode to UTF-8; combine surrogate pairs */
                unsigned long cp = 0;
                int k;
                for (k = 1; k <= 4; k++) {
                    int v = HexVal(p[k]);
                    if (v < 0) break;
                    cp = (cp << 4) | (unsigned long)v;
                }
                if (k <= 4) { p++; continue; }   /* malformed — skip the 'u' */
                p += 5;
                if (cp >= 0xD800 && cp <= 0xDBFF && p[0] == '\\' && p[1] == 'u') {
                    unsigned long lo = 0;
                    for (k = 2; k <= 5; k++) {
                        int v = HexVal(p[k]);
                        if (v < 0) break;
                        lo = (lo << 4) | (unsigned long)v;
                    }
                    if (k > 5 && lo >= 0xDC00 && lo <= 0xDFFF) {
                        cp = 0x10000UL + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        p += 6;
                    }
                }
                if (gSSE.len < (int)sizeof(gSSE.text) - 5) {
                    char u[4];
                    int n = EncodeUTF8(cp, u);
                    for (int j = 0; j < n; j++) gSSE.text[gSSE.len++] = u[j];
                }
                continue;
            }
            else                  ch = *p;
            p++;
        } else {
            ch = *p++;
        }
        gSSE.text[gSSE.len++] = ch;
    }
    gSSE.text[gSSE.len] = '\0';
}

/* ── ApiSend — build Claude request and launch streaming POST ────── */
void ApiSend(const char* userText, const char* apiKey, const char* context)
{
    if (!userText || !userText[0]) return;

    static char escapedUser[512];
    static char escapedCtx[4096];
    static char body[12288];

    JsonEscape(userText, escapedUser, (int)sizeof(escapedUser));

    int bp = 0;

    if (context && context[0]) {
        JsonEscape(context, escapedCtx, (int)sizeof(escapedCtx));
        bp += snprintf(body + bp, sizeof(body) - bp,
                       "{\"model\":\"" API_MODEL "\",\"max_tokens\":%d,\"stream\":true,"
                       "\"system\":\"" API_SYSTEM_PROMPT "\\n\\nConversation context "
                       "(last messages):\\n%s\","
                       "\"messages\":[",
                       API_MAX_TOKENS, escapedCtx);
        /* snprintf returns the *wanted* length — clamp so sizeof(body)-bp
           can never wrap negative if a future edit makes this truncate. */
        if (bp >= (int)sizeof(body)) bp = (int)sizeof(body) - 1;
    } else {
        bp += snprintf(body + bp, sizeof(body) - bp,
                       "{\"model\":\"" API_MODEL "\",\"max_tokens\":%d,\"stream\":true,"
                       "\"system\":\"" API_SYSTEM_PROMPT "\",\"messages\":[",
                       API_MAX_TOKENS);
        if (bp >= (int)sizeof(body)) bp = (int)sizeof(body) - 1;
    }

    bp += snprintf(body + bp, sizeof(body) - bp,
                   "{\"role\":\"user\",\"content\":\"%s\"}]}",
                   escapedUser);
    if (bp >= (int)sizeof(body)) bp = (int)sizeof(body) - 1;

    gSSE.len          = 0;
    gSSE.done         = false;
    gSSE.error_text[0] = '\0';

    gHttp.SetAuthorization(apiKey);
    gHttp.SetExtraHeaders(API_EXTRA_HEADERS);
    gHttp.PostStream(API_ENDPOINT, body,
        [](const char* at, int length) { SSEFeed(at, length); },
        [](HttpResponse& r) {
            gSSE.done = true;
            if (r.Success && r.StatusCode >= 200 && r.StatusCode < 300) return;
            if (!r.Success) {
                snprintf(gSSE.error_text, sizeof(gSSE.error_text), "[Error: connection failed]");
            } else if (r.StatusCode == 401) {
                snprintf(gSSE.error_text, sizeof(gSSE.error_text), "[Error: invalid API key (401)]");
            } else if (r.StatusCode == 429) {
                snprintf(gSSE.error_text, sizeof(gSSE.error_text), "[Error: rate limited (429)]");
            } else {
                snprintf(gSSE.error_text, sizeof(gSSE.error_text), "[Error: HTTP %u]", r.StatusCode);
            }
        });
}
