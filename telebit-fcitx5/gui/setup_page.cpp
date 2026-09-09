#include "setup_page.h"

#include <map>
#include <string>
#include <vector>

#include "fcitx5_bus.h"
#include "widgets.h"

namespace telebit::setup {

// One switch row bound to one boolean option of the addon configuration.
struct OptionRow {
    std::string key;  // the fcitx5 configuration key, e.g. "SpellCheckRestore"
    GtkWidget *toggle = nullptr;
};

struct SetupPage {
    GtkWidget *root = nullptr;
    GtkWidget *hero = nullptr;
    GtkWidget *badge = nullptr;
    GtkWidget *hero_title = nullptr;
    GtkWidget *hero_detail = nullptr;
    GtkWidget *enable_button = nullptr;

    GtkWidget *telex_radio = nullptr;
    GtkWidget *vni_radio = nullptr;
    std::vector<OptionRow> options;

    // Set while the widgets are being filled in from fcitx5, so the handlers
    // that write back do not fire for values they just read.
    bool loading = false;
};

namespace {

constexpr const char *kVniKey = "VNIMode";

void set_hero(SetupPage *page, const char *state, const char *icon_name, const std::string &title,
              const std::string &detail) {
    for (const char *css_class : {"ok", "fail", "busy"}) {
        gtk_widget_remove_css_class(page->hero, css_class);
        gtk_widget_remove_css_class(page->badge, css_class);
    }
    gtk_widget_add_css_class(page->hero, state);
    gtk_widget_add_css_class(page->badge, state);
    gtk_image_set_from_icon_name(GTK_IMAGE(page->badge), icon_name);
    gtk_label_set_text(GTK_LABEL(page->hero_title), title.c_str());
    gtk_label_set_text(GTK_LABEL(page->hero_detail), detail.c_str());
}

// A row of: title, note underneath, and a control on the right.
GtkWidget *make_setting_row(const std::string &title, const std::string &note, GtkWidget *control) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);

    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_widget_set_hexpand(text, TRUE);
    gtk_box_append(GTK_BOX(text), make_label(title, "tb-label", true));
    if (!note.empty()) gtk_box_append(GTK_BOX(text), make_label(note, "tb-note", true));
    gtk_box_append(GTK_BOX(box), text);

    gtk_widget_set_valign(control, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box), control);
    return box;
}

void on_option_toggled(GtkSwitch *toggle, gboolean state, gpointer data) {
    auto *page = static_cast<SetupPage *>(data);
    if (page->loading) return;

    // Find which option this switch belongs to.
    for (const auto &row : page->options) {
        if (row.toggle != GTK_WIDGET(toggle)) continue;
        if (!bus::write_bool_option(row.key, state != 0)) {
            // Put it back rather than show a state fcitx5 does not have.
            page->loading = true;
            gtk_switch_set_active(toggle, state == 0);
            page->loading = false;
            set_hero(page, "fail", "dialog-error-symbolic", "Không lưu được cấu hình",
                     "fcitx5 không nhận lệnh. Thử khởi động lại fcitx5 rồi mở lại cửa sổ này.");
        }
        return;
    }
}

void on_layout_toggled(GtkCheckButton *button, gpointer data) {
    auto *page = static_cast<SetupPage *>(data);
    if (page->loading) return;
    // Only react to the button that became active, or every change would write
    // twice — once for the button turning off and once for the one turning on.
    if (gtk_check_button_get_active(button) == 0) return;

    const bool vni = GTK_WIDGET(button) == page->vni_radio;
    if (!bus::write_bool_option(kVniKey, vni)) {
        set_hero(page, "fail", "dialog-error-symbolic", "Không lưu được kiểu gõ",
                 "fcitx5 không nhận lệnh. Thử khởi động lại fcitx5 rồi mở lại cửa sổ này.");
    }
}

void on_enable_clicked(GtkButton *, gpointer data) {
    auto *page = static_cast<SetupPage *>(data);
    std::string error;
    if (bus::enable_input_method(&error)) {
        setup_page_reload(page);
    } else {
        set_hero(page, "fail", "dialog-error-symbolic", "Chưa bật được Telebit", error);
    }
}

