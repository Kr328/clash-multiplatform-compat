#include "process.h"

#include "cleaner.h"

#include <windows.h>
#include <vector.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

CLEANER_NONNULL(vector, vector_destroy)

static HANDLE nul;

int processInit() {
    SECURITY_ATTRIBUTES attributes;
    memset(&attributes, 0, sizeof(SECURITY_ATTRIBUTES));
    attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    attributes.bInheritHandle = TRUE;
    attributes.lpSecurityDescriptor = NULL;

    nul = CreateFileA(
            "nul:",
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            &attributes,
            OPEN_EXISTING,
            0,
            NULL
    );
    if (nul == INVALID_HANDLE_VALUE) {
        abort();
    }

    return 0;
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
    static char space = ' ';
    static char zero = '\0';
    static char quote = '"';

    CLEANABLE(vector_destroy)
    vector joinedArgs = vector_init(sizeof(char));
    for (const char **argument = args; *argument != NULL; argument++) {
        vector_add_last(joinedArgs, &quote);
        vector_add_all(joinedArgs, (void *) *argument, strlen(*argument));
        vector_add_last(joinedArgs, &quote);

        vector_add_last(joinedArgs, &space);
    }
    vector_remove_last(joinedArgs);
    vector_add_last(joinedArgs, &zero);

    CLEANABLE(vector_destroy)
    vector joinedEnvs = vector_init(sizeof(char));
    for (const char **environment = environments; *environment != NULL; environment++) {
        vector_add_all(joinedEnvs, (void *) *environment, strlen(*environment));

        vector_add_last(joinedEnvs, &zero);
    }
    vector_add_last(joinedEnvs, &zero);

    SECURITY_ATTRIBUTES pipeAttributes;
    memset(&pipeAttributes, 0, sizeof(SECURITY_ATTRIBUTES));
    pipeAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    pipeAttributes.bInheritHandle = TRUE;
    pipeAttributes.lpSecurityDescriptor = NULL;
    HANDLE stdoutR = INVALID_HANDLE_VALUE;
    HANDLE stdoutW = INVALID_HANDLE_VALUE;
    HANDLE stdinR = INVALID_HANDLE_VALUE;
    HANDLE stdinW = INVALID_HANDLE_VALUE;
    HANDLE stderrR = INVALID_HANDLE_VALUE;
    HANDLE stderrW = INVALID_HANDLE_VALUE;
    if (fdStdout != NULL) {
        if (!CreatePipe(&stdoutR, &stdoutW, &pipeAttributes, 4096)) {
            goto error;
        }
        *fdStdout = stdoutR;
        SetHandleInformation(stdoutR, HANDLE_FLAG_INHERIT, FALSE);
    } else {
        stdoutW = nul;
    }
    if (fdStdin != NULL) {
        if (!CreatePipe(&stdinR, &stdinW, &pipeAttributes, 4096)) {
            goto error;
        }
        *fdStdin = stdinW;
        SetHandleInformation(stdinW, HANDLE_FLAG_INHERIT, FALSE);
    } else {
        stdinR = nul;
    }
    if (fdStderr != NULL) {
        if (!CreatePipe(&stderrR, &stderrW, &pipeAttributes, 4096)) {
            goto error;
        }
        *fdStderr = stderrR;
        SetHandleInformation(stderrR, HANDLE_FLAG_INHERIT, FALSE);
    } else {
        stderrW = nul;
    }

    STARTUPINFO startupinfo;
    memset(&startupinfo, 0, sizeof(startupinfo));
    startupinfo.cb = sizeof(STARTUPINFO);
    startupinfo.hStdOutput = stdoutW;
    startupinfo.hStdInput = stdinR;
    startupinfo.hStdError = stderrW;
    startupinfo.dwFlags = STARTF_USESTDHANDLES;

    PROCESS_INFORMATION info;
    memset(&info, 0, sizeof(info));

    LRESULT r = CreateProcessA(
            path,
            vector_get_data(joinedArgs),
            NULL,
            NULL,
            TRUE,
            0,
            vector_get_data(joinedEnvs),
            workingDir,
            &startupinfo,
            &info
    );
    if (!r) {
        goto error;
    }

    fprintf(stderr, "path = %s args = %s", path, (const char *) vector_get_data(joinedArgs));
    fflush(stderr);

    CloseHandle(stdoutW);
    CloseHandle(stdinR);
    CloseHandle(stderrW);
    CloseHandle(info.hThread);
    *(HANDLE *) handle = info.hProcess;

    return 0;

    error:
    if (stdoutR != INVALID_HANDLE_VALUE) {
        CloseHandle(stdoutR);
        CloseHandle(stdoutW);
    }
    if (stdinW != INVALID_HANDLE_VALUE) {
        CloseHandle(stdinR);
        CloseHandle(stdinW);
    }
    if (stderrR != INVALID_HANDLE_VALUE) {
        CloseHandle(stderrR);
        CloseHandle(stderrW);
    }

    return 1;
}

int processWait(void *handle) {
    if (WaitForSingleObject(handle, INFINITE)) {
        return -1;
    }

    DWORD code;
    if (!GetExitCodeProcess(handle, &code)) {
        return -1;
    }

    return (int) code;
}

void processTerminate(void *handle) {
    TerminateProcess(handle, 255);
}

void processRelease(void *handle) {
    CloseHandle(handle);
}

int processGetLastError(char *buf, int length) {
    DWORD error = GetLastError();

    DWORD n = FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            error,
            MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
            buf,
            length - 1,
            NULL
    );

    buf[n] = 0;

    return (int) error;
}
