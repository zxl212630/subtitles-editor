#include "AsrProgressDialog.h"
#include "ThemeManager.h"
#include "TranslationManager.h"

#include <QCloseEvent>
#include <QDebug>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSvgRenderer>
#include <QtMath>

AsrProgressDialog::AsrProgressDialog(QWidget *parent) : BaseDialog(parent) {
  setObjectName("AsrProgressDialog");
  setWindowTitle(tr("AI Subtitle Generation"));
  setFixedSize(460, 320);

  setupTitleBar();

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 20);
  mainLayout->setSpacing(0);

  mainLayout->addWidget(titleBar);
  mainLayout->addStretch();

  statusLabel_ = new QLabel(tr("Initializing..."), this);
  statusLabel_->setObjectName("AsrStatusLabel");
  statusLabel_->setAlignment(Qt::AlignCenter);
  statusLabel_->setStyleSheet(
      "font-size: 13px; font-weight: bold; color: " +
      ThemeManager::instance().getTextNormalColor().name() + ";");
  mainLayout->addWidget(statusLabel_);

  subStatusLabel_ = new QLabel(this);
  subStatusLabel_->setObjectName("AsrSubStatusLabel");
  subStatusLabel_->setAlignment(Qt::AlignCenter);
  subStatusLabel_->setStyleSheet(
      "font-size: 11px; color: " +
      ThemeManager::instance().getTextMutedColor().name() + ";");
  subStatusLabel_->hide(); // Permanently hide as requested
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

  connect(cancelButton_, &QPushButton::clicked, this,
          &AsrProgressDialog::onCancelClicked);

  animTimer_ = new QTimer(this);
  animTimer_->setInterval(33);
  connect(animTimer_, &QTimer::timeout, this,
          &AsrProgressDialog::onAnimationTick);
  animTimer_->start();

  connect(&TranslationManager::instance(), &TranslationManager::languageChanged,
          this, &AsrProgressDialog::retranslateUi);

  setupWindowAgent(titleBar);
}

AsrProgressDialog::~AsrProgressDialog() = default;

void AsrProgressDialog::setStage(Stage stage) {
  currentStage_ = stage;
  retranslateUi();
  update();
}

void AsrProgressDialog::retranslateUi() {
  setWindowTitle(tr("AI Subtitle Generation"));

  if (!isError_) {
    switch (currentStage_) {
    case Stage::Extraction:
      statusLabel_->setText(tr("Extracting Audio..."));
      break;
    case Stage::Upload:
      statusLabel_->setText(tr("Uploading to Cloud..."));
      break;
    case Stage::Recognition:
      statusLabel_->setText(tr("Recognizing Speech..."));
      break;
    }
    cancelButton_->setText(tr("Cancel"));
  } else {
    statusLabel_->setText(tr("Error Occurred"));
    cancelButton_->setText(tr("Close"));
  }
  update();
}

void AsrProgressDialog::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QDialog::changeEvent(event);
}

void AsrProgressDialog::setStatus(const QString &mainText,
                                  const QString &subText) {
  statusLabel_->setText(mainText);
}

