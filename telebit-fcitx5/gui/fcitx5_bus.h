// The org.fcitx.Fcitx.Controller1 calls the setup page is built on.
//
// This is what makes the setup page able to *do* something rather than just
// point at fcitx5-configtool: fcitx5 exposes its input-method group list and
// every addon's configuration over the session bus, which is the same
// interface fcitx5-configtool itself drives. Telebit's own addon accepts a
// partial configuration (TelebitFcitx5Engine::setConfig loads with
// partial=true), so writing a single option back is safe and leaves the
// macro and per-application lists untouched.
//
// Every call is synchronous with a short timeout and no auto-start. These are
// local calls to a running process, and the alternative — async plumbing for
// what is effectively a property read — would cost more than it buys. The
// timeout is what keeps a dead or wedged fcitx5 from freezing the window.

#pragma once

#include <glib.h>

#include <map>
#include <string>
#include <vector>

namespace telebit::setup::bus {

// True when the fcitx5 controller answered at all.
bool running();

// Scalar addon options, as fcitx5 hands them over: leaf values are the strings
// "True"/"False". Only booleans are exposed here — the key lists and the macro
// and per-app tables belong in fcitx5-configtool, which configure_addon()
// opens on exactly the right page.
std::map<std::string, bool> read_bool_options();

// Writes one option. Returns false when the call failed, in which case the UI
// must put the switch back where it was rather than lie about the state.
bool write_bool_option(const std::string &key, bool value);

// Whether telebit-fcitx5 is in the current input-method group — the difference
// between "installed" and "usable", and the single most common reason someone
// installs Telebit and still cannot type Vietnamese.
bool input_method_enabled();

// Appends telebit-fcitx5 to the current group and makes it current.
bool enable_input_method(std::string *error_out);

// Opens the addon's own configuration page in fcitx5-configtool, launched by
// fcitx5 itself.
bool configure_addon();

bool restart();

}  // namespace telebit::setup::bus
