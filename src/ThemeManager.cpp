#include "ThemeManager.h"
#include "ConfigManager.h"
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QPalette>
#include <QTextStream>

ThemeManager &ThemeManager::instance() {
  static ThemeManager instance;
  return instance;
}

ThemeManager::ThemeManager() { init(); }

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

  // OLED (Deep Black)
  themeNames_["oled"] = tr("纯黑 OLED (Pure Black)");
  themes_["oled"] = {
      "#000000", // bgBase
      "#09090b", // bgPanel
      "#18181b", // bgLighter
      "#27272a", // border
      "#3f3f46", // borderDark
      "#f4f4f5", // textNormal (Zinc 100)
      "#71717a"  // textMuted (Zinc 500)
  };

  // Midnight (Deep Navy)
  themeNames_["midnight"] = tr("深邃午夜蓝 (Midnight)");
  themes_["midnight"] = {
      "#020617", // bgBase (Slate 950 / Navy Dark)
      "#0f172a", // bgPanel (Slate 900)
      "#1e293b", // bgLighter (Slate 800)
      "#334155", // border (Slate 700)
      "#475569", // borderDark (Slate 600)
      "#e2e8f0", // textNormal (Slate 200)
      "#94a3b8"  // textMuted (Slate 400)
  };

  // TODO: Add Sepia later

  // Primary Colors Definitions
  primaries_["purple"] = {"#a855f7", "#c084fc", "#7e22ce"};
  primaries_["indigo"] = {"#6366f1", "#818cf8", "#4338ca"};
  primaries_["blue"] = {"#3b82f6", "#60a5fa", "#1d4ed8"}; // Default
  primaries_["cyan"] = {"#06b6d4", "#22d3ee", "#0e7490"};
  primaries_["teal"] = {"#14b8a6", "#2dd4bf", "#0f766e"};
  primaries_["green"] = {"#10b981", "#34d399", "#047857"};
  primaries_["orange"] = {"#f97316", "#fb923c", "#c2410c"};
  primaries_["pink"] = {"#ec4899", "#f472b6", "#be185d"};
  primaries_["red"] = {"#ef4444", "#f87171", "#b91c1c"};
  primaries_["sepia"] = {"#d4ba8a", "#e8cba6", "#b09b72"};
}

void ThemeManager::applyTheme() {
  QString themeId = ConfigManager::instance().theme();
  if (themeId == "light") {
    themeId = "dark";
    ConfigManager::instance().setValue("", "theme", "dark");
  }
  QString primaryId = ConfigManager::instance().primaryColor();

  qDebug() << "[ThemeManager] Applying Theme:" << themeId
           << "Primary:" << primaryId;

  if (!themes_.contains(themeId)) {
    qDebug() << "[ThemeManager] Theme ID" << themeId
             << "not found, falling back to dark";
    themeId = "dark";
  }
  if (!primaries_.contains(primaryId)) {
    qDebug() << "[ThemeManager] Primary ID" << primaryId
             << "not found, falling back to blue";
    primaryId = "blue";
  }

  const auto &t = themes_[themeId];
  const auto &p = primaries_[primaryId];
  qDebug() << "[ThemeManager] Using Primary Hex:" << p.main;

  // 1. Update Global QPalette
  QPalette pal;
  QColor mainBg(t.bgBase);
  QColor panelBg(t.bgPanel);
  QColor text(t.textNormal);
  QColor muted(t.textMuted);
  QColor primary(p.main);

  pal.setColor(QPalette::Window, mainBg);
  pal.setColor(QPalette::WindowText, text);
  pal.setColor(QPalette::Base, panelBg);
  pal.setColor(QPalette::AlternateBase, mainBg);
  pal.setColor(QPalette::Text, text);
  pal.setColor(QPalette::PlaceholderText, muted);
  pal.setColor(QPalette::Button, t.bgLighter);
  pal.setColor(QPalette::ButtonText, text);
  pal.setColor(QPalette::Highlight, primary);
  pal.setColor(QPalette::HighlightedText, Qt::white);
  pal.setColor(QPalette::Link, primary);

  qApp->setPalette(pal);

  // 2. Process and apply QSS
  QString rawQss = loadQssTemplate(themeId);
  QString processedQss = processQss(rawQss, t, p);

  qApp->setStyleSheet(processedQss);
  emit themeChanged();
}

QString ThemeManager::loadQssTemplate(const QString &themeId) const {
  // Actually we only need one template now, or maybe two if the structures are
  // vastly different. Let's load dark.qss as the base template for now, or
  // light.qss depending on themeId.
  QFile file(QString(":/themes/%1.qss").arg(themeId));
  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    qWarning() << "Could not open theme file:" << file.fileName();
    return "";
  }
  return QTextStream(&file).readAll();
}

QString ThemeManager::processQss(QString qss, const ThemeColors &t,
                                 const PrimaryColors &p) const {
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
  if (!primaries_.contains(id))
    id = "blue";
  return QColor(primaries_[id].main);
}

QColor ThemeManager::getBgBaseColor() const {
  QString id = ConfigManager::instance().theme();
  if (!themes_.contains(id))
    id = "dark";
  return QColor(themes_[id].bgBase);
}

QColor ThemeManager::getBgPanelColor() const {
  QString id = ConfigManager::instance().theme();
  if (!themes_.contains(id))
    id = "dark";
  return QColor(themes_[id].bgPanel);
}

QColor ThemeManager::getBgLighterColor() const {
  QString id = ConfigManager::instance().theme();
  if (!themes_.contains(id))
    id = "dark";
  return QColor(themes_[id].bgLighter);
}

QColor ThemeManager::getBorderColor() const {
  QString id = ConfigManager::instance().theme();
  if (!themes_.contains(id))
    id = "dark";
  return QColor(themes_[id].border);
}

QColor ThemeManager::getBorderDarkColor() const {
  QString id = ConfigManager::instance().theme();
  if (!themes_.contains(id))
    id = "dark";
  return QColor(themes_[id].borderDark);
}

QColor ThemeManager::getTextNormalColor() const {
  QString id = ConfigManager::instance().theme();
  if (!themes_.contains(id))
    id = "dark";
  return QColor(themes_[id].textNormal);
}

QColor ThemeManager::getTextMutedColor() const {
  QString id = ConfigManager::instance().theme();
  if (!themes_.contains(id))
    id = "dark";
  return QColor(themes_[id].textMuted);
}

QStringList ThemeManager::availableThemes() const {
  return {"dark", "oled", "midnight"}; // Order matters for UI
}

QStringList ThemeManager::availablePrimaryColors() const {
  return {"purple", "indigo", "blue", "cyan", "teal",
          "green",  "orange", "pink", "red",  "sepia"};
}

QString ThemeManager::getThemeName(const QString &id) const {
  return themeNames_.value(id, id);
}

QString ThemeManager::getPrimaryColorHex(const QString &id) const {
  if (primaries_.contains(id))
    return primaries_[id].main;
  return "#3b82f6";
}
