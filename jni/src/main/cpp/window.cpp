#include "window.hpp"

#include <jni.h>

namespace window {
    static void jniSetWindowBorderless(JNIEnv *env, jclass clazz, jlong handle) {
        setWindowBorderless(reinterpret_cast<void*>(handle));
    }

    static void jniSetWindowFrameSize(JNIEnv *env, jclass clazz, jlong handle, jint frame, jint size) {
        setWindowFrameSize(reinterpret_cast<void*>(handle), static_cast<WindowFrame>(frame), size);
    }

    static void jniSetWindowControlPosition(JNIEnv *env, jclass clazz, jlong handle, jint control, jint left, jint top, jint right, jint bottom) {
        setWindowControlPosition(reinterpret_cast<void*>(handle), static_cast<WindowControl>(control), left, top, right, bottom);
    }

    bool initialize(JNIEnv *env) {
        jclass clazz = env->FindClass("com/github/kr328/clash/compat/WindowCompat");

        JNINativeMethod methods[] = {
                {
                        .name = const_cast<char*>("nativeSetWindowBorderless"),
                        .signature = const_cast<char*>("(J)V"),
                        .fnPtr = reinterpret_cast<void *>(&jniSetWindowBorderless),
                },
                {
                        .name = const_cast<char*>("nativeSetWindowFrameSize"),
                        .signature = const_cast<char*>("(JII)V"),
                        .fnPtr = reinterpret_cast<void *>(&jniSetWindowFrameSize),
                },
                {
                        .name = const_cast<char*>("nativeSetWindowControlPosition"),
                        .signature = const_cast<char*>("(JIIIII)V"),
                        .fnPtr = reinterpret_cast<void*>(&jniSetWindowControlPosition),
                },
        };

        if (env->RegisterNatives(clazz, methods, sizeof(methods) / sizeof(*methods)) != JNI_OK) {
            return false;
        }

        if (!install(env)) {
            return false;
        }

        return true;
    }
}