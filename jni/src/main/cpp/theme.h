#pragma once

#include <jni.h>

typedef void (*OnThemeChangedListener)();

int themeOnLoaded(JNIEnv *env);
int themeInit(OnThemeChangedListener listener);
int themeIsSupported();
int themeIsNightMode();