void on_configure_clicked(GtkButton *, gpointer data) {
    auto *page = static_cast<SetupPage *>(data);
    if (bus::configure_addon()) return;

    // fcitx5 could not open it (usually because fcitx5-configtool is not
    // installed); try the binary directly before giving up, so a working
    // configtool with a wedged fcitx5 still opens.
    const char *argv[] = {"fcitx5-configtool", nullptr};
    if (g_spawn_async(nullptr, const_cast<char **>(argv), nullptr, G_SPAWN_SEARCH_PATH, nullptr,
                      nullptr, nullptr, nullptr) == 0) {
        set_hero(page, "fail", "dialog-error-symbolic", "Không mở được cấu hình fcitx5",
                 "Cài gói fcitx5-configtool để mở được trang cấu hình đầy đủ.");
    }
}

void on_restart_clicked(GtkButton *, gpointer data) {
    auto *page = static_cast<SetupPage *>(data);
    if (!bus::restart()) {
        set_hero(page, "fail", "dialog-error-symbolic", "Không khởi động lại được fcitx5",
                 "Thử chạy `fcitx5 -r` trong terminal.");
        return;
    }
    // fcitx5 drops the bus name while it restarts, so anything asked right now
    // fails; the button below is what the user presses when it is back.
    set_hero(page, "busy", icon_or("content-loading-symbolic", "system-search-symbolic"),
             "Đang khởi động lại fcitx5…", "Bấm “Đọc lại” sau một vài giây.");
}

void on_reload_clicked(GtkButton *, gpointer data) {
    setup_page_reload(static_cast<SetupPage *>(data));
}

GtkWidget *build_hero(SetupPage *page) {
    page->hero = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
    gtk_widget_add_css_class(page->hero, "tb-hero");

    page->badge = gtk_image_new_from_icon_name("emblem-ok-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(page->badge), 24);
    gtk_widget_add_css_class(page->badge, "tb-badge");
    gtk_widget_set_valign(page->badge, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(page->hero), page->badge);

    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(text, TRUE);
    gtk_widget_set_valign(text, GTK_ALIGN_CENTER);
    page->hero_title = make_label("", "tb-hero-title", false);
    page->hero_detail = make_label("", "tb-hero-detail", true);
    gtk_box_append(GTK_BOX(text), page->hero_title);
    gtk_box_append(GTK_BOX(text), page->hero_detail);
    gtk_box_append(GTK_BOX(page->hero), text);

    page->enable_button = gtk_button_new_with_label("Bật Telebit");
    gtk_widget_add_css_class(page->enable_button, "tb-pill");
    gtk_widget_add_css_class(page->enable_button, "suggested-action");
    gtk_widget_set_valign(page->enable_button, GTK_ALIGN_CENTER);
    g_signal_connect(page->enable_button, "clicked", G_CALLBACK(on_enable_clicked), page);
    gtk_box_append(GTK_BOX(page->hero), page->enable_button);

    return page->hero;
}

GtkWidget *build_layout_card(SetupPage *page) {
    GtkWidget *card = make_card();

    page->telex_radio = gtk_check_button_new();
    page->vni_radio = gtk_check_button_new();
    // A GtkCheckButton group is GTK4's radio group; the old GtkRadioButton is
    // gone.
    gtk_check_button_set_group(GTK_CHECK_BUTTON(page->vni_radio),
                               GTK_CHECK_BUTTON(page->telex_radio));
    g_signal_connect(page->telex_radio, "toggled", G_CALLBACK(on_layout_toggled), page);
    g_signal_connect(page->vni_radio, "toggled", G_CALLBACK(on_layout_toggled), page);

    card_append(card, make_setting_row("Telex", "aa = â · dd = đ · as = á · af = à",
                                       page->telex_radio));
    card_append(card, make_setting_row("VNI", "a6 = â · d9 = đ · a1 = á · a2 = à",
                                       page->vni_radio));
    return card;
}

// Title, note and configuration key for each switch, in the order they appear.
struct OptionSpec {
    const char *key;
    const char *title;
    const char *note;
};

const OptionSpec kOptionSpecs[] = {
    {"SpellCheckRestore", "Kiểm tra chính tả",
     "Từ không phải tiếng Việt được trả lại đúng phím đã gõ"},
    {"ModernToneStyle", "Đặt dấu kiểu mới", "hoà, khoẻ, thuý — thay vì hòa, khỏe, thúy"},
    {"AutoCapitalizeSentence", "Tự viết hoa đầu câu", "Sau dấu . ? ! rồi dấu cách hoặc Enter"},
    {"DirectCommitRollback", "Gõ trực tiếp, không gạch chân",
     "Nhanh và tự nhiên hơn, nhưng cần ứng dụng hỗ trợ surrounding text"},
    {"AIEnabled", "Trợ lý AI",
     "Ctrl+Shift+Space mở ô nhập yêu cầu. Cần cấu hình API key qua biến môi trường"},
};

GtkWidget *build_options_card(SetupPage *page) {
    GtkWidget *card = make_card();
    for (const auto &spec : kOptionSpecs) {
        GtkWidget *toggle = gtk_switch_new();
        g_signal_connect(toggle, "state-set", G_CALLBACK(on_option_toggled), page);
        page->options.push_back(OptionRow{spec.key, toggle});
        card_append(card, make_setting_row(spec.title, spec.note, toggle));
    }
    return card;
}

GtkWidget *build_advanced_card(SetupPage *page) {
    GtkWidget *card = make_card();

    GtkWidget *configure = gtk_button_new_with_label("Mở fcitx5-configtool");
    gtk_widget_add_css_class(configure, "tb-pill");
    g_signal_connect(configure, "clicked", G_CALLBACK(on_configure_clicked), page);
    card_append(card, make_setting_row("Gõ tắt, danh sách ứng dụng, phím tắt",
                                       "Những mục còn lại nằm trong trang cấu hình của fcitx5",
                                       configure));

    GtkWidget *restart = gtk_button_new_with_label("Khởi động lại");
    gtk_widget_add_css_class(restart, "tb-pill");
    g_signal_connect(restart, "clicked", G_CALLBACK(on_restart_clicked), page);
    card_append(card, make_setting_row("fcitx5",
                                       "Cần thiết sau khi đổi frontend hoặc cài thêm addon",
                                       restart));

    GtkWidget *reload = gtk_button_new_with_label("Đọc lại");
    gtk_widget_add_css_class(reload, "tb-pill");
    g_signal_connect(reload, "clicked", G_CALLBACK(on_reload_clicked), page);
    card_append(card, make_setting_row("Trạng thái cài đặt",
                                       "Đọc lại từ fcitx5 nếu bạn vừa đổi ở nơi khác", reload));

    return card;
}

}  // namespace