void AsrProgressDialog::setError(const QString &errorMessage) {
  isError_ = true;
  animTimer_->stop();

  // Clean error message (extract <Message> from XML or just keep first line)
  QString cleanMsg = errorMessage;
  if (errorMessage.contains("<Message>")) {
    int start = errorMessage.indexOf("<Message>") + 9;
    int end = errorMessage.indexOf("</Message>");
    if (end > start) {
      cleanMsg = errorMessage.mid(start, end - start);
    }
  } else if (errorMessage.contains(" - server replied:")) {
    // Common Qt Network error format
    cleanMsg = errorMessage.section(" - server replied:", 1).trimmed();
  }

  // If it's still too long, take the first sentence or first 100 chars
  if (cleanMsg.length() > 120) {
    cleanMsg = cleanMsg.left(117) + "...";
  }

  retranslateUi(); // Sets title to "Error Occurred"
  statusLabel_->setStyleSheet(
      "font-size: 13px; font-weight: bold; color: #ef4444;");

  // Reuse subStatusLabel for the cleaned error message but SHOW it only for
  // errors
  subStatusLabel_->setText(cleanMsg);
  subStatusLabel_->setWordWrap(true);
  subStatusLabel_->setFixedWidth(width() - 80);
  subStatusLabel_->show();

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

  // Title bar offset: paintEvent draws on dialog rect, but title bar
  // occupies the top 36px. Shift all Y coordinates down.
  const int titleH = 36;

  // ── Step indicator area ──
  const int stepY = 30 + titleH;
  const int stepR = 14;
  const int lineY = stepY;
  const int stepSpacing = 140;
  int stepStartX = (w - stepSpacing * 2) / 2;

  struct StepInfo {
    QString label;
    Stage stage;
  };
  StepInfo steps[] = {{tr("Extraction"), Stage::Extraction},
                      {tr("Upload"), Stage::Upload},
                      {tr("Recognition"), Stage::Recognition}};

  // Draw connecting lines
  QPen linePen(textMuted, 2);
  p.setPen(linePen);
  p.drawLine(stepStartX + stepR + 4, lineY,
             stepStartX + stepSpacing - stepR - 4, lineY);
  p.drawLine(stepStartX + stepSpacing + stepR + 4, lineY,
             stepStartX + stepSpacing * 2 - stepR - 4, lineY);

  // Highlight completed/active lines
  int currentIdx = static_cast<int>(currentStage_) - 1;
  if (isError_) {
    // Completed lines before the error stage stay green
    QPen completedPen(primary, 2);
    p.setPen(completedPen);
    for (int i = 0; i < currentIdx - 1; i++) {
      int x1 = stepStartX + stepSpacing * i + stepR + 4;
      int x2 = stepStartX + stepSpacing * (i + 1) - stepR - 4;
      p.drawLine(x1, lineY, x2, lineY);
    }
    // Only the line leading to the error stage turns red
    if (currentIdx > 0) {
      QPen errLinePen(errorColor, 2);
      p.setPen(errLinePen);
      int lineIdx = currentIdx - 1;
      int x1 = stepStartX + stepSpacing * lineIdx + stepR + 4;
      int x2 = stepStartX + stepSpacing * (lineIdx + 1) - stepR - 4;
      p.drawLine(x1, lineY, x2, lineY);
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
      p.setPen(
          QPen(Qt::white, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
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
  if (!isError_) {
    const int animTop = 75 + titleH; // Reduced from 90 to shrink vertical gap
    const int animH = 130;
    const int animCenter = w / 2;
    const int iconSize = 48;
    const int srcX = animCenter - 75;
    const int dstX = animCenter + 75;

    // Source icon
    drawSourceIcon(p, srcX, animTop + animH / 2, iconSize);
    // Target icon
    drawTargetIcon(p, dstX, animTop + animH / 2, iconSize);

    // Animated particles
    drawParticles(p, srcX + iconSize / 2 + 8, dstX - iconSize / 2 - 8,
                  animTop + animH / 2);
  }
}

void AsrProgressDialog::renderSVG(QPainter &p, const QString &resPath,
                                  const QRect &rect, const QColor &color) {
  QFile file(resPath);
  if (!file.open(QIODevice::ReadOnly))
    return;

  QByteArray svgData = file.readAll();
  // Replace currentColor with actual theme color
  svgData.replace("currentColor", color.name().toUtf8());

  QSvgRenderer renderer(svgData);
  renderer.render(&p, rect);
}

void AsrProgressDialog::drawSourceIcon(QPainter &p, int cx, int cy, int size) {
  int half = size / 2;
  QRect r(cx - half, cy - half, size, size);

  auto &theme = ThemeManager::instance();
  QColor primary =
      isError_ ? QColor(0xef, 0x44, 0x44) : theme.getPrimaryColor();

  switch (currentStage_) {
  case Stage::Extraction: {
    // 1. SVG Base for video frame
    renderSVG(p, ":/icons/asr_video_base.svg", r, primary);

    // 2. NATIVE: Film tracks scrolling (Inside SVG frame)
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x1e, 0x1e, 0x1e));
    int scrollOffset =
        (tickCount_ / 2) % 10; // Slowed down further from tickCount_ % 10
    // We draw slightly inside the 48x48 rect
    for (int y = -10; y < size + 10; y += 10) {
      p.drawRect(cx - half + 2.5, cy - half + y + scrollOffset, 5, 5);
      p.drawRect(cx + half - 7.5, cy - half + y + scrollOffset, 5, 5);
    }

    // 3. NATIVE: Pulsing Play triangle
    double pulse = (qSin(tickCount_ * 0.15) + 1.0) * 0.5;
    double scale = 0.9 + pulse * 0.2;
    p.save();
    p.translate(cx, cy);
    p.scale(scale, scale);
    QPolygonF tri;
    tri << QPointF(-4, -8) << QPointF(-4, 8) << QPointF(8, 0);
    p.setBrush(primary);
    p.setPen(Qt::NoPen);
    p.drawPolygon(tri);
    p.restore();
    break;
  }
  case Stage::Upload: {
    // Audio wave icon (source) - Slower pulse
    p.setPen(QPen(primary, 2.5, Qt::SolidLine, Qt::RoundCap));
    for (int i = -2; i <= 2; i++) {
      double waveScale = (qSin(tickCount_ * 0.2 + i * 0.5) + 1.0) * 0.5;
      int baseH = (i == 0) ? 24 : (qAbs(i) == 1 ? 16 : 10);
      int barH = baseH * (0.8 + waveScale * 0.4);
      p.drawLine(cx + i * 8, cy - barH / 2, cx + i * 8, cy + barH / 2);
    }
    break;
  }
  case Stage::Recognition: {
    // 1. SVG Base for cloud
    double floatY = qSin(tickCount_ * 0.1) * 3;
    p.save();
    p.translate(0, floatY);
    renderSVG(p, ":/icons/asr_cloud.svg", r, primary);

    // 2. NATIVE: Inner voice bars (wrapped in cloud)
    p.setPen(QPen(primary, 2, Qt::SolidLine, Qt::RoundCap));
    for (int i = -1; i <= 1; i++) {
      double waveScale = (qSin(tickCount_ * 0.3 + i * 0.8) + 1.0) * 0.5;
      int barH = (i == 0 ? 18 : 10) * (0.7 + waveScale * 0.6);
      p.drawLine(cx + i * 6, cy - barH / 2 + 2, cx + i * 6, cy + barH / 2 + 2);
    }
    p.restore();
    break;
  }
  }
}

void AsrProgressDialog::drawTargetIcon(QPainter &p, int cx, int cy, int size) {
  int half = size / 2;
  QRect r(cx - half, cy - half, size, size);

  auto &theme = ThemeManager::instance();
  QColor primary =
      isError_ ? QColor(0xef, 0x44, 0x44) : theme.getPrimaryColor();

  switch (currentStage_) {
  case Stage::Extraction: {
    // Audio wave icon (target)
    p.setPen(QPen(primary, 2.5, Qt::SolidLine, Qt::RoundCap));
    for (int i = -2; i <= 2; i++) {
      double waveScale = (qSin(tickCount_ * 0.2 + i * 0.5) + 1.0) * 0.5;
      int baseH = (i == 0) ? 24 : (qAbs(i) == 1 ? 16 : 10);
      int barH = baseH * (0.8 + waveScale * 0.4);
      p.drawLine(cx + i * 8, cy - barH / 2, cx + i * 8, cy + barH / 2);
    }
    break;
  }
  case Stage::Upload: {
    // 1. SVG Base for Symmetrical Cloud
    double pulse = (qSin(tickCount_ * 0.1) + 1.0) * 0.5;
    double scale = 0.95 + pulse * 0.1;

    p.save();
    p.translate(cx, cy);
    p.scale(scale, scale);
    p.translate(-cx, -cy);
    renderSVG(p, ":/icons/asr_cloud.svg", r, primary);

    // 2. NATIVE: Glow effect
    QRadialGradient glow(cx, cy, 30);
    QColor glowColor = primary;
    glowColor.setAlphaF(pulse * 0.3);
    glow.setColorAt(0, glowColor);
    glow.setColorAt(1, Qt::transparent);
    p.setBrush(glow);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPoint(cx, cy), 40, 40);
    p.restore();
    break;
  }
  case Stage::Recognition: {
    // 1. SVG Base for Text Frame
    renderSVG(p, ":/icons/asr_text_base.svg", r, primary);

    // 2. NATIVE: Typewriter lines
    p.setBrush(primary);
    int lineCount = 4;
    int lineSpacing = 8;
    int startY = cy - (lineCount * lineSpacing) / 2 + 4;

    for (int i = 0; i < lineCount; i++) {
      double typePhase = fmod(tickCount_ * 0.02 + i * 0.25, 1.0);
      double lineWeight = (i == 3) ? 0.5 : (i % 2 == 0 ? 0.8 : 0.6);
      double currentWidth = (size - 16) * lineWeight;
      if (typePhase < 0.4)
        currentWidth *= (typePhase / 0.4);
      p.setOpacity(0.9);
      p.drawRoundedRect(cx - (size - 16) * lineWeight / 2,
                        startY + i * lineSpacing, currentWidth, 3, 1.5, 1.5);
    }
    p.setOpacity(1.0);

    // 3. NATIVE: Scanning beam
    double scanY = fmod(tickCount_ * 0.03, 1.0);
    QLinearGradient scanGrad(0, cy - half + scanY * size - 5, 0,
                             cy - half + scanY * size + 5);
    scanGrad.setColorAt(0, Qt::transparent);
    scanGrad.setColorAt(
        0.5, QColor(primary.red(), primary.green(), primary.blue(), 60));
    scanGrad.setColorAt(1, Qt::transparent);
    p.fillRect(cx - half + 4, cy - half + 4, size - 8, size - 8, scanGrad);
    break;
  }
  }
}

void AsrProgressDialog::drawParticles(QPainter &p, int x1, int x2, int cy) {
  if (isError_)
    return;

  auto &theme = ThemeManager::instance();
  QColor color = theme.getPrimaryColor();

  const int particleCount = 20; // Increased count
  double span = x2 - x1;

  for (int i = 0; i < particleCount; i++) {
    // Each particle has its own speed and horizontal phase
    // We use i to seed pseudo-randomness for Y offset and delay
    double speed = 0.01 + (double)(i % 5) * 0.005;
    double phase = fmod(tickCount_ * speed + (double)i / particleCount, 1.0);

    double x = x1 + phase * span;

    // Messy vertical distribution
    int yOffset = ((i * 17) % 40) - 20; // Spread between -20 and 20
    double y = cy + yOffset;

    double alpha = qSin(phase * M_PI);
    QColor c = color;
    c.setAlphaF(alpha * 0.7);

    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawEllipse(QPointF(x, y), 1.5, 1.5); // Smaller particles

    // Subtle glow for some particles
    if (i % 3 == 0) {
      c.setAlphaF(alpha * 0.2);
      p.setBrush(c);
      p.drawEllipse(QPointF(x, y), 4, 4);
    }
  }
}
