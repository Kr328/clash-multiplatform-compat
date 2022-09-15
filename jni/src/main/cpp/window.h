#pragma once

#include <jni.h>

enum WindowControl {
    CLOSE_BUTTON,
    WINDOW_CONTROL_END
};

enum WindowFrame {
    EDGE_INSETS,
    TITLE_BAR,
    WINDOW_FRAME_END
};

int loadWindow(JNIEnv *env);
void windowInit(JNIEnv *env);
void windowSetWindowBorderless(void *handle);
void windowSetWindowFrameSize(void *handle, enum WindowFrame frame, int size);
void windowSetWindowControlPosition(void *handle, enum WindowControl control, int left, int top, int right, int bottom);
