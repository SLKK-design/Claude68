#pragma once
/* Settings.h — project-specific user preferences.
   Add or rename fields here when adapting to a new API.
   LoadSettings/SaveSettings handle persistence automatically. */

struct AppSettings {
    char apiKey[255];
    char debugMode;   /* 0 = off, 1 = on; persisted in prefs */
};

bool LoadSettings(AppSettings &settings);
void SaveSettings(const AppSettings &settings);
bool ShowSettingsDialog(AppSettings &settings);

/* Global defined in main.cpp */
extern AppSettings gSettings;

#include "../core/helpers/tls_cache.h"
