#pragma once

#include <QMainWindow>
#include <memory>

class AppWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AppWindow(QWidget* parent = nullptr);
    ~AppWindow() override;

private:
    void setupUi();

private:
    struct Private;
    std::unique_ptr<Private> d;
};
