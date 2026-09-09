#include "fcitx5_bus.h"

#include <gio/gio.h>

namespace telebit::setup::bus {
namespace {

constexpr const char *kService = "org.fcitx.Fcitx5";
constexpr const char *kPath = "/controller";
constexpr const char *kInterface = "org.fcitx.Fcitx.Controller1";
constexpr const char *kAddon = "telebit-fcitx5";
constexpr const char *kConfigUri = "fcitx://config/addon/telebit-fcitx5";

// Short enough that a wedged fcitx5 is a greyed-out page rather than a frozen
// window, long enough that a busy session still answers.
constexpr int kTimeoutMs = 2000;

// DO_NOT_AUTO_START matters: without it, asking whether fcitx5 is running
// would start it, and the answer would always be yes.
GDBusProxy *proxy() {
    static GDBusProxy *cached = nullptr;
    if (cached != nullptr) return cached;

    GError *error = nullptr;
    cached = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SESSION,
        static_cast<GDBusProxyFlags>(G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START |
                                     G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                     G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS),
        nullptr, kService, kPath, kInterface, nullptr, &error);
    if (cached == nullptr) {
        g_warning("fcitx5 controller unavailable: %s",
                  error != nullptr ? error->message : "unknown error");
        if (error != nullptr) g_error_free(error);
    }
    return cached;
}

// Returns nullptr on failure and, when `error_out` is given, the reason.
GVariant *call(const char *method, GVariant *args, std::string *error_out) {
    GDBusProxy *p = proxy();
    if (p == nullptr) {
        if (args != nullptr) g_variant_unref(g_variant_ref_sink(args));
        if (error_out != nullptr) *error_out = "Không kết nối được tới fcitx5.";
        return nullptr;
    }

    GError *error = nullptr;
    GVariant *reply = g_dbus_proxy_call_sync(p, method, args, G_DBUS_CALL_FLAGS_NONE, kTimeoutMs,
                                             nullptr, &error);
    if (reply == nullptr) {
        if (error_out != nullptr) {
            *error_out = error != nullptr && error->message != nullptr ? error->message
                                                                      : "Lệnh DBus thất bại.";
        }
        if (error != nullptr) g_error_free(error);
    }
    return reply;
}

std::string current_group() {
    GVariant *reply = call("CurrentInputMethodGroup", nullptr, nullptr);
    if (reply == nullptr) return {};
    const char *name = nullptr;
    g_variant_get(reply, "(&s)", &name);
    std::string out = name != nullptr ? name : "";
    g_variant_unref(reply);
    return out;
}

}  // namespace

bool running() {
    GVariant *reply = call("CurrentInputMethodGroup", nullptr, nullptr);
    if (reply == nullptr) return false;
    g_variant_unref(reply);
    return true;
}

std::map<std::string, bool> read_bool_options() {
    std::map<std::string, bool> options;

    GVariant *reply = call("GetConfig", g_variant_new("(s)", kConfigUri), nullptr);
    if (reply == nullptr) return options;

    // (v, a(sa(sssva{sv}))) — the description half is what fcitx5-configtool
    // builds its widgets from; the values are all we need.
    GVariant *boxed = nullptr;
    g_variant_get_child(reply, 0, "v", &boxed);
    if (boxed != nullptr) {
        GVariantIter iter;
        const char *key = nullptr;
        GVariant *value = nullptr;
        g_variant_iter_init(&iter, boxed);
        while (g_variant_iter_next(&iter, "{&sv}", &key, &value)) {
            // Leaves are strings; anything else is a list or a sub-table and is
            // not this page's business.
            if (key != nullptr && g_variant_is_of_type(value, G_VARIANT_TYPE_STRING) != 0) {
                const char *text = g_variant_get_string(value, nullptr);
                if (g_strcmp0(text, "True") == 0) options[key] = true;
                else if (g_strcmp0(text, "False") == 0) options[key] = false;
            }
            g_variant_unref(value);
        }
        g_variant_unref(boxed);
    }
    g_variant_unref(reply);
    return options;
}

