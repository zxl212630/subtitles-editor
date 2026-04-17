#include "AppWindow.h"

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QFrame>

#include <QWindowKit/QWKWidgets/widgetwindowagent.h>

struct AppWindow::Private
{
    QWK::WidgetWindowAgent* windowAgent = nullptr;
    QFrame* titleBar = nullptr;
    QLabel* titleLabel = nullptr;
};

AppWindow::AppWindow(QWidget* parent)
    : QMainWindow(parent)
    , d(std::make_unique<Private>())
{
    setupUi();
}

AppWindow::~AppWindow() = default;

void AppWindow::setupUi()
{
    setWindowTitle("字幕编辑");
    resize(1440, 900);

    // Setup QWindowKit window agent
    d->windowAgent = new QWK::WidgetWindowAgent(this);
    d->windowAgent->setup(this);

    // Create title bar
    d->titleBar = new QFrame(this);
    d->titleBar->setFixedHeight(36);
    d->titleBar->setObjectName("TitleBar");
    d->titleBar->setStyleSheet(R"(
        QFrame#TitleBar {
            background-color: #252526;
        }
    )");

    auto* layout = new QHBoxLayout(d->titleBar);
    layout->setContentsMargins(12, 0, 12, 0);

    // Title centered
    d->titleLabel = new QLabel("字幕编辑", d->titleBar);
    d->titleLabel->setAlignment(Qt::AlignCenter);
    d->titleLabel->setStyleSheet(R"(
        QLabel {
            color: #FFFFFF;
            font-family: Inter, sans-serif;
            font-size: 16px;
            font-weight: 600;
            background: transparent;
        }
    )");
    layout->addWidget(d->titleLabel);

    // Set menu widget and title bar
    setMenuWidget(d->titleBar);
    d->windowAgent->setTitleBar(d->titleBar);

    // Central widget
    auto* central = new QWidget(this);
    central->setStyleSheet("background-color: #1E1E1E;");
    setCentralWidget(central);
}
