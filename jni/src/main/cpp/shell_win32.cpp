#include "shell.hpp"

#include <array>

#include <windows.h>
#include <cstring>

namespace shell {
    bool pickFile(void *windowHandle, const std::string &windowTitle, const std::vector<PickerFilter> &filters, std::string &path) {
        static std::string zero{"\0", 1};

        std::string filterExpr;
        for (const auto &filter: filters) {
            filterExpr += filter.name;
            filterExpr += zero;

            std::string extensions;
            for (const auto &ext: filter.extensions) {
                extensions += "*." + ext + ";";
            }

            filterExpr += extensions;
            filterExpr += zero;
        }

        path.resize(1024);

        OPENFILENAME ofn{};

        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = reinterpret_cast<HWND>(windowHandle);
        ofn.lpstrTitle = windowTitle.data();
        ofn.lpstrFilter = filterExpr.data();
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
