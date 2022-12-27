#include <jni.h>

#include "jniutils.hpp"
#include "process.hpp"
#include "window.hpp"
#include "theme.hpp"
#include "shell.hpp"

[[maybe_unused]]
JNIEXPORT
JNICALL
jint JNI_OnLoad(JavaVM *vm, [[maybe_unused]] void *reserved) {
    JNIEnv *env = nullptr;

    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_8) != JNI_OK) {
        return -1;
    }

    if (!jniutils::initialize(env)) {
        goto error;
    }

    if (!process::initialize(env)) {
        goto error;
    }

    if (!window::initialize(env)) {
        goto error;
    }

    if (!theme::initialize(env)) {
        goto error;
    }

    if (!shell::initialize(env)) {
        goto error;
    }

    return JNI_VERSION_1_8;

    error:
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
    }

    return -1;
}