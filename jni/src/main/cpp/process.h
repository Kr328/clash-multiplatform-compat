#pragma once

#include <jni.h>
#include <stdint.h>

#ifdef __WIN32__
#include <windows.h>

typedef HANDLE resourceHandle;
#endif

int loadProcess(JNIEnv *env);
int processInit();
int processCreate(
        const char *path,
        const char *args[],
        const char *workingDir,
        const char *environments[],
        resourceHandle *handle,
        resourceHandle *fdStdout,
        resourceHandle *fdStdin,
        resourceHandle *fdStderr
);
int processWait(void *handle);
void processTerminate(void *handle);
void processRelease(void *handle);
int processGetLastError(char *buf, int length);