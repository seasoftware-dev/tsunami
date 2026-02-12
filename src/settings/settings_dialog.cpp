#include "settings_dialog.h"
#include "settings/settings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QTabWidget>
#include <QSlider>
#include <QSpinBox>
#include <QButtonGroup>
#include <QRadioButton>

namespace Tsunami {

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Settings - Tsunami");
    setMinimumSize(900, 550);
    setMaximumSize(900, 550);
    resize(900, 550);
    setModal(true);
    
    bool isDark = Settings::instance().getDarkMode();
    
    QString bgColor = isDark ? "#0f172a" : "#ffffff";
    QString textColor = isDark ? "#e2e8f0" : "#1e293b";
    QString inputBg = isDark ? "#1e293b" : "#ffffff";
    QString borderColor = isDark ? "#334155" : "#cbd5e1";
    QString accentColor = "#3b82f6";
    
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(16, 16, 16, 16);
    main_layout->setSpacing(12);
    
    // Title
    QLabel* title = new QLabel("Settings");
    title->setStyleSheet(QString("font-size: 20px; font-weight: bold; color: %1;").arg(accentColor));
    main_layout->addWidget(title);
    
    // Theme
    QHBoxLayout* theme_row = new QHBoxLayout();
    theme_row->addWidget(new QLabel("Theme:"));
    theme_combo_ = new QComboBox();
    theme_combo_->addItem("Dark", "dark");
    theme_combo_->addItem("Light", "light");
    theme_combo_->addItem("System", "system");
    theme_row->addWidget(theme_combo_);
    theme_row->addStretch();
    main_layout->addLayout(theme_row);
    
    // Privacy Checkboxes
    QLabel* privacy_label = new QLabel("Privacy:");
    privacy_label->setStyleSheet(QString("font-weight: bold; color: %1;").arg(accentColor));
    main_layout->addWidget(privacy_label);
    
    block_trackers_ = new QCheckBox("Block Trackers");
    block_trackers_->setChecked(true);
    main_layout->addWidget(block_trackers_);
    
    block_ads_ = new QCheckBox("Block Ads");
    block_ads_->setChecked(true);
    main_layout->addWidget(block_ads_);
    
    https_only_ = new QCheckBox("HTTPS Only");
    main_layout->addWidget(https_only_);
    
    do_not_track_ = new QCheckBox("Do Not Track");
    do_not_track_->setChecked(true);
    main_layout->addWidget(do_not_track_);
    
    block_third_party_cookies_ = new QCheckBox("Block Third-Party Cookies");
    block_third_party_cookies_->setChecked(true);
    main_layout->addWidget(block_third_party_cookies_);
    
    block_fingerprinting_ = new QCheckBox("Block Fingerprinting");
    block_fingerprinting_->setChecked(true);
    main_layout->addWidget(block_fingerprinting_);
    
    disable_webrtc_ = new QCheckBox("Disable WebRTC");
    main_layout->addWidget(disable_webrtc_);
    
    // Search Engine
    QLabel* search_label = new QLabel("Search Engine:");
    search_label->setStyleSheet(QString("font-weight: bold; color: %1;").arg(accentColor));
    main_layout->addWidget(search_label);
    
    search_group_ = new QButtonGroup(this);
    
    QRadioButton* duckduckgo_radio = new QRadioButton("DuckDuckGo");
    search_group_->addButton(duckduckgo_radio, 0);
    main_layout->addWidget(duckduckgo_radio);
    
    QRadioButton* brave_radio = new QRadioButton("Brave Search");
    search_group_->addButton(brave_radio, 1);
    main_layout->addWidget(brave_radio);
    
    QRadioButton* google_radio = new QRadioButton("Google");
    search_group_->addButton(google_radio, 2);
    main_layout->addWidget(google_radio);
    
    // Startup
    QLabel* startup_label = new QLabel("Startup:");
    startup_label->setStyleSheet(QString("font-weight: bold; color: %1;").arg(accentColor));
    main_layout->addWidget(startup_label);
    
    QHBoxLayout* homepage_row = new QHBoxLayout();
    homepage_row->addWidget(new QLabel("Homepage:"));
    homepage_edit_ = new QLineEdit();
    homepage_edit_->setPlaceholderText("about:blank");
    homepage_row->addWidget(homepage_edit_);
    main_layout->addLayout(homepage_row);
    
