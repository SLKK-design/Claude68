#pragma once
/* ═══════════════════════════════════════════════════════════════════════
   config.h — the one file to edit to wire a new API.

   1. This file       — app name, hostname, endpoint, model, UI strings
   2. CMakeLists.txt  — project() name (top line)
   3. src/api/        — implement ApiSend + SSEHandleLine for your API
   ═══════════════════════════════════════════════════════════════════════ */


/* ── App identity ────────────────────────────────────────────────────── */

#define APP_NAME      "Claude 68"
#define APP_NAME_PSTR "\pClaude 68"     /* Pascal string — for Toolbox calls */


/* ── Server ──────────────────────────────────────────────────────────── */

#define APP_HOST     "https://api.anthropic.com"    /* full URL  */
#define APP_HOSTNAME "api.anthropic.com"            /* TLS SNI   */


/* ── API request ─────────────────────────────────────────────────────── */

#define API_ENDPOINT      "/v1/messages"
#define API_EXTRA_HEADERS "anthropic-version: 2023-06-01\r\n"  /* "" = none */
#define API_MODEL         "claude-haiku-4-5-20251001"
#define API_MAX_TOKENS    1024
#define API_SYSTEM_PROMPT \
    "Respond in plain text only. No markdown, no asterisks, " \
    "no pound signs, no bullet dashes, no code fences."


/* ── UI strings ──────────────────────────────────────────────────────── */

#define APP_INPUT_PLACEHOLDER  "How can I help you today?"
#define APP_WARMUP_MSG         "Setting up secure connection\xC9"


/* ── Prefs file ──────────────────────────────────────────────────────── */

#define PREFS_FILE         "\p" APP_NAME "Prefs"    /* HFS filename (Pascal) */
#define PREFS_FILE_CREATOR 'CMac'                   /* 4-char creator code   */


/* ── TLS: certificates ──────────────────────────────────────────────────
   The trust store is in src/core/net/RootCert.cpp (DER bytes): the root
   CA, plus the intermediate CA that signs your server's certificate.
   Pinning the intermediate means the Mac checks exactly one P-256
   signature on the certificate chain (the intermediate's signature on
   the server's certificate) and never the root's slower one. That check
   runs on the first connection and its result is cached on disk, so
   later connections skip it until the server's certificate changes.

   Current chain: Google Trust Services WE1 (expires 2029-02-20) under
   GTS Root R4. Cloudflare uses it for api.anthropic.com and
   api.notion.com.

   Porting to a server behind a different chain (e.g. DigiCert for
   OpenAI, Amazon CA for AWS): replace both certificates in RootCert.cpp.
   To see the chain your server sends:
     openssl s_client -connect your.host:443 -showcerts </dev/null
   Save the intermediate's PEM block to a file, then turn it into C bytes:
     openssl x509 -in intermediate.pem -outform der | xxd -i
   If the intermediate isn't pinned, the Mac falls back to checking the
   whole chain. On a Classic II that can blow the ~15 s Cloudflare
   handshake window and look like a connection timeout, not a
   certificate error.
   ─────────────────────────────────────────────────────────────────── */
