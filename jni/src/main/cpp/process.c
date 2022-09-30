#include "process.h"

#include <malloc.h>

static jfieldID fieldFileDescriptorFd;
static jfieldID fieldFileDescriptorHandle;
static jmethodID methodFileDescriptorClose;

static jlong jniCreateProcess(
        JNIEnv *env,
        jclass clazz,
        jbyteArray path,
        jobjectArray args,
        jbyteArray workingDir,
        jobjectArray environments,
        jobject fdStdout,
        jobject fdStdin,
        jobject fdStderr
) {
    resourceHandle hProcess = INVALID_RESOURCE_HANDLE;
    resourceHandle hStdout = INVALID_RESOURCE_HANDLE;
    resourceHandle hStdin = INVALID_RESOURCE_HANDLE;
    resourceHandle hStderr = INVALID_RESOURCE_HANDLE;

    jbyte *cPath = (*env)->GetByteArrayElements(env, path, NULL);

    int cArgsLength = (*env)->GetArrayLength(env, args);
    jbyte **cArgs = malloc(sizeof(const char *) * (cArgsLength + 1));
    for (int i = 0; i < cArgsLength; i++) {
        cArgs[i] = (*env)->GetByteArrayElements(env, (*env)->GetObjectArrayElement(env, args, i), NULL);
    }
    cArgs[cArgsLength] = NULL;

    jbyte *cWorkingDir = (*env)->GetByteArrayElements(env, workingDir, NULL);

    int cEnvironmentsLength = (*env)->GetArrayLength(env, environments);
    jbyte **cEnvironments = malloc(sizeof(const char *) * (cEnvironmentsLength + 1));
    for (int i = 0; i < cEnvironmentsLength; i++) {
        cEnvironments[i] = (*env)->GetByteArrayElements(env, (*env)->GetObjectArrayElement(env, environments, i), NULL);
    }
    cEnvironments[cEnvironmentsLength] = NULL;

    int result = processCreate(
            (const char *) cPath,
            (const char **) cArgs,
            (const char *) cWorkingDir,
            (const char **) cEnvironments,
            &hProcess,
            fdStdout != NULL ? &hStdout : NULL,
            fdStdin != NULL ? &hStdin : NULL,
            fdStderr != NULL ? &hStderr : NULL
    );

    (*env)->ReleaseByteArrayElements(env, path, cPath, JNI_ABORT);

    for (int i = 0; i < cArgsLength; i++) {
        (*env)->ReleaseByteArrayElements(env, (*env)->GetObjectArrayElement(env, args, i), cArgs[i], JNI_ABORT);
    }
    free(cArgs);

    (*env)->ReleaseByteArrayElements(env, workingDir, cWorkingDir, JNI_ABORT);

    for (int i = 0; i < cEnvironmentsLength; i++) {
        (*env)->ReleaseByteArrayElements(env, (*env)->GetObjectArrayElement(env, environments, i), cEnvironments[i], JNI_ABORT);
    }
    free(cEnvironments);

    if (result) {
        char buffer[4096];
        processGetLastError(buffer, sizeof(buffer));

        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/io/IOException"), buffer);

        return -1;
    }

#if defined(__WIN32__)
    if (fdStdout != NULL) {
        (*env)->SetLongField(env, fdStdout, fieldFileDescriptorHandle, (jlong) hStdout);
    }
    if (fdStdin != NULL) {
        (*env)->SetLongField(env, fdStdin, fieldFileDescriptorHandle, (jlong) hStdin);
    }
    if (fdStderr != NULL) {
        (*env)->SetLongField(env, fdStderr, fieldFileDescriptorHandle, (jlong) hStderr);
    }
#elif defined(__linux__)
    if (fdStdout != NULL) {
        (*env)->SetIntField(env, fdStdout, fieldFileDescriptorFd, (jint) hStdout);
    }
    if (fdStdin != NULL) {
        (*env)->SetIntField(env, fdStdin, fieldFileDescriptorFd, (jint) hStdin);
    }
    if (fdStderr != NULL) {
        (*env)->SetIntField(env, fdStderr, fieldFileDescriptorFd, (jint) hStderr);
    }
#endif

    return (jlong) hProcess;
}

static jint jniWaitProcess(JNIEnv *env, jclass clazz, jlong handle) {
    return processWait((void*) handle);
}

static void jniTerminateProcess(JNIEnv *env, jclass clazz, jlong handle) {
    processTerminate((void*) handle);
}

static void jniReleaseProcess(JNIEnv *env, jclass clazz, jlong handle) {
    processRelease((void *) handle);
}

static void jniReleaseFileDescriptor(JNIEnv *env, jclass clazz, jobject fd) {
    (*env)->CallVoidMethod(env, fd, methodFileDescriptorClose);
}

int loadProcess(JNIEnv *env) {
    if (processInit()) {
        return 1;
    }

    jclass clazzFileDescriptor = (*env)->FindClass(env, "java/io/FileDescriptor");
    if (clazzFileDescriptor == NULL) {
        return 1;
    }

    fieldFileDescriptorFd = (*env)->GetFieldID(env, clazzFileDescriptor, "fd", "I");
    if (fieldFileDescriptorFd == NULL) {
        return 1;
    }

    fieldFileDescriptorHandle = (*env)->GetFieldID(env, clazzFileDescriptor, "handle", "J");
    if (fieldFileDescriptorHandle == NULL) {
        return 1;
    }

    methodFileDescriptorClose = (*env)->GetMethodID(env, clazzFileDescriptor, "close", "()V");
    if (methodFileDescriptorClose == NULL) {
        return 1;
    }

    jclass process = (*env)->FindClass(env, "com/github/kr328/clash/compat/ProcessCompat");
    if (process == NULL) {
        return 1;
    }

    const JNINativeMethod methods[] = {
            {
                    .name = "nativeCreateProcess",
                    .signature = "([B[[B[B[[BLjava/io/FileDescriptor;Ljava/io/FileDescriptor;Ljava/io/FileDescriptor;)J",
                    .fnPtr = &jniCreateProcess,
            },
            {
                .name = "nativeWaitProcess",
                .signature = "(J)I",
                .fnPtr = &jniWaitProcess,
            },
            {
                .name = "nativeTerminateProcess",
                .signature = "(J)V",
                .fnPtr = &jniTerminateProcess,
            },
            {
                .name = "nativeReleaseProcess",
                .signature = "(J)V",
                .fnPtr = &jniReleaseProcess,
            },
            {
                .name = "nativeReleaseFileDescriptor",
                .signature = "(Ljava/io/FileDescriptor;)V",
                .fnPtr = &jniReleaseFileDescriptor
            }
    };

    return (*env)->RegisterNatives(env, process, methods, sizeof(methods) / sizeof(*methods));
}