#include "shell.h"

#include "cleaner.h"

#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

CLEANER_NULLABLE(char *, free)

int shellIsSupported() {
    return 1;
}

static char *convertFilterToNative(const struct PickerFilter filters[], int length) {
    char *result = NULL;
    size_t offset = 0;

    for (int i = 0; i < length; i++) {
        const struct PickerFilter *filter = &filters[i];

        size_t nameSize = strlen(filter->name);
        result = realloc(result, offset + nameSize + 1);
        strcpy(result + offset, filter->name);
        offset += nameSize + 1;

        for (const char **p = filter->extensions; *p != NULL; p++) {
            size_t extSize = strlen(*p);
            result = realloc(result, offset + extSize + 2 + 1 + 1);
            strcpy(result + offset, "*.");
            strcpy(result + offset + 2, *p);
            strcpy(result + offset + 2 + extSize, ";");
            offset += extSize + 2 + 1;
        }

        result = realloc(result, offset + 1);
        result[offset] = 0;
        offset += 1;
    }

    result = realloc(result, offset + 1);
    result[offset] = 0;

    return result;
}

int shellPickFile(
        void *windowHandle,
        const struct PickerFilter filters[],
        int filtersLength,
        char *path,
        int pathLength
) {
    CLEANABLE(free)
    char *filter = convertFilterToNative(filters, filtersLength);

    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = windowHandle;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = path;
    ofn.nMaxFile = pathLength;
    ofn.lpstrInitialDir = getenv("USERPROFILE");
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        return 0;
    }

    return 1;
}

int shellLaunchOpenFile(void *windowHandle, const char *path) {
    if ((intptr_t) ShellExecuteA(windowHandle, "open", path, NULL, NULL, SW_SHOW) > 32) {
        return 0;
    }

    return 1;
}
