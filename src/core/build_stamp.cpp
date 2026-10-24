/* build_stamp.cpp — always touched before each build so __DATE__/__TIME__ stay current. */
#include "config.h"
#include "chat_window.h"
#include <string.h>

extern bool gDebugMode;

static void ConPrint(const char* s)
{
    ChatLog(s, (int)strlen(s), false);
    ChatLog("\r", 1, false);
}

void LogBuildTime(void)
{
    if (!gDebugMode) return;
    ConPrint(APP_NAME " Debug Log - " __DATE__ " " __TIME__);
    ConPrint("");
    ConPrint("Cmd-Shift-C to copy all text");
    ConPrint("");
    ConPrint("----");
    ConPrint("");
}
