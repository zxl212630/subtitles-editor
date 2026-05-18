#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QColor>

class ThemeManager : public QObject {
    Q_OBJECT
public:
    static ThemeManager& instance();

    struct ThemeColors {
        QString bgBase;
        QString bgPanel;
        QString bgLighter;
        QString border;
        QString borderDark;
        QString textNormal;
        QString textMuted;
    };

    struct PrimaryColors {
        QString main;
        QString hover;
        QString disabled;
    };

    // Initialize mappings
    void init();

    // Apply the currently configured theme and primary color
    void applyTheme();

    // Get specific colors if needed by C++ paints
    QColor getPrimaryColor() const;
    QColor getBgBaseColor() const;
    QColor getBgPanelColor() const;
    QColor getBgLighterColor() const;
    QColor getBorderColor() const;
    QColor getBorderDarkColor() const;
    QColor getTextNormalColor() const;
    QColor getTextMutedColor() const;
    
    // Get lists for UI
    QStringList availableThemes() const;
    QStringList availablePrimaryColors() const;
    
    QString getThemeName(const QString& id) const;
    QString getPrimaryColorHex(const QString& id) const;

signals:
    void themeChanged();

private:
    ThemeManager();
    ~ThemeManager() = default;

    QString loadQssTemplate(const QString& themeId) const;
    QString processQss(QString qss, const ThemeColors& t, const PrimaryColors& p) const;

    QMap<QString, ThemeColors> themes_;
    QMap<QString, PrimaryColors> primaries_;
    QMap<QString, QString> themeNames_;
};
