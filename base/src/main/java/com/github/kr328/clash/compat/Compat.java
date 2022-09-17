package com.github.kr328.clash.compat;

import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.Objects;

final class Compat {
    static {
        final String osName = System.getProperty("os.name").toLowerCase();

        final String extension;
        if (osName.contains("windows")) {
            extension = ".dll";
        } else if (osName.contains("linux")) {
            extension = ".so";
        } else {
            throw new LinkageError("Unsupported os " + osName);
        }

        final String osArch = System.getProperty("os.arch").toLowerCase();

        final String arch;
        if (osArch.contains("amd64") || osArch.contains("x86_64")) {
            arch = "amd64";
        } else if (osArch.contains("x86") || osArch.contains("386") || osArch.contains("686")) {
            arch = "x86";
        } else {
            throw new LinkageError("Unsupported arch " + osArch);
        }

        try {
            final URL url = Objects.requireNonNull(Compat.class.getResource("/com/github/kr328/clash/compat/libcompat-" + arch + extension));
            if ("file".equals(url.getProtocol())) {
                System.load(Path.of(url.toURI()).toAbsolutePath().toString());
            } else {
                final Path tempPath = Files.createTempFile("lib", extension);
                Runtime.getRuntime().addShutdownHook(new Thread(() -> {
                    try {
                        Files.delete(tempPath);
                    } catch (IOException e) {
                        // ignored
                    }
                }));

                try (final InputStream input = url.openStream()) {
                    Files.copy(input, tempPath, StandardCopyOption.REPLACE_EXISTING);
                }

                System.load(tempPath.toAbsolutePath().toString());
            }
        } catch (final Exception e) {
            throw new LinkageError("Load libcompat-" + arch + extension, e);
        }
    }

    static void load() {
        // stub
    }
}
