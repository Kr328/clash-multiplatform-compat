#include "shell.hpp"

#include "jniutils.hpp"

#include <vector>

namespace shell {
    static jfieldID fName;
    static jfieldID fExtensions;

    static jstring jniPickFile(JNIEnv *env, jclass clazz, jlong windowHandle, jstring windowTitle, jobjectArray filters) {
        std::vector<PickerFilter> cFilters;

        std::for_each(jniutils::begin(env, filters), jniutils::end(env, filters), [&](jobject filter) {
            auto filterName = reinterpret_cast<jstring>(env->GetObjectField(filter, fName));

            PickerFilter cFilter{
                .name = jniutils::getString(env, filterName),
            };

            auto filterExtensions = reinterpret_cast<jobjectArray>(env->GetObjectField(filter, fExtensions));
            std::for_each(jniutils::begin(env, filterExtensions), jniutils::end(env, filterExtensions), [&](jobject ext) {
                cFilter.extensions.emplace_back(jniutils::getString(env, reinterpret_cast<jstring>(ext)));
            });

            cFilters.emplace_back(cFilter);
        });

        std::string cTitle = jniutils::getString(env, windowTitle);
        std::string cPath;

        if (pickFile(reinterpret_cast<void*>(windowHandle), cTitle, cFilters, cPath)) {
            return jniutils::newString(env, cPath);
        }

        return nullptr;
    }

    static void jniLaunchFile(JNIEnv *env, jclass clazz, jlong windowHandle, jstring path) {
        std::string cPath = jniutils::getString(env, path);

        launchFile(reinterpret_cast<void*>(windowHandle), cPath);
    }

    bool initialize(JNIEnv *env) {
        jclass cShell = env->FindClass("com/github/kr328/clash/compat/ShellCompat");
        if (cShell == nullptr) {
            return false;
        }

        jclass cPickerFilter = env->FindClass("com/github/kr328/clash/compat/ShellCompat$NativePickerFilter");
        if (cPickerFilter == nullptr) {
            return false;
        }

        fName = env->GetFieldID(cPickerFilter, "name", "Ljava/lang/String;");
        if (fName == nullptr) {
            return false;
        }

        fExtensions = env->GetFieldID(cPickerFilter, "extensions", "[Ljava/lang/String;");
        if (fExtensions == nullptr) {
            return false;
        }

        JNINativeMethod methods[] = {
                {
                        .name = const_cast<char*>("nativePickFile"),
                        .signature = const_cast<char*>("(JLjava/lang/String;[Lcom/github/kr328/clash/compat/ShellCompat$NativePickerFilter;)Ljava/lang/String;"),
                        .fnPtr = reinterpret_cast<void *>(&jniPickFile)
                },
                {
                        .name = const_cast<char*>("nativeLaunchFile"),
                        .signature = const_cast<char*>("(JLjava/lang/String;)V"),
                        .fnPtr = reinterpret_cast<void *>(&jniLaunchFile),
                }
        };

        if (env->RegisterNatives(cShell, methods, sizeof(methods) / sizeof(*methods)) != JNI_OK) {
            return false;
        }

        return true;
    }
}
