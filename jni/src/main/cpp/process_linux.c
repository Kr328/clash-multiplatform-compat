#include "process.h"
#include "cleaner.h"

#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int fdNull;

int processInit() {
    fdNull = open("/dev/null", O_RDWR | O_CLOEXEC);
    if (fdNull < 0) {
        abort();
    }
}

static int createPipePair(int *readable, int *writable) {
    int pipeFds[2] = {-1, -1};

    if (pipe2(pipeFds, O_CLOEXEC) < 0) {
        return -1;
    }

    *readable = pipeFds[0];
    *writable = pipeFds[1];

    return 0;
}

static void cleanFileDescriptors(int fdExecutable) {
    DIR *fds = opendir("/proc/self/fd");
    if (fds == NULL) {
        return;
    }

    struct dirent *entry = NULL;
    while ((entry = readdir(fds)) != NULL) {
        int fd = (int) strtol(entry->d_name, NULL, 10);
        if (fd == dirfd(fds) || fd == fdExecutable ||
            fd == STDOUT_FILENO || fd == STDIN_FILENO || fd == STDERR_FILENO) {
            continue;
        }
        close(fd);
    }

    closedir(fds);
}

static void closeSilent(int fd) {
    int err = errno;
    close(fd);
    errno = err;
}

CLEANER_NONNULL(int, closeSilent)

int processCreate(
        const char *path,
        const char *args[],
        const char *workingDir,
        const char *environments[],
        resourceHandle *handle,
        resourceHandle *fdStdout,
        resourceHandle *fdStdin,
        resourceHandle *fdStderr
) {
    CLEANABLE(closeSilent)
    int fdWorkingDir = open(workingDir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fdWorkingDir < 0) {
        return -1;
    }

    CLEANABLE(closeSilent)
    int fdExecutable = open(path, O_RDONLY | O_CLOEXEC);
    if (fdExecutable < 0) {
        return -1;
    }

    char fdExecutablePath[32] = {0};
    snprintf(fdExecutablePath, sizeof(fdExecutablePath), "/proc/self/fd/%d", fdExecutable);
    if (access(fdExecutablePath, R_OK | X_OK) != 0) {
        return -1;
    }

    CLEANABLE(closeSilent)
    int fdStdoutReadable = -1;
    CLEANABLE(closeSilent)
    int fdStdoutWritable = -1;
    if (fdStdout && createPipePair(&fdStdoutReadable, &fdStdoutWritable) < 0) {
        return -1;
    }

    CLEANABLE(closeSilent)
    int fdStdinReadable = -1;
    CLEANABLE(closeSilent)
    int fdStdinWritable = -1;
    if (fdStdin && createPipePair(&fdStdinReadable, &fdStdinWritable) < 0) {
        return -1;
    }

    CLEANABLE(closeSilent)
    int fdStderrReadable = -1;
    CLEANABLE(closeSilent)
    int fdStderrWritable = -1;
    if (fdStderr && createPipePair(&fdStderrReadable, &fdStderrWritable) < 0) {
        return -1;
    }

    pid_t pid = fork();
    if (pid > 0) { // parent
        *handle = pid;

        if (fdStdout) {
            *fdStdout = fdStdoutReadable;
            fdStdoutReadable = -1;
        }
        if (fdStdin) {
            *fdStdin = fdStdinWritable;
            fdStdinWritable = -1;
        }
        if (fdStderr) {
            *fdStderr = fdStderrReadable;
            fdStderrReadable = -1;
        }

        return 0;
    } else if (pid == 0) { // child
        if (fchdir(fdWorkingDir) < 0) {
            abort();
        }

        if (!fdStdout) {
            fdStdoutWritable = fdNull;
        }
        if (!fdStdin) {
            fdStdinReadable = fdNull;
        }
        if (!fdStderr) {
            fdStderrWritable = fdNull;
        }

        if (dup3(fdStdoutWritable, STDOUT_FILENO, 0) < 0) {
            abort();
        }
        if (dup3(fdStdinReadable, STDIN_FILENO, 0) < 0) {
            abort();
        }
        if (dup3(fdStderrWritable, STDERR_FILENO, 0) < 0) {
            abort();
        }

        cleanFileDescriptors(fdExecutable);

        if (fexecve(fdExecutable, (char *const *) args, (char *const *) environments) < 0) {
            abort();
        }

        return -1;
    }

    return -1;
}

int processWait(void *handle) {
    pid_t pid = (pid_t) (intptr_t) handle;
    int r = -1;
    waitpid(pid, &r, 0);
    return r;
}

void processTerminate(void *handle) {
    pid_t pid = (pid_t) (intptr_t) handle;
    kill(pid, SIGKILL);
}

void processRelease(void *handle) {
    // do nothing
}

int processGetLastError(char *buf, int length) {
    strncpy(buf, strerror(errno), length);
}
