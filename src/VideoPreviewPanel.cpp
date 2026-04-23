#include "VideoPreviewPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QFontDatabase>
#include <QValidator>

static QPushButton* createIconBtn(QWidget* parent, const QString& text, int w, int h,
                                   const QString& bg = "#333333", const QString& color = "#d1d5db")
{
    auto* btn = new QPushButton(text, parent);
    btn->setFixedSize(w, h);
    btn->setStyleSheet(QString(R"(
        QPushButton {
            background-color: %1;
            color: %2;
            border: none;
            border-radius: 4px;
            font-family: Inter, sans-serif;
            font-size: 12px;
            font-weight: bold;
        }
    )").arg(bg, color));
    return btn;
}

VideoPreviewPanel::VideoPreviewPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void VideoPreviewPanel::setupUi()
{
    setObjectName("VideoPreviewPanel");
    setStyleSheet(R"(
        QWidget#VideoPreviewPanel {
            background-color: #1e1e1e;
            border-radius: 10px;
            border: 1px solid #333333;
        }
    )");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // --- Toolbar ---
    auto* toolbar = new QFrame(this);
    toolbar->setFixedHeight(40);
    toolbar->setStyleSheet(R"(
        QFrame {
            background-color: #262626;
            border-top-left-radius: 10px;
            border-top-right-radius: 10px;
            border: none;
        }
    )");
    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(12, 0, 16, 0);
    tbLayout->setSpacing(12);
    tbLayout->setAlignment(Qt::AlignVCenter);

    // Font combo
    fontCombo_ = new QComboBox(toolbar);
    fontCombo_->setFixedSize(140, 28);
    fontCombo_->setStyleSheet(R"(
        QComboBox {
            background-color: #141414;
            color: #d1d5db;
            border: none;
            border-radius: 4px;
            padding-left: 8px;
            font-family: Inter, sans-serif;
            font-size: 12px;
        }
        QComboBox::drop-down { border: none; width: 20px; }
        QComboBox QAbstractItemView {
            background-color: #141414;
            color: #d1d5db;
            selection-background-color: #333333;
        }
    )");
    populateFontCombo();
    tbLayout->addWidget(fontCombo_);

    connect(fontCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { emit fontChanged(fontCombo_->currentText()); });

    // Size combo
    sizeCombo_ = new QComboBox(toolbar);
    sizeCombo_->setFixedSize(60, 28);
    sizeCombo_->setEditable(true);
    sizeCombo_->setStyleSheet(R"(
        QComboBox {
            background-color: #141414;
            color: #d1d5db;
            border: none;
            border-radius: 4px;
            padding-left: 8px;
            font-family: Inter, sans-serif;
            font-size: 12px;
        }
        QComboBox::drop-down { border: none; width: 20px; }
    )");
    populateSizeCombo();
    tbLayout->addWidget(sizeCombo_);

    connect(sizeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { emit fontSizeChanged(sizeCombo_->currentText().toInt()); });

    // Size input validation
    auto* validator = new QIntValidator(1, 999, sizeCombo_);
    sizeCombo_->setValidator(validator);

    // Elastic spacer
    auto* tbSpacer = new QWidget(toolbar);
    tbSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tbLayout->addWidget(tbSpacer);

    // Format buttons
    tbLayout->addWidget(createIconBtn(toolbar, "B", 28, 28));
    tbLayout->addWidget(createIconBtn(toolbar, "I", 28, 28));
    tbLayout->addWidget(createIconBtn(toolbar, "U", 28, 28));
    tbLayout->addWidget(createIconBtn(toolbar, QString(QChar(0x2261)), 28, 28));
    tbLayout->addWidget(createIconBtn(toolbar, QString(QChar(0x2261)), 28, 28));
    tbLayout->addWidget(createIconBtn(toolbar, QString(QChar(0x2261)), 28, 28));

    layout->addWidget(toolbar);

    // --- Video display area ---
    videoArea_ = new QFrame(this);
    videoArea_->setStyleSheet("background-color: transparent; border: none;");
    videoArea_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* vaLayout = new QVBoxLayout(videoArea_);
    vaLayout->setContentsMargins(40, 0, 40, 0);
    vaLayout->setAlignment(Qt::AlignCenter);

    auto* blackRect = new QFrame(videoArea_);
    blackRect->setStyleSheet("background-color: #000000; border: none;");
    blackRect->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    vaLayout->addWidget(blackRect);

    // Drag handles (absolute positioned over videoArea)
    // Positions are relative to videoArea content (excluding 40px left/right padding)
    auto addHandle = [&](int x, int y) {
        auto* h = new QFrame(videoArea_);
        h->setFixedSize(6, 6);
        h->setStyleSheet("background-color: #38bdf8; border-radius: 1px;");
        h->move(x, y);
        handles_.append(h);
        return h;
    };

    // Initial positions; will be updated in resizeEvent
    addHandle(0, 0); // TL
    addHandle(0, 0); // TM
    addHandle(0, 0); // TR
    addHandle(0, 0); // ML
    addHandle(0, 0); // MR
    addHandle(0, 0); // BL
    addHandle(0, 0); // BM
    addHandle(0, 0); // BR

    layout->addWidget(videoArea_, 1);

    // --- Playback control bar ---
    auto* controlBar = new QFrame(this);
    controlBar->setFixedHeight(36);
    controlBar->setStyleSheet(R"(
        QFrame {
            background-color: #262626;
            border-bottom-left-radius: 10px;
            border-bottom-right-radius: 10px;
            border: none;
        }
    )");
    auto* cbLayout = new QHBoxLayout(controlBar);
    cbLayout->setContentsMargins(8, 0, 12, 0);
    cbLayout->setSpacing(8);
    cbLayout->setAlignment(Qt::AlignVCenter);

    auto addIconLabel = [&](const QString& text, int w, int h) {
        auto* lbl = new QLabel(text, controlBar);
        lbl->setFixedSize(w, h);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("color: #d1d5db; font-family: Inter; font-size: 12px; background: transparent;");
        cbLayout->addWidget(lbl);
    };

    addIconLabel(QString(QChar(0x23EE)), 16, 16);
    addIconLabel(QString(QChar(0x23ED)), 16, 16);
    addIconLabel(QString(QChar(0x25B6)), 16, 16);
    addIconLabel(QString(QChar(0x25A0)), 14, 14);

    // Progress bar container
    auto* progressContainer = new QFrame(controlBar);
    progressContainer->setFixedSize(550, 4);
    progressContainer->setStyleSheet("background-color: #333333; border-radius: 2px;");
    auto* progressFill = new QFrame(progressContainer);
    progressFill->setFixedSize(260, 4);
    progressFill->setStyleSheet("background-color: #38bdf8; border-radius: 2px;");
    progressFill->move(0, 0);
    cbLayout->addWidget(progressContainer);

    timeLabel_ = new QLabel("00:00:00:00 / 00:00:00:00", controlBar);
    timeLabel_->setStyleSheet("color: #d1d5db; font-family: Inter, sans-serif; font-size: 11px; background: transparent;");
    cbLayout->addWidget(timeLabel_);

    addIconLabel("Vol", 24, 16);
    addIconLabel("FS", 20, 16);

    layout->addWidget(controlBar);
}

