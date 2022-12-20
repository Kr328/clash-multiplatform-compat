#pragma once

#include <jni.h>

struct PickerFilter {
    char *name;
    const char **extensions;
};

int loadShell(JNIEnv *env);
int shellIsSupported();
int shellPickFile(void *windowHandle, const struct PickerFilter filters[], int filtersLength, char *path, int pathLength);
int shellLaunchOpenFile(void *windowHandle, const char *path);
