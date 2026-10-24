/* sse.cpp — Server-Sent Events line accumulator.
   Generic — do not modify for new projects.
   Call SSEFeed() with raw HTTP body chunks.
   Implement SSEHandleLine() in your api client.          */

#include "api_client.h"
#include <string.h>

static char sLine[4096];
static int  sLineLen = 0;
static bool sLineOverflow = false; // §4 ARCH: overflow flag — set when line exceeds buffer

void SSEFeed(const char* at, int length)
{
    for (int i = 0; i < length; i++) {
        char c = at[i];
        if (c == '\n' || c == '\r') {
            if (sLineLen > 0 && !sLineOverflow) { // §4 ARCH: skip overflowed lines
                sLine[sLineLen] = '\0';
                SSEHandleLine(sLine, sLineLen);
            }
            sLineLen     = 0;    // §4 ARCH: reset on line-end
            sLineOverflow = false;
        } else {
            if (sLineLen < (int)sizeof(sLine) - 1) {
                sLine[sLineLen++] = c;
            } else {
                sLineOverflow = true; // §4 ARCH: flag overflow, skip remainder of line
            }
        }
    }
}