void VideoPreviewPanel::populateFontCombo()
{
    QFontDatabase db;
    QStringList families = db.families();
    families.sort();

    for (const QString& family : families) {
        if (family.startsWith('.') || family.isEmpty()) continue;
        fontCombo_->addItem(family);
    }

    int idx = fontCombo_->findText("Arial");
    if (idx >= 0) {
        fontCombo_->setCurrentIndex(idx);
    } else if (fontCombo_->count() > 0) {
        fontCombo_->setCurrentIndex(0);
    }
}

void VideoPreviewPanel::populateSizeCombo()
{
    const QList<int> sizes = {8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 28, 32, 36, 40, 48, 56, 64, 72};
    for (int s : sizes) {
        sizeCombo_->addItem(QString::number(s));
    }
    sizeCombo_->setCurrentText("24");
}

void VideoPreviewPanel::updateHandlePositions()
{
    if (!videoArea_ || handles_.size() != 8) return;

    int w = videoArea_->width();
    int h = videoArea_->height();
    int left = 48;   // 40 padding + 8 offset
    int right = w - 48;
    int midX = w / 2 - 3;
    int top = 40;
    int midY = h / 2 - 3;
    int bottom = h - 40;

    handles_[0]->move(left, top);    // TL
    handles_[1]->move(midX, top);    // TM
    handles_[2]->move(right, top);   // TR
    handles_[3]->move(left, midY);   // ML
    handles_[4]->move(right, midY);  // MR
    handles_[5]->move(left, bottom); // BL
    handles_[6]->move(midX, bottom); // BM
    handles_[7]->move(right, bottom); // BR
}

void VideoPreviewPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateHandlePositions();
}
