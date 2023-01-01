#include "shell.hpp"

#include <array>

#include <windows.h>
#include <cstring>

namespace shell {
    bool pickFile(void *windowHandle, const std::vector<PickerFilter> &filters, std::string &path) {
        static std::string zero{"\0", 1};

        std::string filter;
        std::for_each(filters.begin(), filters.end(), [&](const PickerFilter &f) {
            filter += f.name;
            filter += zero;

            std::string exts;
            std::for_each(f.extensions.begin(), f.extensions.end(), [&](const std::string &ext) {
                exts += "*." + ext + ";";
            });
            filter += exts;
            filter += zero;
        });

        path.resize(1024);

        OPENFILENAME ofn{};

        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = reinterpret_cast<HWND>(windowHandle);
        ofn.lpstrFilter = filter.data();
        ofn.lpstrFile = path.data();
        ofn.nMaxFile = path.size() - 1;
        ofn.lpstrInitialDir = getenv("USERPROFILE");
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        if (GetOpenFileNameA(&ofn)) {
            path.resize(strlen(path.data()));

            return true;
        }

        return false;
    }

    bool launchFile(void *windowHandle, const std::string &path) {
        if ((intptr_t) ShellExecute(
                reinterpret_cast<HWND>(windowHandle),
                "open",
                path.data(),
                nullptr,
                nullptr,
                SW_SHOW
        ) > 32) {
            return true;
        }

        return false;
    }
}
