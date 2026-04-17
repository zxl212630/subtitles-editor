#pragma once

#include <QMainWindow>

class AppWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AppWindow(QWidget* parent = nullptr);
    ~AppWindow() override;

private:
    void setupUi();
    void setupTitleBar();

private:
    struct Private;
    Private* d;
};
