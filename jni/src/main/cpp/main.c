#include <jni.h>
#include <limits.h>

#include "process.h"
#include "window.h"
#include "theme.h"

JNIEXPORT
JNICALL
__attribute__((unused))
jint JNI_OnLoad(JavaVM *vm, __attribute__((unused)) void *reserved) {
    JNIEnv *env = NULL;

    if ((*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_8) != JNI_OK) {
        return -1;
    }

    if (loadProcess(env)) {
        goto error;
    }

    if (loadWindow(env)) {
        goto error;
    }

    if (loadTheme(env)) {
        goto error;
    }

    return JNI_VERSION_1_8;

    error:
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
    }

    return -1;
}