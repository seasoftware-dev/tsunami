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
#include <QCoreApplication>

namespace Tsunami {

// Bridge object to expose settings to JavaScript
class SettingsBridge : public QObject {
    Q_OBJECT
public:
    explicit SettingsBridge(QObject* parent = nullptr) : QObject(parent) {
        connect(&Settings::instance(), &Settings::settingsChanged, this, &SettingsBridge::onSettingsChanged);
    }

    Q_INVOKABLE QJsonObject getSettings() {
        auto& settings = Settings::instance();
        QJsonObject obj;
        obj["darkMode"] = settings.getDarkMode();
        obj["accentColor"] = settings.getAccentColor();
        obj["searchEngine"] = settings.getSearchEngine();
        obj["theme"] = settings.getTheme();
        return obj;
    }
    
    Q_INVOKABLE void setSetting(const QString& key, const QVariant& value) {
        auto& settings = Settings::instance();
        if (key == "darkMode") settings.setDarkMode(value.toBool());
        else if (key == "accentColor") settings.setAccentColor(value.toString());
        else if (key == "searchEngine") settings.setSearchEngine(value.toString());
        else if (key == "theme") settings.setTheme(value.toString());
        else if (key == "firstRun") settings.setFirstRun(!value.toBool());
    }

signals:
    void settingsChanged(QJsonObject settings);

private slots:
    void onSettingsChanged() {
        emit settingsChanged(getSettings());
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
    
    // Create the bridge object properly parenting it to the page to avoid leaks/crashes
    // But QWebChannel needs a QObject that outlives the page load or is registered properly.
    // Ideally, we register it on the profile if shared, or page if unique.
    // For simplicity, we'll create a new channel per page.
    
    QWebChannel* channel = new QWebChannel(page);
    SettingsBridge* bridge = new SettingsBridge(channel); // Parent to channel
    channel->registerObject("tsunami", bridge); // Use 'tsunami' to match JS
    page->setWebChannel(channel);
    
    // Inject qwebchannel.js
    QFile webChannelFile(QCoreApplication::applicationDirPath() + "/qtwebchannel/qwebchannel.js");
    // Backup location
    if (!webChannelFile.exists()) {
        webChannelFile.setFileName(":/qtwebchannel/qwebchannel.js"); 
    }
    
    QString webChannelJs;
    if (webChannelFile.open(QIODevice::ReadOnly)) {
        webChannelJs = QString::fromUtf8(webChannelFile.readAll());
        webChannelFile.close();
    } else {
        // Fallback: Embed a minimal version or assume it's loaded from resource
        // Qt 6 usually provides this via qrc if the module is included.
        webChannelJs = "/* qwebchannel.js not found, assuming external load */"; 
    }

    QWebEngineScript script;
    
    // Get current settings as JSON
    Tsunami::Settings& appSettings = Tsunami::Settings::instance();
    QJsonObject settingsObj;
    settingsObj["darkMode"] = appSettings.getDarkMode();
    settingsObj["accentColor"] = appSettings.getAccentColor();
    settingsObj["searchEngine"] = appSettings.getSearchEngine();
    settingsObj["theme"] = appSettings.getTheme();
    QJsonDocument settingsDoc(settingsObj);
    QString settingsJson = QString::fromUtf8(settingsDoc.toJson(QJsonDocument::Compact));
    
    script.setSourceCode(webChannelJs + QString(R"(
        (function() {
            // Inject settings directly
            window.tsunamiSettings = %1;
            console.log('Injected settings:', window.tsunamiSettings);
            
            new QWebChannel(qt.webChannelTransport, function(channel) {
                window.tsunami = channel.objects.tsunami;
                console.log('Tsunami bridge connected');
                
                // Notify that bridge is ready
                if (window.onTsunamiReady) window.onTsunamiReady();
                
                // Initial setting application if the function exists
                if (window.tsunami && window.applySettings) {
                    console.log('Applying settings from bridge');
                    window.applySettings(window.tsunami.getSettings());
                }
                
                // Listen for settings changes using callback approach
                if (window.tsunami && window.tsunami.settingsChanged && window.applySettings) {
                    window.tsunami.settingsChanged.connect(function(settings) {
                        console.log('Settings changed:', JSON.stringify(settings));
                        window.applySettings(settings);
                    });
                }
            });
        })();
    )").arg(settingsJson));
    script.setName("tsunami_bridge");
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setInjectionPoint(QWebEngineScript::DocumentReady);
    script.setRunsOnSubFrames(false);
    
    page->scripts().insert(script);
}
} // namespace Tsunami

#include "web_view.moc"
