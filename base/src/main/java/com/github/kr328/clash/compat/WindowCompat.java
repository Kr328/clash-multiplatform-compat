package com.github.kr328.clash.compat;

public final class WindowCompat {
    static {
        Compat.load();
    }

    private static native void nativeSetWindowFrameSize(long handle, int frame, int size);

    private static native void nativeSetWindowBorderless(long handle);

    private static native void nativeSetWindowControlPosition(long handle, int control, int left, int top, int right, int bottom);

    public static void setWindowBorderless(long handle) {
        nativeSetWindowBorderless(handle);
    }

    public static void setWindowFrameSize(long handle, WindowFrame frame, int size) {
        nativeSetWindowFrameSize(handle, frame.ordinal(), size);
    }

    public static void setWindowControlPosition(long handle, WindowControl control, int left, int top, int right, int bottom) {
        nativeSetWindowControlPosition(handle, control.ordinal(), left, top, right, bottom);
    }

    public enum WindowFrame {
        EDGE_INSETS,
        TITLE_BAR,
    }

    public enum WindowControl {
        MINIMIZE_BUTTON,
        MAXIMIZE_BUTTON,
        CLOSE_BUTTON,
    }
}
