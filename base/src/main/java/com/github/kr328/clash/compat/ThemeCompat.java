package com.github.kr328.clash.compat;

import org.jetbrains.annotations.NotNull;

import java.lang.ref.Cleaner;

public final class ThemeCompat {
    static {
        CompatLibrary.load();
    }

    private static native boolean nativeIsSupported();

    private static native boolean nativeIsNight();

    private static native long nativeMonitor(@NotNull OnThemeChangedListener listener);

    private static native void nativeDisposeMonitor(long ptr);

    private static native void nativeReleaseMonitor(long ptr);

    public static boolean isSupported() {
        return nativeIsSupported();
    }

    public static boolean isNight() {
        return nativeIsNight();
    }

    @NotNull
    public static Disposable monitor(@NotNull final OnThemeChangedListener listener) {
        final long ptr = nativeMonitor(listener);

        final Disposable disposable = () -> nativeDisposeMonitor(ptr);

        Disposable.cleaner.register(disposable, () -> nativeReleaseMonitor(ptr));

        return disposable;
    }

    public interface Disposable {
        Cleaner cleaner = Cleaner.create();

        void dispose();
    }

    public interface OnThemeChangedListener {
        void onChanged();
    }
}
