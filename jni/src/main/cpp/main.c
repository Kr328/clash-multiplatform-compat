#include <jni.h>
#include <limits.h>

#include "window.h"
#include "theme.h"

JNIEXPORT
__attribute__((unused))
jint JNI_OnLoad(JavaVM *vm, __attribute__((unused)) void *reserved) {
    JNIEnv *env = NULL;

    if ((*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_8) != JNI_OK) {
        return INT_MAX;
    }

    if (windowOnLoaded(env)) {
        return INT_MAX;
    }

    if (themeOnLoaded(env)) {
        return INT_MAX;
    }

    return JNI_VERSION_1_8;
}