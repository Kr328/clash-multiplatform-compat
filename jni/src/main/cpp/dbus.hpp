#pragma once

#include <functional>
#include <string>

#include <dbus/dbus.h>

namespace dbus {
    class MessageBuilder {
    private:
        DBusMessageIter iterator{};

    private:
        MessageBuilder() = default;

    public:
        explicit MessageBuilder(DBusMessage *msg) {
            dbus_message_iter_init_append(msg, &iterator);
        }

    private:
        void write(int type, DBusBasicValue value) {
            dbus_message_iter_append_basic(&iterator, type, &value);
        }

    public:
        void writeString(const std::string &str) {
            DBusBasicValue value{
                    .str = const_cast<char *>(str.data()),
            };

            write(DBUS_TYPE_STRING, value);
        }

        void writeUInt32(uint32_t u32) {
            DBusBasicValue value{
                    .u32 = u32,
            };

            write(DBUS_TYPE_UINT32, value);
        }

        void writeFileDescriptor(int fd) {
            DBusBasicValue value{
                    .fd = fd,
            };

            write(DBUS_TYPE_UNIX_FD, value);
        }

    public:
        void inner(int type, const char *signature, const std::function<void(MessageBuilder &)> &block) {
            MessageBuilder builder{};

            dbus_message_iter_open_container(&iterator, type, signature, &builder.iterator);

            block(builder);

            dbus_message_iter_close_container(&iterator, &builder.iterator);
        }
    };

    class MessageExtractor {
    private:
        DBusMessageIter iterator{};

    private:
        MessageExtractor() = default;

    public:
        explicit MessageExtractor(DBusMessage *msg) {
            dbus_message_iter_init(msg, &iterator);
        }

    private:
        bool readNext(int type, DBusBasicValue &value) {
            if (type != dbus_message_iter_get_arg_type(&iterator)) {
                return false;
            }

            dbus_message_iter_get_basic(&iterator, &value);

            dbus_message_iter_next(&iterator);

            return true;
        }

    public:
        bool readString(std::string &out) {
            DBusBasicValue value;

            if (readNext(DBUS_TYPE_STRING, value)) {
                out = value.str;

                return true;
            }

            return false;
        }

        bool readObjectPath(std::string &out) {
            DBusBasicValue value;

            if (readNext(DBUS_TYPE_OBJECT_PATH, value)) {
                out = value.str;

                return true;
            }

            return false;
        }

        bool readUInt32(uint32_t &out) {
            DBusBasicValue value;

            if (readNext(DBUS_TYPE_UINT32, value)) {
                out = value.u32;

                return true;
            }

            return false;
        }

    public:
        bool inner(int type, const std::function<bool(MessageExtractor &)> &block) {
            if (dbus_message_iter_get_arg_type(&iterator) != type) {
                return false;
            }

            MessageExtractor extractor{};

            dbus_message_iter_recurse(&iterator, &extractor.iterator);

            if (block(extractor)) {
                dbus_message_iter_next(&iterator);

                return true;
            }

            return false;
        }
    };
}
