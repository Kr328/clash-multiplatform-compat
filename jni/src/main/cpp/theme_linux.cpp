#include "theme.hpp"

#include "utils.hpp"

#include <dbus/dbus.h>
#include <thread>
#include <utility>

namespace theme {
    class MonitorDisposable : public Disposable {
    private:
        DBusConnection *connection;

    public:
        explicit MonitorDisposable(DBusConnection *connection) : connection(connection) {}

        ~MonitorDisposable() override {
            Disposable::~Disposable();

            dbus_connection_close(connection);
        }
    };

    static bool readColorScheme(uint32_t *value) {
        DBusConnection *conn = dbus_bus_get(DBUS_BUS_SESSION, nullptr);
        if (conn == nullptr) {
            return false;
        }

        utils::Scoped<DBusMessage *> request{
                dbus_message_new_method_call(
                        "org.freedesktop.portal.Desktop",
                        "/org/freedesktop/portal/desktop",
                        "org.freedesktop.portal.Settings",
                        "Read"
                ),
                dbus_message_unref,
        };
        if (request == nullptr) {
            return false;
        }

        const char *appearance = "org.freedesktop.appearance";
        const char *colorScheme = "color-scheme";

        DBusMessageIter args;
        dbus_message_iter_init_append(request, &args);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &appearance);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &colorScheme);

        utils::Scoped<DBusMessage *> reply{
                dbus_connection_send_with_reply_and_block(
                        conn,
                        request,
                        DBUS_TIMEOUT_INFINITE,
                        nullptr
                ),
                dbus_message_unref,
        };
        if (reply == nullptr || !dbus_message_iter_init(reply, &args) ||
            dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_VARIANT) {
            return false;
        }

        DBusMessageIter outer;
        dbus_message_iter_recurse(&args, &outer);
        if (dbus_message_iter_get_arg_type(&outer) != DBUS_TYPE_VARIANT) {
            return false;
        }

        DBusMessageIter inner;
        dbus_message_iter_recurse(&outer, &inner);
        if (dbus_message_iter_get_arg_type(&inner) != DBUS_TYPE_UINT32) {
            return -1;
        }

        dbus_message_iter_get_basic(&inner, value);

        return true;
    }

    bool isSupported() {
        uint32_t value;

        return readColorScheme(&value);
    }

    bool isNight() {
        uint32_t value;

        if (!readColorScheme(&value)) {
            return false;
        }

        return value == 1;
    }

    std::unique_ptr<Disposable> monitor(std::function<void()> changed) {
        DBusConnection *conn = dbus_bus_get_private(DBUS_BUS_SESSION, nullptr);
        if (conn == nullptr) {
            return nullptr;
        }

        dbus_bus_add_match(
                conn,
                "type='signal',sender='org.freedesktop.portal.Desktop',interface='org.freedesktop.portal.Settings',path='/org/freedesktop/portal/desktop',member='SettingChanged',arg0='org.freedesktop.appearance'",
                nullptr
        );
        dbus_connection_flush(conn);

        std::thread thread{
                [changed = std::move(changed), c = conn]() {
                    utils::Scoped<DBusConnection *> conn{c, dbus_connection_close};

                    while (dbus_connection_get_is_connected(conn)) {
                        dbus_connection_read_write(conn, DBUS_TIMEOUT_INFINITE);

                        utils::Scoped<DBusMessage *> message{dbus_connection_pop_message(conn), dbus_message_unref};
                        if (message == nullptr) {
                            continue;
                        }

                        if (dbus_message_is_signal(message, "org.freedesktop.portal.Settings",
                                                   "SettingChanged")) {
                            changed();
                        }
                    }
                }
        };
        thread.detach();

        return std::make_unique<MonitorDisposable>(conn);
    }
}
