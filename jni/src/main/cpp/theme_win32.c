#include "theme.h"

#include <windows.h>

static HKEY personalizeKey = NULL;

static DWORD themeMonitor(LPVOID lpParam) {
    while (1) {
        LRESULT r = RegNotifyChangeKeyValue(
                personalizeKey,
                FALSE,
                REG_NOTIFY_CHANGE_LAST_SET|REG_NOTIFY_CHANGE_NAME,
                NULL,
                FALSE
        );
        if (r != ERROR_SUCCESS) {
            break;
        }

        OnThemeChangedListener listener = lpParam;

        listener();
    }

    return 0;
};

int themeInit(OnThemeChangedListener listener) {
    LRESULT r = RegOpenKeyExA(
            HKEY_CURRENT_USER,
            "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0,
            KEY_READ,
            &personalizeKey
    );
    if (r != ERROR_SUCCESS) {
        return 0;
    }

    DWORD type = 0;
    r = RegQueryValueExA(personalizeKey, "SystemUsesLightTheme", 0, &type, NULL, NULL);
    if (r != ERROR_SUCCESS || type != REG_DWORD) {
        RegCloseKey(personalizeKey);

        personalizeKey = NULL;

        return 0;
    }

    CreateThread(NULL,0,&themeMonitor,listener,0,NULL);

    return 0;
}

int themeIsSupported() {
    return personalizeKey != NULL;
}

int themeIsNightMode() {
    DWORD result = 1;
    DWORD resultLength = sizeof(result);

    RegQueryValueExA(personalizeKey, "AppsUseLightTheme", 0, NULL, (LPBYTE) &result, &resultLength);

    return result == 0;
}
