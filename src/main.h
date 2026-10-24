#pragma once

struct AppState {
    bool  streaming;    /* SSE response stream active */
    bool  warmingUp;    /* TLS handshake in progress — show status text */
};

extern AppState gAppState;
extern bool     gDebugMode;   /* toggled by app menu → Debug item */
