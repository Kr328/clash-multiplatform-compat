#pragma once

#include <functional>

namespace utils {
    template<class T>
    class Scoped {
    private:
        T value;
        std::function<void(T)> destroy;

    public:
        Scoped(T value, const std::function<void(T)> &destroy) : value(value), destroy(destroy) {}
        ~Scoped() { destroy(value); }

    public:
        operator T() { // NOLINT(google-explicit-constructor)
            return value;
        }

        Scoped &operator=(T v) {
            value = v;

            return *this;
        }
    };
}
