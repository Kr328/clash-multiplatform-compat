#include "theme.hpp"

#include <windows.h>

#include <thread>
#include <atomic>
#include <utility>

namespace theme {
    static HKEY personalizeKey = []() -> HKEY {
        HKEY key = nullptr;

        RegOpenKeyExA(
                HKEY_CURRENT_USER,
                R"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)",
                0,
                KEY_READ,
                &key
        );

        return key;
    }();

    struct Monitor {
        std::atomic_bool closed{false};
        HANDLE event = CreateEventA(nullptr, true, false, nullptr);
    };

    class MonitorDisposable : public Disposable {
    private:
        std::shared_ptr<Monitor> monitor;
    public:
        explicit MonitorDisposable(const std::shared_ptr<Monitor> &monitor) : monitor(monitor) {}

        ~MonitorDisposable() override {
            Disposable::~Disposable();

            monitor->closed = true;

            SetEvent(monitor->event);
        }
    };

    bool isSupported() {
        return personalizeKey != nullptr;
    }

    bool isNight() {
        DWORD result = 1;
        DWORD resultLength = sizeof(result);

        LRESULT r = RegQueryValueExA(
                personalizeKey,
                "AppsUseLightTheme",
                nullptr,
                nullptr,
                (LPBYTE) &result,
                &resultLength
        );
        if (r != ERROR_SUCCESS) {
            return false;
        }

        return result == 0;
    }

    std::unique_ptr<Disposable> monitor(std::function<void()> changed) {
        DWORD type = 0;
        LRESULT r = RegQueryValueExA(personalizeKey, "SystemUsesLightTheme", nullptr, &type, nullptr, nullptr);
        if (r != ERROR_SUCCESS || type != REG_DWORD) {
            return nullptr;
        }

        std::shared_ptr<Monitor> holder = std::make_shared<Monitor>();

        std::thread thread{
                [changed = std::move(changed), holder]() {
                    while (!holder->closed) {
                        LRESULT r = RegNotifyChangeKeyValue(
                                personalizeKey,
                                false,
                                REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_CHANGE_NAME,
                                holder->event,
                                true
                        );
                        if (r != ERROR_SUCCESS) {
                            return;
                        }

                        WaitForSingleObject(holder->event, INFINITE);

                        if (!holder->closed) {
                            changed();

                            ResetEvent(holder->event);
                        }
                    }
                }
        };
        thread.detach();

        return std::make_unique<MonitorDisposable>(holder);
    }
}