SetupPage *setup_page_new() {
    auto *page = new SetupPage();

    page->root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(page->root), build_hero(page));

    gtk_box_append(GTK_BOX(page->root), make_section_title("Kiểu gõ"));
    gtk_box_append(GTK_BOX(page->root), build_layout_card(page));

    gtk_box_append(GTK_BOX(page->root), make_section_title("Cách gõ"));
    gtk_box_append(GTK_BOX(page->root), build_options_card(page));

    gtk_box_append(GTK_BOX(page->root), make_section_title("Khác"));
    gtk_box_append(GTK_BOX(page->root), build_advanced_card(page));

    setup_page_reload(page);
    return page;
}

GtkWidget *setup_page_widget(SetupPage *page) { return page->root; }

void setup_page_reload(SetupPage *page) {
    const bool running = bus::running();
    const bool enabled = running && bus::input_method_enabled();

    if (!running) {
        set_hero(page, "fail", "dialog-error-symbolic", "fcitx5 chưa chạy",
                 "Khởi động fcitx5 (hoặc đăng xuất rồi đăng nhập lại) rồi bấm “Đọc lại”.");
    } else if (enabled) {
        set_hero(page, "ok", "emblem-ok-symbolic", "Telebit đang bật",
                 "Telebit đã nằm trong nhóm input method của fcitx5. Chuyển bộ gõ bằng "
                 "Ctrl+Space.");
    } else {
        set_hero(page, "fail", "dialog-warning-symbolic", "Telebit chưa được bật",
                 "Addon đã cài nhưng chưa nằm trong nhóm input method của fcitx5, nên chưa "
                 "gõ được tiếng Việt.");
    }
    gtk_widget_set_visible(page->enable_button, running && !enabled);

    // Nothing below the hero can be honoured without fcitx5, so it is disabled
    // rather than showing values that are not in effect anywhere.
    const std::map<std::string, bool> values = running ? bus::read_bool_options()
                                                       : std::map<std::string, bool>{};

    page->loading = true;
    const auto vni = values.find(kVniKey);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(page->vni_radio),
                                vni != values.end() && vni->second);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(page->telex_radio),
                                vni == values.end() || !vni->second);
    gtk_widget_set_sensitive(page->telex_radio, running);
    gtk_widget_set_sensitive(page->vni_radio, running);

    for (const auto &row : page->options) {
        const auto found = values.find(row.key);
        gtk_switch_set_active(GTK_SWITCH(row.toggle), found != values.end() && found->second);
        gtk_widget_set_sensitive(row.toggle, running);
    }
    page->loading = false;
}

}  // namespace telebit::setup
