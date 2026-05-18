#pragma once

#include <QWidget>
#include <QStringList>
#include <QMap>

class ThemeSelectorWidget : public QWidget {
    Q_OBJECT
public:
    explicit ThemeSelectorWidget(QWidget *parent = nullptr);

    void addTheme(const QString &id, const QString &bgBase, const QString &bgPanel, const QString &primary);
    void setCurrentTheme(const QString &id);
    QString currentTheme() const;

signals:
    void themeSelected(const QString &id);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    struct ThemeItem {
        QString id;
        QString bgBase;
        QString bgPanel;
        QString primary;
    };
    QList<ThemeItem> items_;
    QString currentId_;
    int hoverIndex_ = -1;
};

class ColorSelectorWidget : public QWidget {
    Q_OBJECT
public:
    explicit ColorSelectorWidget(QWidget *parent = nullptr);

    void addColor(const QString &id, const QString &hex);
    void setCurrentColor(const QString &id);
    QString currentColor() const;

signals:
    void colorSelected(const QString &id);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    struct ColorItem {
        QString id;
        QString hex;
    };
    QList<ColorItem> items_;
    QString currentId_;
    int hoverIndex_ = -1;
};
