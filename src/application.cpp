/*
 * Tsunami Browser - Qt6 WebEngine Browser
 * application.cpp - Qt6 Application implementation
 */

#include "application.h"
#include "browser_window.h"
#include "settings/settings.h"
#include <QDir>
#include <QStandardPaths>
#include <QIcon>
#include <QFile>
#include <QFileInfo>
#include <iostream>
#include <filesystem>

namespace Tsunami {

// Find a resource file by checking multiple locations
static QString find_resource(const QString& relative_path) {
    QString exe_dir = QCoreApplication::applicationDirPath();
    
    QStringList search_paths = {
        #ifdef PROJECT_SOURCE_DIR
        QString(PROJECT_SOURCE_DIR) + "/data/" + relative_path,
        #endif
        exe_dir + "/../data/" + relative_path,
        exe_dir + "/data/" + relative_path,
        "/usr/share/tsunami/data/" + relative_path,
        "data/" + relative_path,
        "../data/" + relative_path
    };
    
    for (const QString& path : search_paths) {
        if (QFile::exists(path)) {
            return QFileInfo(path).absoluteFilePath();
        }
    }
    return "";
}

int Application::run(int argc, char* argv[]) {
    QApplication app(argc, argv);
    
    app.setApplicationName("Tsunami");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Tsunami");
    app.setOrganizationDomain("tsunami.dev");
    
    // Set application icon for all platforms
    QStringList iconPaths;
    iconPaths << QCoreApplication::applicationDirPath() + "/data/logo.svg"
              << QCoreApplication::applicationDirPath() + "/logo.svg"
              << QStringLiteral(":/logo.svg")
              << QStringLiteral("/usr/share/pixmaps/tsunami.svg");
    
    for (const QString& iconPath : iconPaths) {
        if (QFile::exists(iconPath)) {
            QIcon appIcon(iconPath);
            if (!appIcon.isNull()) {
                app.setWindowIcon(appIcon);
                break;
            }
        }
    }
    
    // Check for first run and show onboarding
    auto& settings = Settings::instance();
    if (settings.isFirstRun()) {
        // Show onboarding/setup page
        BrowserWindow onboardingWindow;
        onboardingWindow.show();
        onboardingWindow.loadUrl(QUrl::fromLocalFile(find_resource("pages/newtab.html")));
        app.processEvents();
        return app.exec();
    }
    
    // Create browser window
    BrowserWindow window;
    window.show();
    
    int result = app.exec();
    return result;
}

QString Application::get_config_dir() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return path;
}

QString Application::get_data_dir() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return path;
}

QString Application::get_cache_dir() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return path;
}

QString Application::get_resource_path(const QString& relative_path) {
    return find_resource(relative_path);
}

} // namespace Tsunami
