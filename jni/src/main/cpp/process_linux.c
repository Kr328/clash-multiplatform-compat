#include "process.h"

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int fdNull;

int processInit()  {
    fdNull = open("/dev/null", O_RDWR);
    if (fdNull < 0) {
        abort();
    }
}

static int createPipePair(int *readable, int *writable) {
    int pipeFds[2] = {-1, -1};

    if (pipe(pipeFds) < 0) {
        return -1;
    }

    *readable = pipeFds[0];
    *writable = pipeFds[1];

    return 0;
}

static int markCloseOnExec(int fd) {
    return fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
}

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
    int fdStdoutW = -1;
    int fdStdinR = -1;
    int fdStderrW = -1;

    if (fdStdout && (createPipePair(fdStdout, &fdStdoutW) < 0 || markCloseOnExec(*fdStdout) < 0)) {
        goto error;
    }
    if (fdStdin && (createPipePair(&fdStdinR, fdStdin) < 0 || markCloseOnExec(*fdStdin) < 0)) {
        goto error;
    }
    if (fdStderr && (createPipePair(fdStderr, &fdStderrW) < 0 || markCloseOnExec(*fdStderr) < 0)) {
        goto error;
    }

    pid_t pid = fork();
    if (pid > 0) { // parent
        *handle = pid;

        close(fdStdoutW);
        close(fdStdinR);
        close(fdStderrW);

        return 0;
    } else if (pid == 0) { // child
        if (chdir(workingDir) < 0) {
            abort();
        }

        if (!fdStdout) {
            fdStdoutW = fdNull;
        }
        if (!fdStdin) {
            fdStdinR = fdNull;
        }
        if (!fdStderr) {
            fdStderrW = fdNull;
        }

        if (dup2(fdStdoutW, STDOUT_FILENO) < 0) {
            abort();
        }
        if (dup2(fdStdinR, STDIN_FILENO) < 0) {
            abort();
        }
        if (dup2(fdStderrW, STDERR_FILENO) < 0) {
            abort();
        }

        close(fdNull);
        close(fdStdoutW);
        close(fdStdinR);
        close(fdStderrW);

        if (execve(path, (char *const *) args, (char *const *) environments) < 0) {
            abort();
        }

        return -1;
    } else { // error
        goto error;
    }

    error:
    if (fdStdout) {
        close(*fdStdout);
        close(fdStdoutW);
    }
    if (fdStdin) {
        close(*fdStdin);
        close(fdStdinR);
    }
    if (fdStderr) {
        close(*fdStderr);
        close(fdStderrW);
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
