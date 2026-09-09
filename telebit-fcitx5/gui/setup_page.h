// "Cài đặt" — turn Telebit on and change how it types, here rather than in
// fcitx5-configtool.
//
// The page only exposes the handful of options a person changes when they
// start using Telebit: which layout (Telex or VNI) and the four typing
// behaviours. Macros, the per-application list and the key bindings stay in
// fcitx5-configtool, which the page can open on Telebit's own configuration
// page through fcitx5 itself.

#pragma once

#include <gtk/gtk.h>

namespace telebit::setup {

struct SetupPage;

SetupPage *setup_page_new();

GtkWidget *setup_page_widget(SetupPage *page);

// Re-reads everything from fcitx5: whether it is running, whether Telebit is
// in the current input-method group, and the current option values. Called on
// construction and whenever the page is shown again, because fcitx5-configtool
// may have changed things in the meantime.
void setup_page_reload(SetupPage *page);

}  // namespace telebit::setup
