/*
 * Sea Browser - Privacy-focused web browser
 * browser_window.cpp - Main browser window implementation (GTK3)
 */

#include "browser_window.h"
#include "web_view.h"
#include "tab_manager.h"
#include "history/history_manager.h"
#include "settings/settings_manager.h"
#include "settings/settings_dialog.h"
#include <filesystem>
#include <regex>
#include <iostream>
#include <algorithm>
#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

namespace SeaBrowser {

static TabManager* global_tab_manager = nullptr;

static const char* FIREFOX_USER_AGENT = 
    "Mozilla/5.0 (X11; Linux x86_64; rv:133.0) Gecko/20100101 Firefox/133.0";
static const char* CHROME_USER_AGENT = 
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36";
static const char* SAFARI_USER_AGENT = 
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 14_1) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.1 Safari/605.1.15";

#ifdef GDK_WINDOWING_X11
static void set_motif_hints_no_decorations(GtkWidget* window) {
    if (!window || !gtk_widget_get_realized(window)) return;
    GdkWindow* gdk_window = gtk_widget_get_window(window);
    if (!gdk_window || !GDK_IS_X11_WINDOW(gdk_window)) return;
    
    Display* display = GDK_DISPLAY_XDISPLAY(gdk_window_get_display(gdk_window));
    Window xwindow = GDK_WINDOW_XID(gdk_window);
    
    struct {
        unsigned long flags;
        unsigned long functions;
        unsigned long decorations;
        long input_mode;
        unsigned long status;
    } hints = {2, 0, 0, 0, 0};
    
    Atom motif_hints = XInternAtom(display, "_MOTIF_WM_HINTS", False);
    XChangeProperty(display, xwindow, motif_hints, motif_hints, 32,
                    PropModeReplace, (unsigned char*)&hints, 5);
    XFlush(display);
}
#endif

void configure_web_view(WebKitWebView* view) {
    auto settings = webkit_web_view_get_settings(view);
    std::string ua_setting = SettingsManager::instance().general().user_agent;
    if (ua_setting == "firefox") webkit_settings_set_user_agent(settings, FIREFOX_USER_AGENT);
    else if (ua_setting == "safari") webkit_settings_set_user_agent(settings, SAFARI_USER_AGENT);
    else webkit_settings_set_user_agent(settings, CHROME_USER_AGENT);
    
    webkit_settings_set_enable_javascript(settings, TRUE);
    webkit_settings_set_enable_developer_extras(settings, TRUE);
    webkit_settings_set_hardware_acceleration_policy(settings, WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS);
    
    auto context = webkit_web_view_get_context(view);
    auto cookie_manager = webkit_web_context_get_cookie_manager(context);
    auto policy = SettingsManager::instance().privacy().cookie_policy;
    if (policy == CookiePolicy::BlockAll)
        webkit_cookie_manager_set_accept_policy(cookie_manager, WEBKIT_COOKIE_POLICY_ACCEPT_NEVER);
    else if (policy == CookiePolicy::AcceptAll)
        webkit_cookie_manager_set_accept_policy(cookie_manager, WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS);
    else
        webkit_cookie_manager_set_accept_policy(cookie_manager, WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY);
}

GtkWidget* BrowserWindow::create(GtkApplication* app) {
    auto window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Sea Browser");
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 800);
    
    // We set decorated(TRUE) because gtk_window_set_titlebar handles the replacement
    gtk_window_set_decorated(GTK_WINDOW(window), TRUE);
    
    auto vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    auto url_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(url_entry), "Search or enter URL...");
    
    // Setup header bar via GtkHeaderBar
    auto header = setup_header_bar(window, nullptr, url_entry);
    gtk_window_set_titlebar(GTK_WINDOW(window), header);
    
    auto notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
    gtk_widget_set_vexpand(notebook, TRUE);
    
    global_tab_manager = new TabManager(GTK_NOTEBOOK(notebook), GTK_ENTRY(url_entry), GTK_WINDOW(window));
    g_signal_connect(url_entry, "activate", G_CALLBACK(on_url_activate), global_tab_manager);
    
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);
    
    const char* first_run_env = g_getenv("SEA_BROWSER_FIRST_RUN");
    if (first_run_env && strcmp(first_run_env, "1") == 0) {
        global_tab_manager->create_tab("sea://setup");
    } else {
        global_tab_manager->create_tab(SettingsManager::instance().general().homepage);
    }
    
