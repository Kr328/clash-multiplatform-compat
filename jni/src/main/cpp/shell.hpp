#pragma once

#include <jni.h>

#include <string>
#include <vector>

namespace shell {
    struct PickerFilter {
        std::string name;
        std::vector<std::string> extensions;
    };

    bool initialize(JNIEnv *env);

    bool pickFile(void *windowHandle, const std::string &windowTitle, const std::vector<PickerFilter> &filters, std::string &path);
    bool launchFile(void *windowHandle, const std::string &path);
}
