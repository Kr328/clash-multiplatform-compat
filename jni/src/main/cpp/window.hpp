#pragma once

#include <jni.h>

namespace window {
    enum WindowControl {
        CLOSE_BUTTON,
        BACK_BUTTON,
        WINDOW_CONTROL_END
    };

    enum WindowFrame {
        EDGE_INSETS,
        TITLE_BAR,
        WINDOW_FRAME_END
    };

    bool initialize(JNIEnv *env);

    bool install(JNIEnv *env);
    void setWindowBorderless(void *handle);
    void setWindowFrameSize(void *handle, enum WindowFrame frame, int size);
    void setWindowControlPosition(void *handle, enum WindowControl control, int left, int top, int right, int bottom);
}
