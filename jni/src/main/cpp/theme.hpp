#pragma once

#include <jni.h>

#include <memory>
#include <functional>

namespace theme {
    class Disposable {
    public:
        virtual ~Disposable() = default;
    };

    bool initialize(JNIEnv *env);

    bool isSupported();
    bool isNight();
    std::unique_ptr<Disposable> monitor(std::function<void ()> changed);
}

