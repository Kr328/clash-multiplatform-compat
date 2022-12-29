#include "theme.hpp"

#include "jniutils.hpp"

namespace theme {
    static jmethodID mOnChanged;

    struct monitorHolder {
        std::unique_ptr<Disposable> disposable;
    };

    static jboolean jniIsSupported(JNIEnv *env) {
        return isSupported();
    }

    static jboolean jniIsNight(JNIEnv *env, jclass clazz) {
        return isNight();
    }

    static jlong jniMonitor(JNIEnv *env, jclass clazz, jobject listener) {
        listener = env->NewGlobalRef(listener);

        std::unique_ptr<Disposable> disposable = monitor([listener]() {
            jniutils::AttachedEnv env{jniutils::currentJavaVM()};

            env->CallVoidMethod(listener, mOnChanged);
        });

        return reinterpret_cast<jlong>(new monitorHolder{std::move(disposable)});
    }

    static void jniDisposeMonitor(JNIEnv *env, jclass clazz, jlong ptr) {
        auto holder = reinterpret_cast<monitorHolder*>(ptr);

        holder->disposable = nullptr;
    }

    static void jniReleaseMonitor(JNIEnv *env, jclass clazz, jlong ptr) {
        delete reinterpret_cast<monitorHolder*>(ptr);
    }

    bool initialize(JNIEnv *env) {
        jclass compat = env->FindClass("com/github/kr328/clash/compat/ThemeCompat");
        jclass listener = env->FindClass("com/github/kr328/clash/compat/ThemeCompat$OnThemeChangedListener");

        mOnChanged = env->GetMethodID(listener, "onChanged", "()V");
        if (mOnChanged == nullptr) {
            return false;
        }

        JNINativeMethod methods[] = {
                {
                        .name = const_cast<char*>("nativeIsSupported"),
                        .signature = const_cast<char*>("()Z"),
                        .fnPtr = reinterpret_cast<void *>(&jniIsSupported),
                },
                {
                        .name = const_cast<char*>("nativeIsNight"),
                        .signature = const_cast<char*>("()Z"),
                        .fnPtr = reinterpret_cast<void*>(&jniIsNight),
                },
                {
                    .name = const_cast<char*>("nativeMonitor"),
                    .signature = const_cast<char*>("(Lcom/github/kr328/clash/compat/ThemeCompat$OnThemeChangedListener;)J"),
                    .fnPtr = reinterpret_cast<void*>(&jniMonitor),
                },
                {
                        .name = const_cast<char*>("nativeDisposeMonitor"),
                        .signature = const_cast<char*>("(J)V"),
                        .fnPtr = reinterpret_cast<void*>(&jniDisposeMonitor),
                },
                {
                        .name = const_cast<char*>("nativeReleaseMonitor"),
                        .signature = const_cast<char*>("(J)V"),
                        .fnPtr = reinterpret_cast<void*>(&jniReleaseMonitor),
                },
        };

        if (env->RegisterNatives(compat, methods, sizeof(methods) / sizeof(*methods)) != JNI_OK) {
            return false;
        }

        return true;
    }
}
