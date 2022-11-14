package com.github.kr328.clash.compat;

import org.jetbrains.annotations.NotNull;

import java.util.Objects;

public final class WindowCompat {
    static {
        CompatLibrary.load();
    }

    private static native void nativeSetWindowFrameSize(long handle, int frame, int size);

    private static native void nativeSetWindowBorderless(long handle);

    private static native void nativeSetWindowControlPosition(long handle, int control, int left, int top, int right, int bottom);

    private static native void nativeSetWindowMinimumSize(long handle, int width, int height);

    public static void setWindowBorderless(long handle) {
        nativeSetWindowBorderless(handle);
    }

    public static void setWindowFrameSize(long handle, @NotNull WindowFrame frame, int size) {
        nativeSetWindowFrameSize(handle, Objects.requireNonNull(frame).ordinal(), size);
    }

    public static void setWindowControlPosition(long handle, @NotNull WindowControl control, int left, int top, int right, int bottom) {
        nativeSetWindowControlPosition(handle, Objects.requireNonNull(control).ordinal(), left, top, right, bottom);
    }

    public static void setWindowMinimumSize(long handle, int width, int height) {
        nativeSetWindowMinimumSize(handle, width, height);
    }

    public enum WindowFrame {
        EDGE_INSETS,
        TITLE_BAR,
    }

    public enum WindowControl {
        CLOSE_BUTTON,
        BACK_BUTTON
    }
}
