#pragma once

#include <jni.h>
#include <stdint.h>

#if defined(__WIN32__)
#include <windows.h>

#define INVALID_RESOURCE_HANDLE INVALID_HANDLE_VALUE

typedef HANDLE resourceHandle;
#elif defined(__linux__)
#define INVALID_RESOURCE_HANDLE (-1)

typedef int resourceHandle;
#endif

int loadProcess(JNIEnv *env);
int processInit();
int processCreate(
        const char *path,
        const char *args[],
        const char *workingDir,
        const char *environments[],
        resourceHandle *handle,
        resourceHandle *fdStdin,
        resourceHandle *fdStdout,
        resourceHandle *fdStderr
);
int processWait(void *handle);
void processTerminate(void *handle);
void processRelease(void *handle);
int processGetLastError(char *buf, int length);