    restore_tabs_ = new QCheckBox("Restore tabs from last session");
    restore_tabs_->setChecked(true);
    main_layout->addWidget(restore_tabs_);
    
    auto_reload_ = new QCheckBox("Auto-reload pages");
    main_layout->addWidget(auto_reload_);
    
    QHBoxLayout* reload_row = new QHBoxLayout();
    reload_row->addWidget(new QLabel("Interval:"));
    auto_reload_interval_ = new QSpinBox();
    auto_reload_interval_->setRange(5, 3600);
    auto_reload_interval_->setValue(30);
    auto_reload_interval_->setEnabled(false);
    reload_row->addWidget(auto_reload_interval_);
    reload_row->addStretch();
    main_layout->addLayout(reload_row);
    
    connect(auto_reload_, &QCheckBox::toggled, auto_reload_interval_, &QSpinBox::setEnabled);
    
    // Advanced
    QLabel* advanced_label = new QLabel("Advanced:");
    advanced_label->setStyleSheet(QString("font-weight: bold; color: %1;").arg(accentColor));
    main_layout->addWidget(advanced_label);
    
    QHBoxLayout* zoom_row = new QHBoxLayout();
    zoom_row->addWidget(new QLabel("Zoom:"));
    zoom_level_ = new QSlider(Qt::Horizontal);
    zoom_level_->setRange(25, 200);
    zoom_level_->setValue(100);
    zoom_row->addWidget(zoom_level_);
    zoom_label_ = new QLabel("100%");
    zoom_row->addWidget(zoom_label_);
    zoom_row->addStretch();
    main_layout->addLayout(zoom_row);
    
    connect(zoom_level_, &QSlider::valueChanged, this, [this](int value) {
        zoom_label_->setText(QString::number(value) + "%");
    });
    
    show_bookmarks_bar_ = new QCheckBox("Show bookmarks bar");
    main_layout->addWidget(show_bookmarks_bar_);
    
    auto_clear_cache_ = new QCheckBox("Auto-clear cache on exit");
    main_layout->addWidget(auto_clear_cache_);
    
    main_layout->addStretch();
    
    // Buttons
    QHBoxLayout* button_row = new QHBoxLayout();
    button_row->setContentsMargins(0, 8, 0, 0);
    
    QPushButton* reset_btn = new QPushButton("Reset");
    connect(reset_btn, &QPushButton::clicked, this, &SettingsDialog::resetSettings);
    button_row->addWidget(reset_btn);
    
    button_row->addStretch();
    
    QPushButton* cancel_btn = new QPushButton("Cancel");
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    button_row->addWidget(cancel_btn);
    
    QPushButton* save_btn = new QPushButton("Save");
    save_btn->setStyleSheet(QString("background: %1; color: white; padding: 8px 24px; border: none; border-radius: 6px;").arg(accentColor));
    connect(save_btn, &QPushButton::clicked, this, &SettingsDialog::saveSettings);
    connect(save_btn, &QPushButton::clicked, this, &QDialog::accept);
    button_row->addWidget(save_btn);
    
    main_layout->addLayout(button_row);
    
