#include "AsrProgressDialog.h"
#include "ThemeManager.h"

#include <QCloseEvent>
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtMath>

AsrProgressDialog::AsrProgressDialog(QWidget *parent) : QDialog(parent) {
    setObjectName("AsrProgressDialog");
    setWindowTitle(tr("语音识别"));
    setFixedSize(460, 320);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 20);
    mainLayout->setSpacing(0);

    mainLayout->addStretch();

    statusLabel_ = new QLabel(tr("准备中..."), this);
    statusLabel_->setObjectName("AsrStatusLabel");
    statusLabel_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(statusLabel_);

    subStatusLabel_ = new QLabel(this);
    subStatusLabel_->setObjectName("AsrSubStatusLabel");
    subStatusLabel_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(subStatusLabel_);

    mainLayout->addSpacing(12);

    auto *btnLayout = new QHBoxLayout();
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setAlignment(Qt::AlignCenter);

    cancelButton_ = new QPushButton(tr("取消"), this);
    cancelButton_->setObjectName("AsrCancelButton");
    cancelButton_->setFixedWidth(100);
    btnLayout->addWidget(cancelButton_);

    mainLayout->addLayout(btnLayout);

    connect(cancelButton_, &QPushButton::clicked, this, &AsrProgressDialog::onCancelClicked);

    animTimer_ = new QTimer(this);
    animTimer_->setInterval(33);
    connect(animTimer_, &QTimer::timeout, this, &AsrProgressDialog::onAnimationTick);
    animTimer_->start();
}

AsrProgressDialog::~AsrProgressDialog() = default;

void AsrProgressDialog::setStage(Stage stage) {
    currentStage_ = stage;
    update();
}

void AsrProgressDialog::setStatus(const QString &mainText, const QString &subText) {
    statusLabel_->setText(mainText);
    subStatusLabel_->setText(subText);
}

void AsrProgressDialog::setError(const QString &errorMessage) {
    isError_ = true;
    animTimer_->stop();
    statusLabel_->setText(errorMessage);
    statusLabel_->setStyleSheet("color: #ef4444;");
    subStatusLabel_->clear();
    cancelButton_->setText(tr("关闭"));
    update();
}

void AsrProgressDialog::onAnimationTick() {
    tickCount_++;
    update();
}

void AsrProgressDialog::onCancelClicked() {
    qDebug() << "[ASR Dialog] onCancelClicked, canceled_=" << canceled_;
    if (!canceled_) {
        canceled_ = true;
        qDebug() << "[ASR Dialog] emitting canceled signal";
        emit canceled();
    }
    qDebug() << "[ASR Dialog] calling close()";
    close();
}

void AsrProgressDialog::closeEvent(QCloseEvent *event) {
    qDebug() << "[ASR Dialog] closeEvent, canceled_=" << canceled_;
    if (!canceled_) {
        canceled_ = true;
        qDebug() << "[ASR Dialog] closeEvent emitting canceled signal";
        emit canceled();
    }
    event->accept();
}