bool write_bool_option(const std::string &key, bool value) {
    // A one-key dictionary: the addon merges it into the configuration it
    // already has, so the macro and per-application lists survive.
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&builder, "{sv}", key.c_str(),
                          g_variant_new_string(value ? "True" : "False"));

    GVariant *args = g_variant_new("(sv)", kConfigUri, g_variant_builder_end(&builder));
    GVariant *reply = call("SetConfig", args, nullptr);
    if (reply == nullptr) return false;
    g_variant_unref(reply);
    return true;
}

bool input_method_enabled() {
    const std::string group = current_group();
    if (group.empty()) return false;

    GVariant *reply = call("InputMethodGroupInfo", g_variant_new("(s)", group.c_str()), nullptr);
    if (reply == nullptr) return false;

    bool found = false;
    GVariant *entries = nullptr;
    g_variant_get(reply, "(&s@a(ss))", nullptr, &entries);
    if (entries != nullptr) {
        GVariantIter iter;
        const char *name = nullptr;
        const char *layout = nullptr;
        g_variant_iter_init(&iter, entries);
        while (g_variant_iter_next(&iter, "(&s&s)", &name, &layout)) {
            if (g_strcmp0(name, kAddon) == 0) {
                found = true;
                break;
            }
        }
        g_variant_unref(entries);
    }
    g_variant_unref(reply);
    return found;
}

bool enable_input_method(std::string *error_out) {
    const std::string group = current_group();
    if (group.empty()) {
        if (error_out != nullptr) *error_out = "fcitx5 chưa chạy, hoặc không có nhóm nào.";
        return false;
    }

    GVariant *reply = call("InputMethodGroupInfo", g_variant_new("(s)", group.c_str()), error_out);
    if (reply == nullptr) return false;

    const char *default_layout = nullptr;
    GVariant *entries = nullptr;
    g_variant_get(reply, "(&s@a(ss))", &default_layout, &entries);

    // Rewrite the group with Telebit appended, keeping the existing entries and
    // their layouts: the keyboard layout entry has to stay, or the group loses
    // its way back to plain Latin input.
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(ss)"));
    bool already_there = false;
    if (entries != nullptr) {
        GVariantIter iter;
        const char *name = nullptr;
        const char *layout = nullptr;
        g_variant_iter_init(&iter, entries);
        while (g_variant_iter_next(&iter, "(&s&s)", &name, &layout)) {
            g_variant_builder_add(&builder, "(ss)", name != nullptr ? name : "",
                                  layout != nullptr ? layout : "");
            if (g_strcmp0(name, kAddon) == 0) already_there = true;
        }
    }
    if (!already_there) g_variant_builder_add(&builder, "(ss)", kAddon, "");

    GVariant *args = g_variant_new("(ssa(ss))", group.c_str(),
                                   default_layout != nullptr ? default_layout : "",
                                   &builder);
    if (entries != nullptr) g_variant_unref(entries);
    g_variant_unref(reply);

    GVariant *set_reply = call("SetInputMethodGroupInfo", args, error_out);
    if (set_reply == nullptr) return false;
    g_variant_unref(set_reply);

    // Make it the active one too, so the switch has a visible effect instead of
    // only appearing in a list.
    GVariant *current = call("SetCurrentIM", g_variant_new("(s)", kAddon), nullptr);
    if (current != nullptr) g_variant_unref(current);
    return true;
}

bool configure_addon() {
    GVariant *reply = call("ConfigureAddon", g_variant_new("(s)", kAddon), nullptr);
    if (reply == nullptr) return false;
    g_variant_unref(reply);
    return true;
}

bool restart() {
    GVariant *reply = call("Restart", nullptr, nullptr);
    if (reply == nullptr) return false;
    g_variant_unref(reply);
    return true;
}

}  // namespace telebit::setup::bus
