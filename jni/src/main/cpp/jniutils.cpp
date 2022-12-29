#include "jniutils.hpp"

namespace jniutils {
    static JavaVM *javaVm;
    static jclass cString;
    static jmethodID mGetBytes;
    static jmethodID mNewString;
    static jobject oCharsetsUTF8;

    bool initialize(JNIEnv *env) {
        env->GetJavaVM(&javaVm);

        cString = reinterpret_cast<jclass>(env->NewGlobalRef(env->FindClass("java/lang/String")));
        if (cString == nullptr) {
            return false;
        }

        mGetBytes = env->GetMethodID(cString, "getBytes", "(Ljava/nio/charset/Charset;)[B");
        if (mGetBytes == nullptr) {
            return false;
        }

        mNewString = env->GetMethodID(cString, "<init>", "([BLjava/nio/charset/Charset;)V");
        if (mNewString == nullptr) {
            return false;
        }

        jclass cStandardCharsets = env->FindClass("java/nio/charset/StandardCharsets");
        if (cStandardCharsets == nullptr) {
            return false;
        }

        jfieldID fUTF8 = env->GetStaticFieldID(cStandardCharsets, "UTF_8", "Ljava/nio/charset/Charset;");
        if (fUTF8 == nullptr) {
            return false;
        }

        oCharsetsUTF8 = env->NewGlobalRef(env->GetStaticObjectField(cStandardCharsets, fUTF8));
        if (oCharsetsUTF8 == nullptr) {
            return false;
        }

        return true;
    }

    JavaVM *currentJavaVM() {
        return javaVm;
    }

    std::string getString(JNIEnv *env, jstring str) {
        auto bytes = reinterpret_cast<jbyteArray>(env->CallObjectMethod(str, mGetBytes, oCharsetsUTF8));
        std::string out;
        out.resize(env->GetArrayLength(bytes));
        env->GetByteArrayRegion(bytes, 0, static_cast<jsize>(out.size()), reinterpret_cast<jbyte *>(out.data()));
        return out;
    }

    jstring newString(JNIEnv *env, const std::string &str) {
        jbyteArray array = env->NewByteArray(static_cast<jsize>(str.size()));

        env->SetByteArrayRegion(array, 0, static_cast<jsize>(str.size()), reinterpret_cast<const jbyte *>(str.data()));

        return reinterpret_cast<jstring>(env->NewObject(cString, mNewString, array, oCharsetsUTF8));
    }

    jniutils::ArrayIterator<jobjectArray, jobject> begin(JNIEnv *env, jobjectArray array) {
        return {env, array, 0};
    }

    jniutils::ArrayIterator<jobjectArray, jobject> end(JNIEnv *env, jobjectArray array) {
        jsize length = env->GetArrayLength(array);

        return {env, array, length};
    }
}
