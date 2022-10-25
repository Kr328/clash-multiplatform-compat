package com.github.kr328.clash.compat;

import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

import java.io.Closeable;
import java.io.FileDescriptor;
import java.io.IOException;
import java.lang.ref.Cleaner;
import java.nio.charset.Charset;
import java.nio.file.Path;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.*;

public final class ProcessCompat {
    private static final ExecutorService MONITOR_POOL = Executors.newCachedThreadPool((runnable) -> {
        final Thread thread = new Thread(runnable);
        thread.setName("Process-Monitor-Thread");
        thread.setDaemon(true);
        return thread;
    });

    static {
        CompatLibrary.load();
    }

    private static byte @NotNull [] toNativeString(@NotNull final String str) {
        return (str + "\0").getBytes(Charset.defaultCharset());
    }

    @NotNull
    public static synchronized Process createProcess(
            @NotNull final Path executablePath,
            @NotNull final List<String> arguments,
            @Nullable final Path workingDir,
            @Nullable final Map<String, String> environments,
            final boolean pipeStdin,
            final boolean pipeStdout,
            final boolean pipeStderr
    ) throws IOException {
        Objects.requireNonNull(executablePath);
        Objects.requireNonNull(arguments);

        final Map<String, String> mergedEnvironments = new HashMap<>(System.getenv());
        if (environments != null) {
            mergedEnvironments.putAll(environments);
        }

        final byte[] nativeExecutablePath = toNativeString(executablePath.toAbsolutePath().toString());
        final byte[][] nativeArguments = arguments.stream().map(ProcessCompat::toNativeString).toArray(byte[][]::new);
        final byte[] nativeWorkingDir = toNativeString((workingDir != null ? workingDir : Path.of(".")).toAbsolutePath().toString());
        final byte[][] nativeEnvironments = mergedEnvironments.entrySet().stream()
                .map(e -> e.getKey() + "=" + e.getValue()).map(ProcessCompat::toNativeString).toArray(byte[][]::new);

        final FileDescriptor stdin;
        if (pipeStdin) {
            stdin = new FileDescriptor();
        } else {
            stdin = null;
        }

        final FileDescriptor stdout;
        if (pipeStdout) {
            stdout = new FileDescriptor();
        } else {
            stdout = null;
        }

        final FileDescriptor stderr;
        if (pipeStderr) {
            stderr = new FileDescriptor();
        } else {
            stderr = null;
        }

        final long handle = nativeCreateProcess(
                nativeExecutablePath,
                nativeArguments,
                nativeWorkingDir,
                nativeEnvironments,
                stdin,
                stdout,
                stderr
        );

        final Future<Integer> monitor = MONITOR_POOL.submit(() -> nativeWaitProcess(handle));

        return new Process(handle, stdin, stdout, stderr, monitor);
    }

    private native static long nativeCreateProcess(
            final byte @NotNull [] path,
            final byte[] @NotNull [] args,
            final byte @NotNull [] workingDir,
            final byte[] @NotNull [] environments,
            @Nullable final FileDescriptor stdin,  // Out
            @Nullable final FileDescriptor stdout, // Out
            @Nullable final FileDescriptor stderr  // Out
    ) throws IOException;

    private native static int nativeWaitProcess(long handle);

    private native static void nativeTerminateProcess(long handle);

    private native static void nativeReleaseProcess(long handle);

    private native static void nativeReleaseFileDescriptor(@NotNull final FileDescriptor fd) throws IOException;

    private static void releaseFileDescriptor(@NotNull final FileDescriptor fd) {
        try {
            nativeReleaseFileDescriptor(fd);
        } catch (IOException e) {
            // ignore
        }
    }

    public static class Process implements AutoCloseable, Closeable, Future<Integer> {
        private static final Cleaner cleaner = Cleaner.create();

        @Nullable
        private final FileDescriptor stdin;
        @Nullable
        private final FileDescriptor stdout;
        @Nullable
        private final FileDescriptor stderr;
        @NotNull
        private final Cleaner.Cleanable cleanable;
        @NotNull
        private final Future<Integer> monitor;

        private Process(
                final long handle,
                @Nullable final FileDescriptor stdin,
                @Nullable final FileDescriptor stdout,
                @Nullable final FileDescriptor stderr,
                @NotNull final Future<Integer> monitor
        ) {
            this.cleanable = cleaner.register(this, () -> {
                nativeTerminateProcess(handle);
                nativeReleaseProcess(handle);

                if (stdin != null) {
                    releaseFileDescriptor(stdin);
                }
                if (stdout != null) {
                    releaseFileDescriptor(stdout);
                }
                if (stderr != null) {
                    releaseFileDescriptor(stderr);
                }
            });

            this.stdin = stdin;
            this.stdout = stdout;
            this.stderr = stderr;
            this.monitor = monitor;
        }

        @Nullable
        public FileDescriptor getStdin() {
            return stdin;
        }

        @Nullable
        public FileDescriptor getStdout() {
            return stdout;
        }

        @Nullable
        public FileDescriptor getStderr() {
            return stderr;
        }

        @Override
        public void close() {
            cleanable.clean();
        }

        @Override
        public boolean cancel(boolean mayInterruptIfRunning) {
            if (monitor.cancel(mayInterruptIfRunning)) {
                close();

                return true;
            }

            return false;
        }

        @Override
        public boolean isCancelled() {
            return monitor.isCancelled();
        }

        @Override
        public boolean isDone() {
            return monitor.isDone();
        }

        @NotNull
        @Override
        public Integer get() throws InterruptedException, ExecutionException {
            return monitor.get();
        }

        @NotNull
        @Override
        public Integer get(long timeout, @NotNull TimeUnit unit) throws InterruptedException, ExecutionException, TimeoutException {
            return monitor.get(timeout, unit);
        }
    }
}