    // Apply theme stylesheet
    setStyleSheet(QString(R"(
        QDialog {
            background-color: %1;
        }
        QLabel {
            color: %2;
        }
        QComboBox {
            padding: 6px 12px;
            border: 1px solid %3;
            border-radius: 6px;
            background: %4;
            color: %2;
        }
        QCheckBox {
            spacing: 8px;
            color: %2;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border-radius: 4px;
            border: 1px solid %3;
            background: %4;
        }
        QCheckBox::indicator:checked {
            background: %5;
            border-color: %5;
        }
        QRadioButton {
            spacing: 8px;
            color: %2;
        }
        QRadioButton::indicator {
            width: 16px;
            height: 16px;
            border-radius: 8px;
            border: 1px solid %3;
            background: %4;
        }
        QRadioButton::indicator:checked {
            background: %5;
            border-color: %5;
        }
        QLineEdit {
            padding: 6px 12px;
            border: 1px solid %3;
            border-radius: 6px;
            background: %4;
            color: %2;
        }
        QSlider::groove:horizontal {
            height: 6px;
            background: %3;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: %5;
            width: 16px;
            margin: -5px 0;
            border-radius: 4px;
        }
        QSpinBox {
            padding: 6px 12px;
            border: 1px solid %3;
            border-radius: 6px;
            background: %4;
            color: %2;
        }
        QPushButton {
            padding: 8px 20px;
            border: 1px solid %3;
            border-radius: 6px;
            background: %4;
            color: %2;
        }
        QPushButton:hover {
            background: %3;
        }
    )").arg(bgColor, textColor, borderColor, inputBg, accentColor));
    
    loadSettings();
}

void SettingsDialog::loadSettings() {
    auto& settings = Settings::instance();
    
    QString theme = settings.getTheme();
    int theme_index = theme_combo_->findData(theme);
    if (theme_index >= 0) theme_combo_->setCurrentIndex(theme_index);
    
    block_trackers_->setChecked(settings.getBlockTrackers());
    block_ads_->setChecked(settings.getBlockAds());
    https_only_->setChecked(settings.getHttpsOnly());
    do_not_track_->setChecked(settings.getDoNotTrack());
    block_third_party_cookies_->setChecked(settings.getBlockThirdPartyCookies());
    block_fingerprinting_->setChecked(settings.getBlockFingerprinting());
    disable_webrtc_->setChecked(settings.getDisableWebRTC());
    
    QString searchEngine = settings.getSearchEngine();
    int search_id = 0;
    if (searchEngine == "brave") search_id = 1;
    else if (searchEngine == "google") search_id = 2;
    QAbstractButton* search_btn = search_group_->button(search_id);
    if (search_btn) search_btn->setChecked(true);
    
    homepage_edit_->setText(settings.getHomepage());
    restore_tabs_->setChecked(settings.getRestoreTabs());
    auto_reload_->setChecked(settings.getAutoReload());
    auto_reload_interval_->setValue(settings.getAutoReloadInterval());
    
    zoom_level_->setValue(settings.getZoomLevel());
    zoom_label_->setText(QString::number(settings.getZoomLevel()) + "%");
    show_bookmarks_bar_->setChecked(settings.getShowBookmarksBar());
    auto_clear_cache_->setChecked(settings.getAutoClearCache());
}

void SettingsDialog::saveSettings() {
    auto& settings = Settings::instance();
    
    QString theme = theme_combo_->currentData().toString();
    settings.setTheme(theme);
    settings.setDarkMode(theme == "dark");
    
    settings.setBlockTrackers(block_trackers_->isChecked());
    settings.setBlockAds(block_ads_->isChecked());
    settings.setHttpsOnly(https_only_->isChecked());
    settings.setDoNotTrack(do_not_track_->isChecked());
    settings.setBlockThirdPartyCookies(block_third_party_cookies_->isChecked());
    settings.setBlockFingerprinting(block_fingerprinting_->isChecked());
    settings.setDisableWebRTC(disable_webrtc_->isChecked());
    
    int search_id = search_group_->checkedId();
    QString searchEngine = "duckduckgo";
    if (search_id == 1) searchEngine = "brave";
    else if (search_id == 2) searchEngine = "google";
    settings.setSearchEngine(searchEngine);
    
    settings.setHomepage(homepage_edit_->text());
    settings.setRestoreTabs(restore_tabs_->isChecked());
    settings.setAutoReload(auto_reload_->isChecked());
    settings.setAutoReloadInterval(auto_reload_interval_->value());
    
    settings.setZoomLevel(zoom_level_->value());
    settings.setShowBookmarksBar(show_bookmarks_bar_->isChecked());
    settings.setAutoClearCache(auto_clear_cache_->isChecked());
    
    settings.save();
}

void SettingsDialog::resetSettings() {
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Reset Settings", 
        "Reset all settings to defaults?",
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        Settings::instance().reset();
        loadSettings();
    }
}

void SettingsDialog::showDialog(QWidget* parent) {
    SettingsDialog dialog(parent);
    dialog.exec();
}

} // namespace Tsunami
