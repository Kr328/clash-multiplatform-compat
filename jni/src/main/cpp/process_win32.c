#include "process.h"

#include "cleaner.h"

#include <windows.h>
#include <vector.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define PIPE_BUFFER_SIZE 4096

CLEANER_NONNULL(vector, vector_destroy)

CLEANER_NULLABLE(LPPROC_THREAD_ATTRIBUTE_LIST, free)

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

static void closeHandleSilent(HANDLE handle) {
    DWORD error = GetLastError();
    CloseHandle(handle);
    SetLastError(error);
}

static int createPipePair(HANDLE *readable, HANDLE *writable, BOOL readableInheritable, BOOL writableInheritable) {
    if (!CreatePipe(readable, writable, NULL, PIPE_BUFFER_SIZE)) {
        return 1;
    }
    if (readableInheritable) {
        SetHandleInformation(*readable, HANDLE_FLAG_INHERIT, TRUE);
    }
    if (writableInheritable) {
        SetHandleInformation(*writable, HANDLE_FLAG_INHERIT, TRUE);
    }

    return 0;
}

CLEANER_NULLABLE(HANDLE, closeHandleSilent)

int processCreate(
        const char *path,
        const char *args[],
        const char *workingDir,
        const char *environments[],
        resourceHandle *handle,
        resourceHandle *fdStdin,
        resourceHandle *fdStdout,
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

    CLEANABLE(closeHandleSilent)
    HANDLE stdinReadable = INVALID_HANDLE_VALUE;
    CLEANABLE(closeHandleSilent)
    HANDLE stdinWritable = INVALID_HANDLE_VALUE;
    if (fdStdin != NULL) {
        if (createPipePair(&stdinReadable, &stdinWritable, TRUE, FALSE)) {
            return 1;
        }
    }

    CLEANABLE(closeHandleSilent)
    HANDLE stdoutReadable = INVALID_HANDLE_VALUE;
    CLEANABLE(closeHandleSilent)
    HANDLE stdoutWritable = INVALID_HANDLE_VALUE;
    if (fdStdout != NULL) {
        if (createPipePair(&stdoutReadable, &stdoutWritable, FALSE, TRUE)) {
            return 1;
        }
    }

    CLEANABLE(closeHandleSilent)
    HANDLE stderrReadable = INVALID_HANDLE_VALUE;
    CLEANABLE(closeHandleSilent)
    HANDLE stderrWritable = INVALID_HANDLE_VALUE;
    if (fdStderr != NULL) {
        if (createPipePair(&stderrReadable, &stderrWritable, FALSE, TRUE)) {
            return 1;
        }
    }

    SIZE_T attributesListSize;
    if (!InitializeProcThreadAttributeList(NULL, 1, 0, &attributesListSize) && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return 1;
    }

    CLEANABLE(free)
    LPPROC_THREAD_ATTRIBUTE_LIST attributesList = malloc(attributesListSize);
    if (!InitializeProcThreadAttributeList(attributesList, 1, 0, &attributesListSize)) {
        return 1;
    }

    HANDLE childStdin = stdinReadable;
    if (childStdin == INVALID_HANDLE_VALUE) {
        childStdin = nul;
    }
    HANDLE childStdout = stdoutWritable;
    if (childStdout == INVALID_HANDLE_VALUE) {
        childStdout = nul;
    }
    HANDLE childStderr = stderrWritable;
    if (childStderr == INVALID_HANDLE_VALUE) {
        childStderr = nul;
    }

    HANDLE inheritHandles[3] = {childStdin, childStdout, childStderr};

    WINBOOL attributeUpdated = UpdateProcThreadAttribute(
            attributesList,
            0,
            PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            inheritHandles,
            sizeof(inheritHandles),
            NULL,
            NULL
    );
    if (!attributeUpdated) {
        return 1;
    }

    STARTUPINFOEXA si;
    memset(&si, 0, sizeof(si));
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.hStdInput = childStdin;
    si.StartupInfo.hStdOutput = childStdout;
    si.StartupInfo.hStdError = childStderr;
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.lpAttributeList = attributesList;

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
            &si.StartupInfo,
            &info
    );
    if (!r) {
        return 1;
    }

    if (fdStdin != NULL) {
        *fdStdin = stdinWritable;
        stdinWritable = INVALID_HANDLE_VALUE;
    }
    if (fdStdout != NULL) {
        *fdStdout = stdoutReadable;
        stdoutReadable = INVALID_HANDLE_VALUE;
    }
    if (fdStderr != NULL) {
        *fdStderr = stderrReadable;
        stderrReadable = INVALID_HANDLE_VALUE;
    }

    closeHandleSilent(info.hThread);

    *handle = info.hProcess;

    return 0;
}

int processWait(void *handle) {
    DWORD code = 1;
    while (GetExitCodeProcess(handle, &code) && code == STILL_ACTIVE) {
        WaitForSingleObject(handle, INFINITE);
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
