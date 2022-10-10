package com.github.kr328.clash.compat;

import java.lang.ref.Cleaner;
import java.util.ArrayList;

public final class ThemeCompat {
    private static final ArrayList<OnThemeChangedListener> listeners = new ArrayList<>();

    static {
        CompatLibrary.load();
    }

    private static native boolean nativeIsSupported();

    private static native boolean nativeIsNightMode();

    @SuppressWarnings("unused")
    private static void onThemeChanged() {
        listeners.forEach(OnThemeChangedListener::onChanged);
    }

    public static boolean isSupported() {
        return nativeIsSupported();
    }

    public static boolean isNightMode() {
        return nativeIsNightMode();
    }

    public static OnThemeChangedListener.Holder registerOnThemeChangedListener(final OnThemeChangedListener listener) {
        listeners.add(listener);

        return new OnThemeChangedListener.Holder(listener);
    }

    public interface OnThemeChangedListener {
        void onChanged();

        class Holder implements AutoCloseable {
            private static final Cleaner cleaner = Cleaner.create();

            private final Cleaner.Cleanable cleanable;

            public Holder(OnThemeChangedListener listener) {
                this.cleanable = cleaner.register(this, () -> listeners.remove(listener));
            }

            @Override
            public void close() {
                cleanable.clean();
            }
        }
    }
}
