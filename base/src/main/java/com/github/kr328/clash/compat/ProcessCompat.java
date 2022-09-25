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
        thread.setDaemon(true);
        return thread;
    });

    static {
        Compat.load();
    }

    private static byte[] toNativeString(final String str) {
        return (str + "\0").getBytes(Charset.defaultCharset());
    }

    public static Process createProcess(
            @NotNull final Path executablePath,
            @NotNull final List<String> arguments,
            @Nullable final Path workingDir,
            @Nullable final Map<String, String> environments,
            final boolean pipeStdout,
            final boolean pipeStdin,
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

        final FileDescriptor stdout;
        if (pipeStdout) {
            stdout = new FileDescriptor();
        } else {
            stdout = null;
        }

        final FileDescriptor stdin;
        if (pipeStdin) {
            stdin = new FileDescriptor();
        } else {
            stdin = null;
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
                stdout,
                stdin,
                stderr
        );

        final CompletableFuture<Integer> monitor = new CompletableFuture<>();
        MONITOR_POOL.submit(() -> {
            int exitCode = nativeWaitProcess(handle);

            nativeReleaseProcess(handle);

            return exitCode;
        });

        return new Process(handle, stdout, stdin, stderr, monitor);
    }

    private native static long nativeCreateProcess(
            final byte[] path,
            final byte[][] args,
            final byte[] workingDir,
            final byte[][] environments,
            final FileDescriptor stdout, // Out
            final FileDescriptor stdin,  // Out
            final FileDescriptor stderr  // Out
    ) throws IOException;

    private native static int nativeWaitProcess(long handle);

    private native static void nativeTerminateProcess(long handle);

    private native static void nativeReleaseProcess(long handle);

    private native static void nativeReleaseFileDescriptor(final FileDescriptor fd) throws IOException;

    private static void releaseFileDescriptor(final FileDescriptor fd) {
        try {
            nativeReleaseFileDescriptor(fd);
        } catch (IOException e) {
            // ignore
        }
    }

    public static class Process implements AutoCloseable, Closeable, Future<Integer> {
        private static final Cleaner cleaner = Cleaner.create();

        private final FileDescriptor stdout;
        private final FileDescriptor stdin;
        private final FileDescriptor stderr;
        private final Cleaner.Cleanable cleanable;
        private final Future<Integer> monitor;

        private Process(
                final long handle,
                final FileDescriptor stdout,
                final FileDescriptor stdin,
                final FileDescriptor stderr,
                final Future<Integer> monitor
        ) {
            this.cleanable = cleaner.register(this, () -> {
                nativeTerminateProcess(handle);

                releaseFileDescriptor(stdout);
                releaseFileDescriptor(stdin);
                releaseFileDescriptor(stderr);
            });

            this.stdout = stdout;
            this.stdin = stdin;
            this.stderr = stderr;
            this.monitor = monitor;
        }

        @Nullable
        public FileDescriptor getStdout() {
            return stdout;
        }

        @Nullable
        public FileDescriptor getStdin() {
            return stdin;
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

        @Override
        public Integer get() throws InterruptedException, ExecutionException {
            return monitor.get();
        }

        @Override
        public Integer get(long timeout, @NotNull TimeUnit unit) throws InterruptedException, ExecutionException, TimeoutException {
            return monitor.get(timeout, unit);
        }
    }
}
