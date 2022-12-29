#pragma once

#include "jniutils.hpp"

#include <jni.h>

#include <string>
#include <vector>
#include <cstdint>

#if defined(__WIN32__)
#include <windows.h>
#elif defined(__linux__)
#include <fcntl.h>
#endif

namespace process {
#if defined(__WIN32__)
    using ResourceHandle = HANDLE;
    static const ResourceHandle InvalidResourceHandle = INVALID_HANDLE_VALUE;
    inline static ResourceHandle fromJLong(jlong value) { return reinterpret_cast<ResourceHandle>(value); }
#elif defined(__linux__)
    using ResourceHandle = int;
    static const ResourceHandle InvalidResourceHandle = -1;
    inline static ResourceHandle fromJLong(jlong value) { return static_cast<ResourceHandle>(value); }
#endif

    bool initialize(JNIEnv *env);
    bool create(
            const std::string &path,
            const std::vector<std::string> &args,
            const std::string &workingDir,
            const std::vector<std::string> &environments,
            ResourceHandle *handle,
            ResourceHandle *fdStdin,
            ResourceHandle *fdStdout,
            ResourceHandle *fdStderr
    );
    int wait(ResourceHandle handle);
    void terminate(ResourceHandle handle);
    void release(ResourceHandle handle);
}
