#include "theme.h"

#include "cleaner.h"

#include <dbus/dbus.h>
#include <pthread.h>

CLEANER_NULLABLE(DBusConnection*, dbus_connection_close)
CLEANER_NULLABLE(DBusMessage*, dbus_message_unref)

static int isSupported = 0;

static int queryColorScheme() {
    DBusConnection *conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
    if (conn == NULL) {
        return -1;
    }

    CLEANABLE(dbus_message_unref)
    DBusMessage *message = dbus_message_new_method_call(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.Settings",
            "Read"
    );
    if (message == NULL) {
        return -1;
    }

    const char *appearance = "org.freedesktop.appearance";
    const char *colorScheme = "color-scheme";

    DBusMessageIter args;
    dbus_message_iter_init_append(message, &args);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &appearance);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &colorScheme);

    CLEANABLE(dbus_message_unref)
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, message, DBUS_TIMEOUT_INFINITE, NULL);
    if (reply == NULL || !dbus_message_iter_init(reply, &args) || dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_VARIANT) {
        return -1;
    }

    DBusMessageIter outer;
    dbus_message_iter_recurse(&args, &outer);
    if (dbus_message_iter_get_arg_type(&outer) != DBUS_TYPE_VARIANT) {
        return -1;
    }

    DBusMessageIter inner;
    dbus_message_iter_recurse(&outer, &inner);
    if (dbus_message_iter_get_arg_type(&inner) != DBUS_TYPE_UINT32) {
        return -1;
    }

    dbus_uint32_t value = 0;
    dbus_message_iter_get_basic(&inner, &value);

    return (int) value;
}

static void colorSchemeObserver(OnThemeChangedListener listener) {
    CLEANABLE(dbus_connection_close)
    DBusConnection *conn = dbus_bus_get_private(DBUS_BUS_SESSION, NULL);
    if (conn == NULL) {
        return;
    }

    dbus_bus_add_match(conn, "type='signal',sender='org.freedesktop.portal.Desktop',interface='org.freedesktop.portal.Settings',path='/org/freedesktop/portal/desktop',member='SettingChanged',arg0='org.freedesktop.appearance'", NULL);
    dbus_connection_flush(conn);

    while (dbus_connection_get_is_connected(conn)) {
        dbus_connection_read_write(conn, DBUS_TIMEOUT_INFINITE);

        CLEANABLE(dbus_message_unref)
        DBusMessage *message = dbus_connection_pop_message(conn);
        if (!message) {
            continue;
        }

        if (dbus_message_is_signal(message, "org.freedesktop.portal.Settings", "SettingChanged")) {
            listener();
        }
    }
}

int themeInit(OnThemeChangedListener listener) {
    if (queryColorScheme() < 0) {
        return 0;
    }

    pthread_t _unused;
    if (pthread_create(&_unused, NULL, (void *(*)(void *)) &colorSchemeObserver, listener)) {
        return 1;
    }

    isSupported = 1;

    return 0;
}

int themeIsSupported() {
    return isSupported;
}

int themeIsNightMode() {
    int ret = queryColorScheme();
    if (ret < 0) {
        return 0;
    }

    return ret != 0;
}