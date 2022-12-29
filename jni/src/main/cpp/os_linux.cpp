#include "os.hpp"

#include <cerrno>
#include <cstring>

namespace os {
    std::string getLastError() {
        return strerror(errno);
    }
}
