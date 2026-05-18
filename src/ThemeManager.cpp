#include "ThemeManager.h"
#include "ConfigManager.h"
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>

ThemeManager& ThemeManager::instance() {
    static ThemeManager instance;
    return instance;
}

ThemeManager::ThemeManager() {
    init();
}

void ThemeManager::init() {
    // Theme Definitions (Backgrounds)
    themeNames_["dark"] = tr("深色 (Dark)");
    themes_["dark"] = {
        "#151515", // bgBase
        "#1e1e1e", // bgPanel
        "#262626", // bgLighter
        "#3f3f46", // border
        "#333333", // borderDark
        "#ffffff", // textNormal
        "#9ca3af"  // textMuted
    };

    themeNames_["light"] = tr("浅色 (Light)");
    themes_["light"] = {
        "#f3f4f6", // bgBase
        "#ffffff", // bgPanel
        "#e5e7eb", // bgLighter
        "#d1d5db", // border
        "#9ca3af", // borderDark
        "#111827", // textNormal
        "#4b5563"  // textMuted
    };
    
    // TODO: Add OLED and Sepia later

    // Primary Colors Definitions
    primaries_["purple"]  = {"#a855f7", "#c084fc", "#7e22ce"};
    primaries_["indigo"]  = {"#6366f1", "#818cf8", "#4338ca"};
    primaries_["blue"]    = {"#3b82f6", "#60a5fa", "#1d4ed8"}; // Default
    primaries_["cyan"]    = {"#06b6d4", "#22d3ee", "#0e7490"};
    primaries_["teal"]    = {"#14b8a6", "#2dd4bf", "#0f766e"};
    primaries_["green"]   = {"#10b981", "#34d399", "#047857"};
    primaries_["orange"]  = {"#f97316", "#fb923c", "#c2410c"};
    primaries_["pink"]    = {"#ec4899", "#f472b6", "#be185d"};
    primaries_["red"]     = {"#ef4444", "#f87171", "#b91c1c"};
    primaries_["sepia"]   = {"#d4ba8a", "#e8cba6", "#b09b72"};
}

void ThemeManager::applyTheme() {
    QString themeId = ConfigManager::instance().theme();
    QString primaryId = ConfigManager::instance().primaryColor();

    if (!themes_.contains(themeId)) themeId = "dark";
    if (!primaries_.contains(primaryId)) primaryId = "blue";

    const auto& t = themes_[themeId];
    const auto& p = primaries_[primaryId];

    QString rawQss = loadQssTemplate(themeId);
    QString processedQss = processQss(rawQss, t, p);

    qApp->setStyleSheet(processedQss);
    emit themeChanged();
}

QString ThemeManager::loadQssTemplate(const QString& themeId) const {
    // Actually we only need one template now, or maybe two if the structures are vastly different.
    // Let's load dark.qss as the base template for now, or light.qss depending on themeId.
    QFile file(QString(":/themes/%1.qss").arg(themeId));
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        qWarning() << "Could not open theme file:" << file.fileName();
        return "";
    }
    return QTextStream(&file).readAll();
}

QString ThemeManager::processQss(QString qss, const ThemeColors& t, const PrimaryColors& p) const {
    qss.replace("@primary_color", p.main);
    qss.replace("@primary_hover", p.hover);
    qss.replace("@primary_disabled", p.disabled);

    qss.replace("@bg_base", t.bgBase);
    qss.replace("@bg_panel", t.bgPanel);
    qss.replace("@bg_lighter", t.bgLighter);
    
    qss.replace("@border_color", t.border);
    qss.replace("@border_dark", t.borderDark);
    
    qss.replace("@text_normal", t.textNormal);
    qss.replace("@text_muted", t.textMuted);

    return qss;
}

QColor ThemeManager::getPrimaryColor() const {
    QString id = ConfigManager::instance().primaryColor();
    if (!primaries_.contains(id)) id = "blue";
    return QColor(primaries_[id].main);
}

QColor ThemeManager::getBgBaseColor() const {
    QString id = ConfigManager::instance().theme();
    if (!themes_.contains(id)) id = "dark";
    return QColor(themes_[id].bgBase);
}

QStringList ThemeManager::availableThemes() const {
    return {"dark", "light"}; // Order matters for UI
}

QStringList ThemeManager::availablePrimaryColors() const {
    return {"purple", "indigo", "blue", "cyan", "teal", "green", "orange", "pink", "red", "sepia"};
}

QString ThemeManager::getThemeName(const QString& id) const {
    return themeNames_.value(id, id);
}

QString ThemeManager::getPrimaryColorHex(const QString& id) const {
    if (primaries_.contains(id)) return primaries_[id].main;
    return "#3b82f6";
}
