#include "theme.h"

static JavaVM *javaVm;
static jclass classThemeCompat;
static jmethodID methodOnChanged;

static jboolean jniIsSupported(JNIEnv *env) {
    return themeIsSupported();
}

static jboolean jniIsNightMode(JNIEnv *env, jclass clazz) {
    return themeIsNightMode();
}

static void onThemeChanged() {
    jclass clazz = classThemeCompat;
    jmethodID method = methodOnChanged;

    if (clazz == NULL || method == NULL) {
        return;
    }

    JNIEnv *env = NULL;

    if ((*javaVm)->AttachCurrentThread(javaVm, (void **) &env, NULL) != JNI_OK) {
        return;
    }

    (*env)->CallStaticVoidMethod(env, classThemeCompat, methodOnChanged);

    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }

    (*javaVm)->DetachCurrentThread(javaVm);
}

int loadTheme(JNIEnv *env) {
    if (themeInit(&onThemeChanged)) {
        return 1;
    }

    if ((*env)->GetJavaVM(env, &javaVm) != JNI_OK) {
        return 1;
    }

    jclass clazz = (*env)->FindClass(env, "com/github/kr328/clash/compat/ThemeCompat");

    JNINativeMethod methods[] = {
            {
                    .name = "nativeIsSupported",
                    .signature = "()Z",
                    .fnPtr = &jniIsSupported,
            },
            {
                    .name = "nativeIsNightMode",
                    .signature = "()Z",
                    .fnPtr = &jniIsNightMode,
            },
    };

    if ((*env)->RegisterNatives(env, clazz, methods, sizeof(methods) / sizeof(JNINativeMethod)) != JNI_OK) {
        return 1;
    }

    classThemeCompat = (*env)->NewGlobalRef(env, clazz);
    if (classThemeCompat == NULL) {
        return 1;
    }

    methodOnChanged = (*env)->GetStaticMethodID(env, clazz, "onThemeChanged", "()V");
    if (methodOnChanged == NULL) {
        return 1;
    }

    return 0;
}