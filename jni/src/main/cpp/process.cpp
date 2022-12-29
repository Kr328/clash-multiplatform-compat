#include "process.hpp"

#include "os.hpp"

#include <vector>

namespace process {
    static jfieldID fFileDescriptorFd;
    static jfieldID fFileDescriptorHandle;
    static jmethodID mFileDescriptorClose;

    static jlong jniCreateProcess(
            JNIEnv *env,
            jclass clazz,
            jstring path,
            jobjectArray args,
            jstring workingDir,
            jobjectArray environments,
            jobject fdStdin,
            jobject fdStdout,
            jobject fdStderr
    ) {
        std::string cPath = jniutils::getString(env, path);

        std::vector<std::string> cArgs;
        std::for_each(jniutils::begin(env, args), jniutils::end(env, args), [&](jobject a) {
            cArgs.push_back(jniutils::getString(env, reinterpret_cast<jstring>(a)));
        });

        std::string cWorkingDir = jniutils::getString(env, workingDir);

        std::vector<std::string> cEnvironments;
        std::for_each(jniutils::begin(env, environments), jniutils::end(env, environments), [&](jobject e) {
            cEnvironments.push_back(jniutils::getString(env, reinterpret_cast<jstring>(e)));
        });

        ResourceHandle hProcess = InvalidResourceHandle;
        ResourceHandle hStdin = InvalidResourceHandle;
        ResourceHandle hStdout = InvalidResourceHandle;
        ResourceHandle hStderr = InvalidResourceHandle;

        bool success = create(
                cPath,
                cArgs,
                cWorkingDir,
                cEnvironments,
                &hProcess,
                fdStdin != nullptr ? &hStdin : nullptr,
                fdStdout != nullptr ? &hStdout : nullptr,
                fdStderr != nullptr ? &hStderr : nullptr
        );

        if (!success) {
            std::string error = os::getLastError();

            env->ThrowNew(env->FindClass("java/io/IOException"), error.data());

            return -1;
        }

#if defined(__WIN32__)
        if (fdStdin != nullptr) {
            env->SetLongField(fdStdin, fFileDescriptorHandle, reinterpret_cast<jlong>(hStdin));
        }
        if (fdStdout != nullptr) {
            env->SetLongField(fdStdout, fFileDescriptorHandle, reinterpret_cast<jlong>(hStdout));
        }
        if (fdStderr != nullptr) {
            env->SetLongField(fdStderr, fFileDescriptorHandle, reinterpret_cast<jlong>(hStderr));
        }
#elif defined(__linux__)
        if (fdStdin != nullptr) {
            env->SetIntField(fdStdin, fFileDescriptorFd, (jint) hStdin);
        }
        if (fdStdout != nullptr) {
            env->SetIntField(fdStdout, fFileDescriptorFd, (jint) hStdout);
        }
        if (fdStderr != nullptr) {
            env->SetIntField(fdStderr, fFileDescriptorFd, (jint) hStderr);
        }
#endif

        return (jlong) hProcess;
    }

    static jint jniWaitProcess(JNIEnv *env, jclass clazz, jlong handle) {
        return wait(fromJLong(handle));
    }

    static void jniTerminateProcess(JNIEnv *env, jclass clazz, jlong handle) {
        terminate(fromJLong(handle));
    }

    static void jniReleaseProcess(JNIEnv *env, jclass clazz, jlong handle) {
        release(fromJLong(handle));
    }

    static void jniReleaseFileDescriptor(JNIEnv *env, jclass clazz, jobject fd) {
        env->CallVoidMethod(fd, mFileDescriptorClose);
    }

    bool initialize(JNIEnv *env) {
        jclass clazzFileDescriptor = env->FindClass("java/io/FileDescriptor");
        if (clazzFileDescriptor == nullptr) {
            return false;
        }

        fFileDescriptorFd = env->GetFieldID(clazzFileDescriptor, "fd", "I");
        if (fFileDescriptorFd == nullptr) {
            return false;
        }

        fFileDescriptorHandle = env->GetFieldID(clazzFileDescriptor, "handle", "J");
        if (fFileDescriptorHandle == nullptr) {
            return false;
        }

        mFileDescriptorClose = env->GetMethodID(clazzFileDescriptor, "close", "()V");
        if (mFileDescriptorClose == nullptr) {
            return false;
        }

        jclass process = env->FindClass("com/github/kr328/clash/compat/ProcessCompat");
        if (process == nullptr) {
            return false;
        }

        const JNINativeMethod methods[] = {
                {
                        .name = const_cast<char *>("nativeCreateProcess"),
                        .signature = const_cast<char *>("(Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;Ljava/io/FileDescriptor;Ljava/io/FileDescriptor;Ljava/io/FileDescriptor;)J"),
                        .fnPtr = reinterpret_cast<void *>(&jniCreateProcess),
                },
                {
                        .name = const_cast<char *>("nativeWaitProcess"),
                        .signature = const_cast<char *>("(J)I"),
                        .fnPtr = reinterpret_cast<void *>(&jniWaitProcess),
                },
                {
                        .name = const_cast<char *>("nativeTerminateProcess"),
                        .signature = const_cast<char *>("(J)V"),
                        .fnPtr = reinterpret_cast<void *>(&jniTerminateProcess),
                },
                {
                        .name = const_cast<char *>("nativeReleaseProcess"),
                        .signature = const_cast<char *>("(J)V"),
                        .fnPtr = reinterpret_cast<void *>(&jniReleaseProcess),
                },
                {
                        .name = const_cast<char *>("nativeReleaseFileDescriptor"),
                        .signature = const_cast<char *>("(Ljava/io/FileDescriptor;)V"),
                        .fnPtr = reinterpret_cast<void *>(&jniReleaseFileDescriptor)
                }
        };

        if (env->RegisterNatives(process, methods, sizeof(methods) / sizeof(*methods)) != JNI_OK) {
            return false;
        }

        return true;
    }
}
