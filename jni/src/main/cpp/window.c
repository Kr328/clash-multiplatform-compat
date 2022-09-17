#include "window.h"

#include <jni.h>
#include <stdint.h>

static void jniSetWindowBorderless(JNIEnv *env, jclass clazz, jlong handle) {
    windowSetWindowBorderless((void *) (uintptr_t) handle);
}

static void jniSetWindowFrameSize(JNIEnv *env, jclass clazz, jlong handle, jint frame, jint size) {
    windowSetWindowFrameSize((void *) (uintptr_t) handle, frame, size);
}

static void jniSetWindowControlPosition(JNIEnv *env, jclass clazz, jlong handle, jint control, jint left, jint top, jint right, jint bottom) {
    windowSetWindowControlPosition((void *) (uintptr_t) handle, control, left, top, right, bottom);
}

int loadWindow(JNIEnv *env) {
    windowInit(env);

    jclass clazz = (*env)->FindClass(env, "com/github/kr328/clash/compat/WindowCompat");

    JNINativeMethod methods[] = {
            {
                    .name = "nativeSetWindowBorderless",
                    .signature = "(J)V",
                    .fnPtr = &jniSetWindowBorderless,
            },
            {
                    .name = "nativeSetWindowFrameSize",
                    .signature = "(JII)V",
                    .fnPtr = &jniSetWindowFrameSize,
            },
            {
                    .name = "nativeSetWindowControlPosition",
                    .signature = "(JIIIII)V",
                    .fnPtr = &jniSetWindowControlPosition,
            },
    };

    if ((*env)->RegisterNatives(env, clazz, methods, sizeof(methods) / sizeof(JNINativeMethod)) != JNI_OK) {
        return 1;
    }

    return 0;
}