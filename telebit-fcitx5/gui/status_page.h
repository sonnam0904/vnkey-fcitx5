// "Trạng thái" — does the input method actually reach applications?
//
// This is `telebit doctor` with widgets instead of a terminal: it calls the
// same collect() and renders the same Output, so the two front ends cannot
// disagree about what is wrong with a machine.

#pragma once

#include <gtk/gtk.h>

namespace telebit::setup {

struct StatusPage;

// `app` is held (g_application_hold) for the duration of each probe, so
// closing the window mid-run does not tear the process down under a worker
// thread that is still inside popen().
StatusPage *status_page_new(GtkApplication *app);

GtkWidget *status_page_widget(StatusPage *page);

// Starts a probe on a worker thread. Safe to call while one is running — the
// controls are disabled for as long as it is.
void status_page_refresh(StatusPage *page);

// Called from the window's destroy handler: a probe cannot be cancelled once
// it is inside popen(), so the delivery callback needs to be told that there
// is nothing left to draw into.
void status_page_closed(StatusPage *page);

}  // namespace telebit::setup
