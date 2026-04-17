#include "AppWindow.h"

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>

struct AppWindow::Private
{
    QFrame* titleBar = nullptr;
};

AppWindow::AppWindow(QWidget* parent)
    : QMainWindow(parent)
    , d(new Private)
{
    setupUi();
}

AppWindow::~AppWindow()
{
    delete d;
}

void AppWindow::setupUi()
{
    setWindowTitle("字幕编辑");
    resize(1440, 900);

    setupTitleBar();

    // Central widget placeholder
    auto* central = new QWidget(this);
    setCentralWidget(central);
}

void AppWindow::setupTitleBar()
{
    // Title bar placeholder - 后续与 QwindowKit 集成
    d->titleBar = new QFrame(this);
    d->titleBar->setFixedHeight(36);
    d->titleBar->setObjectName("TitleBar");

    auto* layout = new QHBoxLayout(d->titleBar);
    layout->setContentsMargins(12, 0, 12, 0);

    auto* title = new QLabel("字幕编辑", d->titleBar);
    title->setObjectName("TitleText");

    layout->addWidget(title);
    layout->addStretch();

    // Window controls placeholder (macOS style)
    auto* controls = new QHBoxLayout();
    controls->setSpacing(8);

    auto* minimize = new QFrame(d->titleBar);
    minimize->setFixedSize(12, 12);
    minimize->setObjectName("Minimize");

    auto* maximize = new QFrame(d->titleBar);
    maximize->setFixedSize(12, 12);
    maximize->setObjectName("Maximize");

    auto* close = new QFrame(d->titleBar);
    close->setFixedSize(12, 12);
    close->setObjectName("Close");

    controls->addWidget(minimize);
    controls->addWidget(maximize);
    controls->addWidget(close);
    layout->addLayout(controls);

    setMenuWidget(d->titleBar);
}
