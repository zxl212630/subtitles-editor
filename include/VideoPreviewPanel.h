#pragma once

#include <QWidget>
#include <QFontDatabase>

class QComboBox;
class QLabel;
class QFrame;
class QPushButton;

class VideoPreviewPanel : public QWidget
{
    Q_OBJECT

public:
    explicit VideoPreviewPanel(QWidget* parent = nullptr);

signals:
    void fontChanged(const QString& family);
    void fontSizeChanged(int size);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUi();
    void populateFontCombo();
    void populateSizeCombo();
    void updateHandlePositions();

    QComboBox* fontCombo_ = nullptr;
    QComboBox* sizeCombo_ = nullptr;
    QLabel* timeLabel_ = nullptr;
    QFrame* videoArea_ = nullptr;
    QList<QFrame*> handles_;
};