#ifdef GDK_WINDOWING_X11
    auto apply_hints = +[](GtkWidget* win, GdkEvent*, gpointer) -> gboolean {
        set_motif_hints_no_decorations(win);
        return FALSE;
    };
    g_signal_connect(window, "realize", G_CALLBACK(+[](GtkWidget* win, gpointer) {
        set_motif_hints_no_decorations(win);
    }), nullptr);
    g_signal_connect(window, "map-event", G_CALLBACK(apply_hints), nullptr);
#endif

    HistoryManager::instance().cleanup_history();
    return window;
}

GtkWidget* BrowserWindow::setup_header_bar(GtkWidget* window, GtkWidget*, GtkWidget* url_entry) {
    auto header = gtk_header_bar_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(header), "blue-titlebar");
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), FALSE); 
    
    auto nav_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_style_context_add_class(gtk_widget_get_style_context(nav_box), "linked");
    
    auto back_btn = gtk_button_new_from_icon_name("go-previous-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_clicked), nullptr);
    gtk_box_pack_start(GTK_BOX(nav_box), back_btn, FALSE, FALSE, 0);
    
    auto forward_btn = gtk_button_new_from_icon_name("go-next-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    g_signal_connect(forward_btn, "clicked", G_CALLBACK(on_forward_clicked), nullptr);
    gtk_box_pack_start(GTK_BOX(nav_box), forward_btn, FALSE, FALSE, 0);
    
    auto reload_btn = gtk_button_new_from_icon_name("view-refresh-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    g_signal_connect(reload_btn, "clicked", G_CALLBACK(on_reload_clicked), nullptr);
    gtk_box_pack_start(GTK_BOX(nav_box), reload_btn, FALSE, FALSE, 0);
    
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), nav_box);
    
    gtk_widget_set_hexpand(url_entry, TRUE);
    gtk_header_bar_set_custom_title(GTK_HEADER_BAR(header), url_entry);
    
    auto right_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    
    auto new_tab_btn = gtk_button_new_from_icon_name("tab-new-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    g_signal_connect(new_tab_btn, "clicked", G_CALLBACK(on_new_tab_clicked), nullptr);
    gtk_box_pack_start(GTK_BOX(right_box), new_tab_btn, FALSE, FALSE, 0);
    
    auto home_btn = gtk_button_new_from_icon_name("go-home-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    g_signal_connect(home_btn, "clicked", G_CALLBACK(on_home_clicked), nullptr);
    gtk_box_pack_start(GTK_BOX(right_box), home_btn, FALSE, FALSE, 0);
    
    auto m_btn = gtk_menu_button_new();
    gtk_button_set_image(GTK_BUTTON(m_btn), gtk_image_new_from_icon_name("open-menu-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR));
    
    auto menu = gtk_menu_new();
    auto s_item = gtk_menu_item_new_with_label("Settings");
    g_signal_connect(s_item, "activate", G_CALLBACK(on_settings_clicked), nullptr);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), s_item);
    
    auto h_item = gtk_menu_item_new_with_label("History");
    g_signal_connect(h_item, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer) {
        if (global_tab_manager) global_tab_manager->create_tab("sea://history");
    }), nullptr);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), h_item);
    
    gtk_widget_show_all(menu);
    gtk_menu_button_set_popup(GTK_MENU_BUTTON(m_btn), menu);
    gtk_box_pack_start(GTK_BOX(right_box), m_btn, FALSE, FALSE, 0);
    
    auto win_controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(win_controls), "window-controls");

    auto min_btn = gtk_button_new_from_icon_name("window-minimize-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_style_context_add_class(gtk_widget_get_style_context(min_btn), "win-control-btn");
    g_signal_connect(min_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer win) {
        gtk_window_iconify(GTK_WINDOW(win));
    }), window);
    gtk_box_pack_start(GTK_BOX(win_controls), min_btn, FALSE, FALSE, 0);

    auto max_btn = gtk_button_new_from_icon_name("window-maximize-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_style_context_add_class(gtk_widget_get_style_context(max_btn), "win-control-btn");
    g_signal_connect(max_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer win) {
        if (gtk_window_is_maximized(GTK_WINDOW(win))) gtk_window_unmaximize(GTK_WINDOW(win));
        else gtk_window_maximize(GTK_WINDOW(win));
    }), window);
    gtk_box_pack_start(GTK_BOX(win_controls), max_btn, FALSE, FALSE, 0);

    auto close_btn = gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_style_context_add_class(gtk_widget_get_style_context(close_btn), "win-control-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(close_btn), "close-btn");
    g_signal_connect(close_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer win) {
        gtk_window_close(GTK_WINDOW(win));
    }), window);
    gtk_box_pack_start(GTK_BOX(win_controls), close_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(right_box), win_controls, FALSE, FALSE, 0);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), right_box);
    
    g_signal_connect(window, "button-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventButton* event, gpointer) -> gboolean {
        if (event->button == 8) { on_back_clicked(nullptr, nullptr); return TRUE; }
        else if (event->button == 9) { on_forward_clicked(nullptr, nullptr); return TRUE; }
        return FALSE;
    }), nullptr);

    gtk_widget_show_all(header);
    return header;
}

