#pragma once

#include <jni.h>

#include <string>
#include <functional>

namespace jniutils {
    template<class Array, class Element>
    class ArrayIterator {
    private:
        static jobject get(JNIEnv *env, jobjectArray array, jsize index) {
            return env->GetObjectArrayElement(array, index);
        }

    private:
        JNIEnv *env;
        Array array;
        jsize index;

    public:
        ArrayIterator(JNIEnv *env, Array array, jsize initial) : env(env), array(array), index(initial) {}

        ArrayIterator &operator++() {
            index++;
            return *this;
        }

        ArrayIterator operator++(int) { // NOLINT(cert-dcl21-cpp)
            auto ret = *this;
            ++(*this);
            return ret;
        }

        bool operator==(ArrayIterator const &other) const {
            return index == other.index;
        }

        bool operator!=(ArrayIterator const &other) const {
            return index != other.index;
        }

        Element operator*() {
            return get(env, array, index);
        }

        using difference_type = Element;
        using value_type = Element;
        using pointer = const Element *;
        using reference = const Element &;
        using iterator_category = std::random_access_iterator_tag;
    };

    class AttachedEnv {
    private:
        JavaVM *vm;
        JNIEnv *env;

    public:
        explicit AttachedEnv(JavaVM *vm) : vm(vm), env(nullptr) {
            vm->AttachCurrentThread(reinterpret_cast<void **>(&this->env), nullptr);
        }

        ~AttachedEnv() {
            vm->DetachCurrentThread();
        }

    public:
        JNIEnv *operator->() {
            return env;
        }
    };

    bool initialize(JNIEnv *env);
    JavaVM *currentJavaVM();

    std::string getString(JNIEnv *env, jstring str);
    jstring newString(JNIEnv *env, std::string const &str);

    ArrayIterator<jobjectArray, jobject> begin(JNIEnv *env, jobjectArray array);
    ArrayIterator<jobjectArray, jobject> end(JNIEnv *env, jobjectArray array);
}