void AsrProgressDialog::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    auto &theme = ThemeManager::instance();

    // Background
    p.fillRect(rect(), theme.getBgPanelColor());

    QColor primary = theme.getPrimaryColor();
    QColor textNormal = theme.getTextNormalColor();
    QColor textMuted = theme.getTextMutedColor();
    QColor errorColor(0xef, 0x44, 0x44);

    int w = width();

    // ── Step indicator area ──
    const int stepY = 30;
    const int stepR = 14;
    const int lineY = stepY + stepR;
    const int stepSpacing = 140;
    int stepStartX = (w - stepSpacing * 2) / 2;

    struct StepInfo {
        QString label;
        Stage stage;
    };
    StepInfo steps[] = {
        {tr("提取"), Stage::Extraction},
        {tr("上传"), Stage::Upload},
        {tr("识别"), Stage::Recognition}
    };

    // Draw connecting lines
    QPen linePen(textMuted, 2);
    p.setPen(linePen);
    p.drawLine(stepStartX + stepR + 4, lineY, stepStartX + stepSpacing - stepR - 4, lineY);
    p.drawLine(stepStartX + stepSpacing + stepR + 4, lineY,
               stepStartX + stepSpacing * 2 - stepR - 4, lineY);

    // Highlight completed/active lines
    int currentIdx = static_cast<int>(currentStage_) - 1;
    if (isError_) {
        QPen errLinePen(errorColor, 2);
        p.setPen(errLinePen);
        if (currentIdx > 0) {
            p.drawLine(stepStartX + stepR + 4, lineY,
                       stepStartX + stepSpacing - stepR - 4, lineY);
        }
        if (currentIdx > 1) {
            p.drawLine(stepStartX + stepSpacing + stepR + 4, lineY,
                       stepStartX + stepSpacing * 2 - stepR - 4, lineY);
        }
    } else {
        QPen activeLinePen(primary, 2);
        p.setPen(activeLinePen);
        for (int i = 0; i < currentIdx; i++) {
            int x1 = stepStartX + stepSpacing * i + stepR + 4;
            int x2 = stepStartX + stepSpacing * (i + 1) - stepR - 4;
            p.drawLine(x1, lineY, x2, lineY);
        }
    }

    // Draw step circles and labels
    for (int i = 0; i < 3; i++) {
        int cx = stepStartX + stepSpacing * i;
        bool isActive = (i == currentIdx);
        bool isDone = (i < currentIdx);

        QColor circleColor;
        if (isError_ && isActive) {
            circleColor = errorColor;
        } else if (isDone || isActive) {
            circleColor = primary;
        } else {
            circleColor = textMuted;
        }

        // Circle fill
        if (isDone) {
            p.setPen(Qt::NoPen);
            p.setBrush(circleColor);
            p.drawEllipse(QPoint(cx, stepY), stepR, stepR);

            // Checkmark
            p.setPen(QPen(Qt::white, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawLine(cx - 5, stepY, cx - 1, stepY + 5);
            p.drawLine(cx - 1, stepY + 5, cx + 6, stepY - 5);
        } else {
            // Ring
            p.setBrush(isActive ? circleColor : Qt::NoBrush);
            p.setPen(QPen(circleColor, 2.5));
            p.drawEllipse(QPoint(cx, stepY), stepR, stepR);

            if (isActive && !isError_) {
                // Pulse animation on active circle
                double pulse = (qSin(tickCount_ * 0.15) + 1.0) * 0.5;
                int pulseR = stepR + static_cast<int>(pulse * 4);
                QColor pulseColor = circleColor;
                pulseColor.setAlphaF(0.3 * (1.0 - pulse));
                p.setPen(Qt::NoPen);
                p.setBrush(pulseColor);
                p.drawEllipse(QPoint(cx, stepY), pulseR, pulseR);
            }

            if (isActive && isError_) {
                // X mark for error
                p.setPen(QPen(Qt::white, 2.5, Qt::SolidLine, Qt::RoundCap));
                int s = 5;
                p.drawLine(cx - s, stepY - s, cx + s, stepY + s);
                p.drawLine(cx + s, stepY - s, cx - s, stepY + s);
            } else if (isActive) {
                // Inner dot
                p.setPen(Qt::NoPen);
                p.setBrush(Qt::white);
                p.drawEllipse(QPoint(cx, stepY), 4, 4);
            }
        }

        // Step label
        p.setPen(isActive ? textNormal : textMuted);
        QFont labelFont = font();
        labelFont.setPointSize(10);
        p.setFont(labelFont);
        p.drawText(QRect(cx - 40, stepY + stepR + 6, 80, 20),
                   Qt::AlignHCenter | Qt::AlignTop, steps[i].label);
    }

    // ── Animation area ──
    const int animTop = 90;
    const int animH = 130;
    const int animCenter = w / 2;
    const int iconSize = 48;
    const int srcX = animCenter - 110;
    const int dstX = animCenter + 110;

    // Source icon
    drawSourceIcon(p, srcX, animTop + animH / 2, iconSize);
    // Target icon
    drawTargetIcon(p, dstX, animTop + animH / 2, iconSize);

    // Animated particles
    drawParticles(p, srcX + iconSize / 2 + 8, dstX - iconSize / 2 - 8,
                  animTop + animH / 2);
}

void AsrProgressDialog::drawSourceIcon(QPainter &p, int cx, int cy, int size) {
    int half = size / 2;
    QRect r(cx - half, cy - half, size, size);

    auto &theme = ThemeManager::instance();
    QColor color = isError_ ? QColor(0xef, 0x44, 0x44) : theme.getPrimaryColor();
    p.setPen(QPen(color, 2));
    p.setBrush(Qt::NoBrush);

    switch (currentStage_) {
    case Stage::Extraction: {
        // Film / video rectangle
        p.drawRoundedRect(r, 4, 4);
        // Play triangle
        QPolygonF tri;
        tri << QPointF(cx - 6, cy - 10) << QPointF(cx - 6, cy + 10)
            << QPointF(cx + 10, cy);
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawPolygon(tri);
        break;
    }
    case Stage::Upload: {
        // Audio wave icon
        p.setPen(QPen(color, 2.5, Qt::SolidLine, Qt::RoundCap));
        for (int i = -2; i <= 2; i++) {
            int barH = (i == 0) ? 20 : (qAbs(i) == 1 ? 14 : 8);
            p.drawLine(cx + i * 7, cy - barH / 2, cx + i * 7, cy + barH / 2);
        }
        break;
    }
    case Stage::Recognition: {
        // Cloud icon
        QPainterPath cloud;
        cloud.addEllipse(cx - 14, cy + 2, 20, 16);
        cloud.addEllipse(cx - 6, cy - 10, 22, 22);
        cloud.addEllipse(cx + 6, cy, 18, 16);
        p.setPen(QPen(color, 2));
        p.setBrush(Qt::NoBrush);
        p.drawPath(cloud);
        break;
    }
    }
}

void AsrProgressDialog::drawTargetIcon(QPainter &p, int cx, int cy, int size) {
    int half = size / 2;
    QRect r(cx - half, cy - half, size, size);

    auto &theme = ThemeManager::instance();
    QColor color = isError_ ? QColor(0xef, 0x44, 0x44) : theme.getPrimaryColor();
    p.setPen(QPen(color, 2));
    p.setBrush(Qt::NoBrush);

    switch (currentStage_) {
    case Stage::Extraction: {
        // Audio wave icon (target)
        p.setPen(QPen(color, 2.5, Qt::SolidLine, Qt::RoundCap));
        for (int i = -2; i <= 2; i++) {
            int barH = (i == 0) ? 20 : (qAbs(i) == 1 ? 14 : 8);
            p.drawLine(cx + i * 7, cy - barH / 2, cx + i * 7, cy + barH / 2);
        }
        break;
    }
    case Stage::Upload: {
        // Cloud icon (target)
        QPainterPath cloud;
        cloud.addEllipse(cx - 14, cy + 2, 20, 16);
        cloud.addEllipse(cx - 6, cy - 10, 22, 22);
        cloud.addEllipse(cx + 6, cy, 18, 16);
        p.setPen(QPen(color, 2));
        p.setBrush(Qt::NoBrush);
        p.drawPath(cloud);
        break;
    }
    case Stage::Recognition: {
        // Text lines icon (target)
        p.setPen(QPen(color, 2, Qt::SolidLine, Qt::RoundCap));
        int lineW = 24;
        for (int i = -1; i <= 1; i++) {
            int lw = (i == 0) ? lineW : lineW - 6;
            int lx = cx - lw / 2;
            int ly = cy + i * 10;
            p.drawLine(lx, ly, lx + lw, ly);
        }
        break;
    }
    }
}

void AsrProgressDialog::drawParticles(QPainter &p, int x1, int x2, int cy) {
    if (isError_)
        return;

    auto &theme = ThemeManager::instance();
    QColor color = theme.getPrimaryColor();

    const int particleCount = 6;
    double span = x2 - x1;
    double speed = 0.02;

    for (int i = 0; i < particleCount; i++) {
        double phase = fmod(tickCount_ * speed + (double)i / particleCount, 1.0);
        double x = x1 + phase * span;
        double alpha = qSin(phase * M_PI);
        QColor c = color;
        c.setAlphaF(alpha * 0.8);

        int r = 3 + static_cast<int>(alpha * 2);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QPointF(x, cy), r, r);
    }
}
