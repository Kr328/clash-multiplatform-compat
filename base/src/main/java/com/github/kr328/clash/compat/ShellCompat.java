package com.github.kr328.clash.compat;

import org.jetbrains.annotations.Blocking;
import org.jetbrains.annotations.NonBlocking;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.util.List;

public final class ShellCompat {
    static {
        CompatLibrary.load();
    }

    private static native boolean nativeIsSupported();

    private static native byte @Nullable [] nativePickFile(long windowHandle, @NotNull NativePickerFilter[] filters) throws IOException;

    private static native void nativeLaunchOpenFile(long windowHandle, final byte @Nullable [] path) throws IOException;

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
                .map(f -> new NativePickerFilter(f.name.getBytes(StandardCharsets.UTF_8), f.extensions.toArray(new String[0])))
                .toArray(NativePickerFilter[]::new);

        final byte[] path = nativePickFile(windowHandle, nativeFilers);
        if (path != null) {
            return Path.of(new String(path, StandardCharsets.UTF_8));
        }

        return null;
    }

    @NonBlocking
    public static void launchOpenFile(long windowHandle, @NotNull final Path path) throws IOException {
        nativeLaunchOpenFile(windowHandle, path.toAbsolutePath().toString().getBytes(StandardCharsets.UTF_8));
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
        public final byte[] name;
        public final String[] extensions;

        private NativePickerFilter(byte[] name, String[] extensions) {
            this.name = name;
            this.extensions = extensions;
        }
    }
}
