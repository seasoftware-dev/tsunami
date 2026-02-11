#include "web_view.h"
#include "settings/settings.h"
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineSettings>
#include <QWebEnginePage>
#include <QWebEngineScriptCollection>
#include <QWebChannel>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

namespace Tsunami {

class SettingsBridge : public QObject {
    Q_OBJECT
public:
    Q_INVOKABLE QVariant getSetting(const QString& key) {
        auto& settings = Settings::instance();
        if (key == "darkMode") return settings.getDarkMode();
        if (key == "accentColor") return settings.getAccentColor();
        if (key == "searchEngine") return settings.getSearchEngine();
        if (key == "theme") return settings.getTheme();
        return QVariant();
    }
    
    Q_INVOKABLE void setSetting(const QString& key, const QVariant& value) {
        auto& settings = Settings::instance();
        if (key == "darkMode") settings.setDarkMode(value.toBool());
        else if (key == "accentColor") settings.setAccentColor(value.toString());
        else if (key == "searchEngine") settings.setSearchEngine(value.toString());
        else if (key == "theme") settings.setTheme(value.toString());
        else if (key == "firstRun") settings.setFirstRun(!value.toBool());
    }
    
    Q_INVOKABLE QVariant getAllSettings() {
        auto& settings = Settings::instance();
        QJsonObject obj;
        obj["darkMode"] = settings.getDarkMode();
        obj["accentColor"] = settings.getAccentColor();
        obj["searchEngine"] = settings.getSearchEngine();
        obj["theme"] = settings.getTheme();
        return QVariant(obj);
    }
};

void WebView::setupPage(QWebEnginePage* page) {
    if (!page) return;

    QWebEngineProfile* profile = page->profile();

    profile->setHttpUserAgent(
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36"
    );

    QWebEngineSettings* settings = profile->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    settings->setAttribute(QWebEngineSettings::WebGLEnabled, true);
    settings->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled, true);
    settings->setAttribute(QWebEngineSettings::AutoLoadImages, true);
    settings->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, true);

    profile->setHttpAcceptLanguage("en-US,en;q=0.9");
    
    // Add Qt bridge for JavaScript
    static SettingsBridge* bridge = new SettingsBridge();
    QWebChannel* channel = new QWebChannel(page);
    channel->registerObject("tsunami", bridge);
    page->setWebChannel(channel);
    
    // Inject JavaScript bridge
    QString jsBridge = R"(
        (function() {
            // Prefer the correctly spelled 'tsunami' namespace, but keep 'tunami' as a backwards-compatible alias.
            if (window.tsunami) {
                if (!window.tunami) {
                    window.tunami = window.tsunami;
                }
                return;
            }
            window.tsunami = {
                getSettings: function() {
                    if (window.tsunami._settings) return window.tsunami._settings;
                    var xhr = new XMLHttpRequest();
                    xhr.open('GET', 'qt://channel', false);
                    xhr.send();
                    try {
                        window.tsunami._settings = JSON.parse(xhr.responseText);
                    } catch(e) {
                        window.tsunami._settings = {};
                    }
                    return window.tsunami._settings;
                },
                getSetting: function(key) {
                    if (window.tsunami._settings && window.tsunami._settings.hasOwnProperty(key)) {
                        return window.tsunami._settings[key];
            if (!window.tunami) {
                window.tunami = window.tsunami;
            }
                    }
                    var xhr = new XMLHttpRequest();
                    xhr.open('GET', 'qt://getSetting/' + key, false);
                    xhr.send();
                    return xhr.responseText;
                },
                setSetting: function(key, value) {
                    var xhr = new XMLHttpRequest();
                    xhr.open('POST', 'qt://setSetting/' + key + '/' + encodeURIComponent(String(value)), false);
                    xhr.send();
                }
            };
        })();
    )";
    
    page->runJavaScript(jsBridge);
}

} // namespace Tsunami

#include "web_view.moc"