void BrowserWindow::on_url_activate(GtkEntry* entry, gpointer user_data) {
    auto tab_manager = static_cast<TabManager*>(user_data);
    auto web_view = tab_manager->get_current_web_view();
    if (web_view) webkit_web_view_load_uri(web_view, process_url_input(gtk_entry_get_text(entry)).c_str());
}

void BrowserWindow::on_back_clicked(GtkButton*, gpointer) {
    if (global_tab_manager) {
        auto v = global_tab_manager->get_current_web_view();
        if (v && webkit_web_view_can_go_back(v)) webkit_web_view_go_back(v);
    }
}

void BrowserWindow::on_forward_clicked(GtkButton*, gpointer) {
    if (global_tab_manager) {
        auto v = global_tab_manager->get_current_web_view();
        if (v && webkit_web_view_can_go_forward(v)) webkit_web_view_go_forward(v);
    }
}

void BrowserWindow::on_reload_clicked(GtkButton*, gpointer) {
    if (global_tab_manager) {
        auto v = global_tab_manager->get_current_web_view();
        if (v) webkit_web_view_reload(v);
    }
}

void BrowserWindow::on_home_clicked(GtkButton*, gpointer) {
    if (global_tab_manager) {
        auto v = global_tab_manager->get_current_web_view();
        if (v) webkit_web_view_load_uri(v, SettingsManager::instance().general().homepage.c_str());
    }
}

void BrowserWindow::on_new_tab_clicked(GtkButton*, gpointer) {
    if (global_tab_manager) global_tab_manager->create_tab("sea://newtab");
}

void BrowserWindow::on_settings_clicked(GtkMenuItem*, gpointer) {
    if (global_tab_manager) global_tab_manager->create_tab("sea://settings");
}

std::string BrowserWindow::process_url_input(const std::string& input) {
    auto trimmed = input;
    while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t')) trimmed.erase(0, 1);
    while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t')) trimmed.pop_back();
    if (trimmed.empty()) return "sea://newtab";
    if (trimmed.starts_with("sea://") || trimmed.starts_with("http://") || trimmed.starts_with("https://")) return trimmed;
    if (is_url(trimmed)) return "https://" + trimmed;
    return get_search_url(trimmed);
}

bool BrowserWindow::is_url(const std::string& input) {
    if (input.find(' ') != std::string::npos) return false;
    static const std::vector<std::string> tlds = { ".com", ".org", ".net", ".io", ".co", ".dev", ".app", ".gov", ".edu" };
    for (const auto& tld : tlds) if (input.size() > tld.size() && input.substr(input.size() - tld.size()) == tld) return true;
    return false;
}

std::string BrowserWindow::get_search_url(const std::string& query) {
    std::string engine = SettingsManager::instance().search().default_engine;
    std::string prefix = "https://www.google.com/search?q=";
    if (engine == "duckduckgo") prefix = "https://duckduckgo.com/?q=";
    else if (engine == "bing") prefix = "https://www.bing.com/search?q=";
    else if (engine == "brave") prefix = "https://search.brave.com/search?q=";
    std::string encoded;
    for (char c : query) {
        if (isalnum(c)) encoded += c;
        else if (c == ' ') encoded += '+';
        else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", static_cast<unsigned char>(c));
            encoded += buf;
        }
    }
    return prefix + encoded;
}

} // namespace SeaBrowser
