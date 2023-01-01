#include "shell.hpp"

#include "utils.hpp"
#include "dbus.hpp"

#include <dbus/dbus.h>
#include <fcntl.h>
#include <unistd.h>

namespace shell {
    bool pickFile(
            void *windowHandle,
            const std::string &windowTitle,
            const std::vector<PickerFilter> &filters,
            std::string &path
    ) {
        utils::Scoped<DBusConnection *> conn{
                dbus_bus_get_private(DBUS_BUS_SESSION, nullptr),
                dbus_connection_close,
        };

        utils::Scoped<DBusMessage *> request{
                dbus_message_new_method_call(
                        "org.freedesktop.portal.Desktop",
                        "/org/freedesktop/portal/desktop",
                        "org.freedesktop.portal.FileChooser",
                        "OpenFile"
                ),
                dbus_message_unref,
        };
        if (request == nullptr) {
            return false;
        }

        char parentWindow[64] = {0};
        std::sprintf(parentWindow, "x11:%lx", reinterpret_cast<long>(windowHandle));

        dbus::MessageBuilder args{request};
        args.writeString(parentWindow); // parent id
        args.writeString(windowTitle);  // window windowTitle
        args.inner(DBUS_TYPE_ARRAY, "{sv}", [&](dbus::MessageBuilder &b) {
            b.inner(DBUS_TYPE_DICT_ENTRY, nullptr, [&](dbus::MessageBuilder &b) {
                b.writeString("filters");
                b.inner(DBUS_TYPE_VARIANT, "a(sa(us))", [&](dbus::MessageBuilder &b) {
                    b.inner(DBUS_TYPE_ARRAY, "(sa(us))", [&](dbus::MessageBuilder &b) {
                        for (const auto &filter: filters) {
                            b.inner(DBUS_TYPE_STRUCT, nullptr, [&](dbus::MessageBuilder &b) {
                                b.writeString(filter.name);
                                b.inner(DBUS_TYPE_ARRAY, "(us)", [&](dbus::MessageBuilder &b) {
                                    for (const auto &ext: filter.extensions) {
                                        b.inner(DBUS_TYPE_STRUCT, nullptr, [&](dbus::MessageBuilder &b) {
                                            b.writeUInt32(static_cast<uint32_t>(0));
                                            b.writeString("*." + ext);
                                        });
                                    }
                                });
                            });
                        }
                    });
                });
            });
        });

        utils::Scoped<DBusMessage *> reply{
                dbus_connection_send_with_reply_and_block(
                        conn,
                        request,
                        DBUS_TIMEOUT_INFINITE,
                        nullptr
                ),
                dbus_message_unref,
        };
        if (reply == nullptr) {
            return false;
        }

        dbus::MessageExtractor response{reply};

        std::string responsePath;
        if (!response.readObjectPath(responsePath)) {
            return false;
        }

        dbus_bus_add_match(
                conn,
                "type='signal'",
                nullptr
        );
        dbus_connection_flush(conn);

        while (true) {
            dbus_connection_read_write(conn, DBUS_TIMEOUT_INFINITE);

            utils::Scoped<DBusMessage *> signal{dbus_connection_pop_message(conn), dbus_message_unref};
            if (signal == nullptr) {
                continue;
            }

            if (!dbus_message_is_signal(signal, "org.freedesktop.portal.Request", "Response")) {
                continue;
            }

            if (responsePath != dbus_message_get_path(signal)) {
                continue;
            }

            dbus::MessageExtractor body{signal};
            uint32_t responseCode = -1;
            if (!body.readUInt32(responseCode) || responseCode != 0) {
                return -1;
            }

            std::string uri;

            bool ret = body.inner(DBUS_TYPE_ARRAY, [&](dbus::MessageExtractor &ex) -> bool {
                while (true) {
                    bool ret = ex.inner(DBUS_TYPE_DICT_ENTRY, [&](dbus::MessageExtractor &ex) -> bool {
                        std::string key;

                        if (!ex.readString(key)) {
                            return false;
                        }

                        if (key == "uris") {
                            return ex.inner(DBUS_TYPE_VARIANT, [&](dbus::MessageExtractor &ex) -> bool {
                                return ex.inner(DBUS_TYPE_ARRAY, [&](dbus::MessageExtractor &ex) -> bool {
                                    return ex.readString(uri);
                                });
                            });
                        }

                        return true;
                    });
                    if (!ret) {
                        return true;
                    }
                }
            });
            if (!ret) {
                continue;
            }

            if (uri.rfind("file://", 0) == 0) {
                uri = uri.substr(7);
            }

            path = uri;

            return true;
        }
    }

    bool launchFile(void *windowHandle, const std::string &path) {
        utils::Scoped<DBusConnection *> conn{
                dbus_bus_get_private(DBUS_BUS_SESSION, nullptr),
                dbus_connection_close,
        };

        utils::Scoped<DBusMessage *> request{
                dbus_message_new_method_call(
                        "org.freedesktop.portal.Desktop",
                        "/org/freedesktop/portal/desktop",
                        "org.freedesktop.portal.OpenURI",
                        "OpenFile"
                ),
                dbus_message_unref,
        };
        if (request == nullptr) {
            return false;
        }

        utils::Scoped<int> fdFile{open(path.data(), O_RDWR), close};
        if (fdFile < 0) {
            return false;
        }

        char parentWindow[64] = {0};
        std::sprintf(parentWindow, "x11:%lx", reinterpret_cast<long>(windowHandle));

        dbus::MessageBuilder args{request};
        args.writeString(parentWindow);
        args.writeFileDescriptor(fdFile);
        args.inner(DBUS_TYPE_ARRAY, "{sv}", [&](dbus::MessageBuilder &b) {});

        utils::Scoped<DBusMessage *> reply{
                dbus_connection_send_with_reply_and_block(
                        conn,
                        request,
                        DBUS_TIMEOUT_INFINITE,
                        nullptr
                ),
                dbus_message_unref,
        };

        return true;
    }
}