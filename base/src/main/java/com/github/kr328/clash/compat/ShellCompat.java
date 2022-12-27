package com.github.kr328.clash.compat;

import org.jetbrains.annotations.Blocking;
import org.jetbrains.annotations.NonBlocking;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

import java.io.IOException;
import java.nio.file.Path;
import java.util.List;

public final class ShellCompat {
    static {
        CompatLibrary.load();
    }

    private static native boolean nativeIsSupported();

    private static native @Nullable String nativePickFile(long windowHandle, @NotNull NativePickerFilter[] filters) throws IOException;

    private static native void nativeLaunchFile(long windowHandle, final @Nullable String path) throws IOException;

    public static boolean isSupported() {
        return nativeIsSupported();
    }

    @Nullable
    @Blocking
    public static Path pickFile(long windowHandle, @Nullable List<PickerFilter> filters) throws IOException {
        if (filters == null) {
            filters = List.of(new PickerFilter("All Files", List.of("*")));
        }

        final NativePickerFilter[] nativeFilers = filters.stream()
                .map(f -> new NativePickerFilter(f.name, f.extensions.toArray(new String[0])))
                .toArray(NativePickerFilter[]::new);

        final String path = nativePickFile(windowHandle, nativeFilers);
        if (path != null) {
            return Path.of(path);
        }

        return null;
    }

    @NonBlocking
    public static void launchFile(long windowHandle, @NotNull final Path path) throws IOException {
        nativeLaunchFile(windowHandle, path.toAbsolutePath().toString());
    }

    public static final class PickerFilter {
        private final String name;
        private final List<String> extensions;

        public PickerFilter(String name, List<String> extensions) {
            this.name = name;
            this.extensions = extensions;
        }

        public String getName() {
            return name;
        }

        public List<String> getExtensions() {
            return extensions;
        }
    }

    private static class NativePickerFilter {
        public final String name;
        public final String[] extensions;

        private NativePickerFilter(String name, String[] extensions) {
            this.name = name;
            this.extensions = extensions;
        }
    }
}
