/*
 * Sea Browser - Privacy-focused web browser
 * application.cpp - GTK3 Application implementation
 */

#include "application.h"
#include "browser_window.h"
#include "web_view.h"
#include "settings/settings_manager.h"
#include "history/history_manager.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <vector>
#include <limits.h>
#include <unistd.h>

namespace SeaBrowser {

// Get the directory where the executable is located
static std::string get_exe_dir() {
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        std::string path(buf);
        size_t pos = path.rfind('/');
        if (pos != std::string::npos) {
            return path.substr(0, pos);
        }
    }
    return ".";
}

// Find a resource file by checking multiple locations
static std::string find_resource(const std::string& relative_path) {
    std::string exe_dir = get_exe_dir();
    
    std::vector<std::string> search_paths = {
#ifdef PROJECT_SOURCE_DIR
        std::string(PROJECT_SOURCE_DIR) + "/data/" + relative_path,
#endif
        exe_dir + "/../data/" + relative_path,
        exe_dir + "/data/" + relative_path,
        "/usr/share/seabrowser/data/" + relative_path,
        "data/" + relative_path,
        "../data/" + relative_path
    };
    
    for (const auto& path : search_paths) {
        if (std::filesystem::exists(path)) {
            return std::filesystem::absolute(path).string();
        }
    }
    return "";
}

int Application::run(int argc, char* argv[]) {
    // Parse our custom arguments before GTK sees them
    std::vector<char*> filtered_argv;
    for (int i = 0; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.starts_with("--theme=")) {
            std::string theme = arg.substr(8);
            std::cout << "[SeaBrowser] Theme override: " << theme << std::endl;
            if (theme == "kde") g_setenv("SEA_THEME", "kde", TRUE);
            else if (theme == "adwaita") g_setenv("SEA_THEME", "adwaita", TRUE);
        } else {
            filtered_argv.push_back(argv[i]);
        }
    }
    int filtered_argc = static_cast<int>(filtered_argv.size());

    // Force client-side decorations (custom titlebar) on all platforms
    // This MUST be set before any GTK calls
    g_setenv("GTK_CSD", "1", TRUE);

    auto app = gtk_application_new("io.seabrowser.SeaBrowser", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);
    g_signal_connect(app, "startup", G_CALLBACK(on_startup), nullptr);
    int status = g_application_run(G_APPLICATION(app), filtered_argc, filtered_argv.data());
    g_object_unref(app);
    return status;
}

void Application::on_startup(GApplication*, gpointer) {
    // Detect display server
    const char* session_type = g_getenv("XDG_SESSION_TYPE");
    if (session_type && strcmp(session_type, "wayland") == 0) {
        if (!g_getenv("GDK_BACKEND")) {
            g_setenv("GDK_BACKEND", "wayland", TRUE);
        }
        g_setenv("WEBKIT_DISABLE_DMABUF_RENDERER", "1", TRUE);
    } else {
        if (!g_getenv("GDK_BACKEND")) {
            g_setenv("GDK_BACKEND", "x11", TRUE);
        }
    }

    // Load custom CSS theme
    const char* fallback_css = 
        ".sea-header { background: #2563eb; background-image: linear-gradient(to bottom, #2563eb, #1e40af); border-bottom: 2px solid #1d4ed8; padding: 4px; min-height: 42px; color: white; }"
        ".win-control-btn { min-width: 32px; min-height: 32px; border-radius: 0; padding: 4px; opacity: 0.8; }"
        ".win-control-btn:hover { background: rgba(255,255,255,0.1); opacity: 1; }"
        ".win-control-btn.close-btn:hover { background: #ef4444; }"
        "entry { background: rgba(15,23,42,0.4); border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; color: white; padding: 4px; }";

    std::string css_path = find_resource("style.css");
    auto provider = gtk_css_provider_new();
    GError* error = nullptr;

    if (!css_path.empty()) {
        std::cout << "[SeaBrowser] CSS path found: " << css_path << std::endl;
        gtk_css_provider_load_from_path(provider, css_path.c_str(), &error);
    } else {
        std::cerr << "[SeaBrowser] WARNING: style.css not found, using fallback" << std::endl;
        gtk_css_provider_load_from_data(provider, fallback_css, -1, &error);
    }

    if (error) {
        std::cerr << "[SeaBrowser] ERROR: CSS error: " << error->message << " (using fallback)" << std::endl;
        g_error_free(error);
        gtk_css_provider_load_from_data(provider, fallback_css, -1, nullptr);
    }

    auto display = gdk_display_get_default();
    auto screen = gdk_display_get_default_screen(display);
    gtk_style_context_add_provider_for_screen(
        screen,
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER // Use high priority to override system themes
    );
    g_object_set_data_full(G_OBJECT(display), "sea-css-provider", provider, g_object_unref);
    std::cout << "[SeaBrowser] CSS theme initialized" << std::endl;
    
    // Initialize settings
    SettingsManager::instance().load(get_config_dir());
    
    // Initialize history
    HistoryManager::instance().init(get_data_dir() + "/history.db");

    // Register custom URI scheme for internal pages
    auto context = webkit_web_context_get_default();
    webkit_web_context_register_uri_scheme(context, "sea", WebView::sea_scheme_request_handler, nullptr, nullptr);
    webkit_security_manager_register_uri_scheme_as_secure(
        webkit_web_context_get_security_manager(context), "sea");
    
    std::cout << "[SeaBrowser] Startup complete" << std::endl;
}

void Application::on_activate(GtkApplication* app, gpointer) {
    bool first_run = !SettingsManager::instance().general().setup_completed;
    g_setenv("SEA_BROWSER_FIRST_RUN", first_run ? "1" : "0", TRUE);
    
    auto window = BrowserWindow::create(app);
    
    // Fallback: Apply CSS provider directly to window if it failed at screen level
    auto display = gdk_display_get_default();
    auto provider = (GtkCssProvider*)g_object_get_data(G_OBJECT(display), "sea-css-provider");
    if (provider) {
        gtk_style_context_add_provider(
            gtk_widget_get_style_context(GTK_WIDGET(window)),
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
    }
    
    if (first_run) {
        SettingsManager::instance().general().setup_completed = true;
        SettingsManager::instance().save();
    }
    
    gtk_widget_show_all(GTK_WIDGET(window));
    gtk_window_present(GTK_WINDOW(window));
}

std::string Application::get_config_dir() {
    auto path = std::filesystem::path(g_get_user_config_dir()) / "seabrowser";
    std::filesystem::create_directories(path);
    return path.string();
}

std::string Application::get_data_dir() {
    auto path = std::filesystem::path(g_get_user_data_dir()) / "seabrowser";
    std::filesystem::create_directories(path);
    return path.string();
}

std::string Application::get_cache_dir() {
    auto path = std::filesystem::path(g_get_user_cache_dir()) / "seabrowser";
    std::filesystem::create_directories(path);
    return path.string();
}

std::string Application::get_resource_path(const std::string& relative_path) {
    return find_resource(relative_path);
}

} // namespace SeaBrowser
