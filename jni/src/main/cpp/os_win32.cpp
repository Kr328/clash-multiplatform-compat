#include "os.hpp"

#include <windows.h>

namespace os {
    std::string getLastError() {
        DWORD err = GetLastError();
        if (err == 0) {
            return {};
        }

        LPSTR msg = nullptr;

        size_t size = FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                err,
                MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
                (LPSTR) &msg,
                0,
                nullptr
        );

        std::string ret{msg, size};

        LocalFree(msg);

        return ret;
    }
}
