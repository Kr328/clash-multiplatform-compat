#include "shell.h"

#include <limits.h>
#include <string.h>
#include <stdlib.h>

static jfieldID fieldName;
static jfieldID fieldExtensions;

static jboolean jniIsSupported(JNIEnv *env, jclass clazz) {
    return (jboolean) shellIsSupported();
}

static jbyteArray jniPickFile(JNIEnv *env, jclass clazz, jlong windowHandle, jobjectArray filters) {
    int filtersLength = (*env)->GetArrayLength(env, filters);
    struct PickerFilter *cFilters = malloc(sizeof(struct PickerFilter) * filtersLength);

    for (int filterIdx = 0; filterIdx < filtersLength ; filterIdx++) {
        struct PickerFilter *cFilter = &cFilters[filterIdx];

        jobject jFilter = (*env)->GetObjectArrayElement(env, filters, filterIdx);

        jbyteArray jName = (*env)->GetObjectField(env, jFilter, fieldName);
        int nameSize = (*env)->GetArrayLength(env, jName);

        cFilter->name = malloc(nameSize + 1);
        (*env)->GetByteArrayRegion(env, jName, 0, nameSize, (jbyte *) cFilter->name);
        cFilter->name[nameSize] = 0;

        jobjectArray jExtensions = (*env)->GetObjectField(env, jFilter, fieldExtensions);
        int extensionsSize = (*env)->GetArrayLength(env, jExtensions);
        cFilter->extensions = malloc(sizeof(char *) * (extensionsSize + 1));

        for (int extensionIdx = 0 ; extensionIdx < extensionsSize ; extensionIdx++ ) {
            jstring jExtension = (*env)->GetObjectArrayElement(env, jExtensions, extensionIdx);
            cFilter->extensions[extensionIdx] = (*env)->GetStringUTFChars(env, jExtension, NULL);
        }

        cFilter->extensions[extensionsSize] = NULL;
    }

    char path[PATH_MAX] = {0};

    int failed = shellPickFile((void *) windowHandle, cFilters, filtersLength, path, sizeof(path));

    for (int filterIdx = 0; filterIdx < filtersLength ; filterIdx++) {
        struct PickerFilter *cFilter = &cFilters[filterIdx];

        jobject jFilter = (*env)->GetObjectArrayElement(env, filters, filterIdx);

        free(cFilter->name);

        jobjectArray jExtensions = (*env)->GetObjectField(env, jFilter, fieldExtensions);

        int extensionsSize = (*env)->GetArrayLength(env, jExtensions);
        for (int extensionIdx = 0 ; extensionIdx < extensionsSize ; extensionIdx++ ) {
            jstring jExtension = (*env)->GetObjectArrayElement(env, jExtensions, extensionIdx);

            (*env)->ReleaseStringUTFChars(env, jExtension, cFilter->extensions[extensionIdx]);
        }

        free(cFilter->extensions);
    }

    free(cFilters);

    if (failed) {
        return NULL;
    }

    jsize length = (jsize) strlen(path);

    jbyteArray jPath = (*env)->NewByteArray(env, length);

    (*env)->SetByteArrayRegion(env, jPath, 0, length, (jbyte*) path);

    return jPath;
}

static void jniLaunchOpenFile(JNIEnv *env, jclass clazz, jlong windowHandle, jbyteArray jPath) {
    char path[PATH_MAX] = {0};

    jsize length = (*env)->GetArrayLength(env, jPath);
    if (length > (PATH_MAX - 1)) {
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/io/IOException"), "Path too long");

        return;
    }

    (*env)->GetByteArrayRegion(env, jPath, 0, length, (jbyte *) path);

    path[length] = 0;

    shellLaunchOpenFile((void *) windowHandle, path);
}

int loadShell(JNIEnv *env) {
    jclass cShell = (*env)->FindClass(env, "com/github/kr328/clash/compat/ShellCompat");
    if (cShell == NULL) {
        return 1;
    }

    jclass cPickerFilter = (*env)->FindClass(env, "com/github/kr328/clash/compat/ShellCompat$NativePickerFilter");
    if (cPickerFilter == NULL) {
        return 1;
    }

    fieldName = (*env)->GetFieldID(env, cPickerFilter, "name", "[B");
    if (fieldName == NULL) {
        return 1;
    }

    fieldExtensions = (*env)->GetFieldID(env, cPickerFilter, "extensions", "[Ljava/lang/String;");
    if (fieldExtensions == NULL) {
        return 1;
    }

    JNINativeMethod methods[] = {
            {
                .name = "nativeIsSupported",
                .signature = "()Z",
                .fnPtr = &jniIsSupported,
            },
            {
                .name = "nativePickFile",
                .signature = "(J[Lcom/github/kr328/clash/compat/ShellCompat$NativePickerFilter;)[B",
                .fnPtr = &jniPickFile
            },
            {
                .name = "nativeLaunchOpenFile",
                .signature = "(J[B)V",
                .fnPtr = &jniLaunchOpenFile,
            }
    };

    if ((*env)->RegisterNatives(env, cShell, methods, sizeof(methods) / sizeof(*methods)) != JNI_OK) {
        return 1;
    }

    return 0;
}
