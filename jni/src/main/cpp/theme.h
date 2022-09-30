#pragma once

#include <jni.h>

typedef void (*OnThemeChangedListener)();

int loadTheme(JNIEnv *env);
int themeInit(OnThemeChangedListener listener);
int themeIsSupported();
int themeIsNightMode();

