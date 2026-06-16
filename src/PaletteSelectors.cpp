#include "PaletteSelectors.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

// --- ThemeSelectorWidget ---

ThemeSelectorWidget::ThemeSelectorWidget(QWidget *parent) : QWidget(parent) {
  setMouseTracking(true);
  setFixedSize(400, 70);
}

void ThemeSelectorWidget::addTheme(const QString &id, const QString &bgBase,
                                   const QString &bgPanel,
                                   const QString &primary) {
  items_.append({id, bgBase, bgPanel, primary});
  update();
}

void ThemeSelectorWidget::setCurrentTheme(const QString &id) {
  if (currentId_ != id) {
    currentId_ = id;
    update();
    emit themeSelected(id);
  }
}

QString ThemeSelectorWidget::currentTheme() const { return currentId_; }

void ThemeSelectorWidget::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  int itemWidth = 80;
  int itemHeight = 60;
  int spacing = 15;
  int x = 5; // Start with some offset to avoid clipping the selection border

  for (int i = 0; i < items_.size(); ++i) {
    const auto &item = items_[i];

    QRect rect(x, 5, itemWidth, itemHeight);

    // Draw selection border
    if (item.id == currentId_) {
      QPainterPath path;
      path.addRoundedRect(rect.adjusted(-3, -3, 3, 3), 8, 8);
      p.setPen(QPen(QColor(item.primary), 2));
      p.setBrush(Qt::NoBrush);
      p.drawPath(path);
    }

    // Draw miniature UI
    QPainterPath clipPath;
    clipPath.addRoundedRect(rect, 6, 6);
    p.setClipPath(clipPath);

    // Base bg
    p.fillRect(rect, QColor(item.bgBase));

    // Sidebar mock
    QRect sidebarRect(rect.x(), rect.y(), rect.width() * 0.3, rect.height());
    p.fillRect(sidebarRect, QColor(item.bgPanel));

    // Mock lines in sidebar
    p.fillRect(sidebarRect.x() + 4, sidebarRect.y() + 8, 12, 2,
               QColor("#888888"));
    p.fillRect(sidebarRect.x() + 4, sidebarRect.y() + 14, 8, 2,
               QColor("#888888"));
    p.fillRect(sidebarRect.x() + 4, sidebarRect.y() + 20, 10, 2,
               QColor("#888888"));

    // Content mock lines
    int contentX = sidebarRect.right() + 8;
    p.fillRect(contentX, rect.y() + 8, 20, 4,
               QColor(item.primary)); // Mock title
    p.fillRect(contentX, rect.y() + 18, 35, 3,
               QColor(item.bgPanel).lighter(120));
    p.fillRect(contentX, rect.y() + 26, 25, 3,
               QColor(item.bgPanel).lighter(120));
    p.fillRect(contentX, rect.y() + 34, 40, 3,
               QColor(item.bgPanel).lighter(120));

    p.setClipping(false);
    x += itemWidth + spacing;
  }
}

void ThemeSelectorWidget::mousePressEvent(QMouseEvent *event) {
  int itemWidth = 80;
  int spacing = 15;
  int xStart = 5;

  int index = (event->pos().x() - xStart) / (itemWidth + spacing);
  if (index >= 0 && index < items_.size()) {
    int xInItem = (event->pos().x() - xStart) % (itemWidth + spacing);
    if (xInItem >= 0 && xInItem <= itemWidth) {
      setCurrentTheme(items_[index].id);
    }
  }
}

void ThemeSelectorWidget::mouseMoveEvent(QMouseEvent *event) {
  int itemWidth = 80;
  int spacing = 15;
  int xStart = 5;
  int index = (event->pos().x() - xStart) / (itemWidth + spacing);

  int newHover = -1;
  if (index >= 0 && index < items_.size()) {
    int xInItem = (event->pos().x() - xStart) % (itemWidth + spacing);
    if (xInItem >= 0 && xInItem <= itemWidth) {
      newHover = index;
    }
  }

  if (newHover != hoverIndex_) {
    hoverIndex_ = newHover;
    update();
  }
}

// --- ColorSelectorWidget ---

ColorSelectorWidget::ColorSelectorWidget(QWidget *parent) : QWidget(parent) {
  setMouseTracking(true);
  setFixedSize(500, 40);
}

void ColorSelectorWidget::addColor(const QString &id, const QString &hex) {
  items_.append({id, hex});
  update();
}

void ColorSelectorWidget::setCurrentColor(const QString &id) {
  if (currentId_ != id) {
    currentId_ = id;
    update();
    emit colorSelected(id);
  }
}

QString ColorSelectorWidget::currentColor() const { return currentId_; }

void ColorSelectorWidget::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  int radius = 16;
  int spacing = 16;
  int xOffset = 4;
  int x = xOffset + radius;

  for (int i = 0; i < items_.size(); ++i) {
    const auto &item = items_[i];
    QPoint center(x, height() / 2);

    // Hover or selected outline
    if (item.id == currentId_) {
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(item.hex));
      p.drawEllipse(center, radius, radius);

      // Draw checkmark
      p.setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      p.drawLine(center.x() - 4, center.y(), center.x() - 1, center.y() + 4);
      p.drawLine(center.x() - 1, center.y() + 4, center.x() + 5,
                 center.y() - 4);
    } else {
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(item.hex));
      p.drawEllipse(center, radius, radius);

      if (i == hoverIndex_) {
        p.setPen(QPen(QColor(255, 255, 255, 100), 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(center, radius + 2, radius + 2);
      }
    }

    x += radius * 2 + spacing;
  }
}

void ColorSelectorWidget::mousePressEvent(QMouseEvent *event) {
  int radius = 16;
  int spacing = 16;
  int xOffset = 4;
  int cellWidth = radius * 2 + spacing;

  int index = (event->pos().x() - xOffset) / cellWidth;
  if (index >= 0 && index < items_.size()) {
    int xCenter = xOffset + index * cellWidth + radius;
    if (qAbs(event->pos().x() - xCenter) <= radius) {
      setCurrentColor(items_[index].id);
    }
  }
}

void ColorSelectorWidget::mouseMoveEvent(QMouseEvent *event) {
  int radius = 16;
  int spacing = 16;
  int xOffset = 4;
  int cellWidth = radius * 2 + spacing;
  int index = (event->pos().x() - xOffset) / cellWidth;

  int newHover = -1;
  if (index >= 0 && index < items_.size()) {
    int xCenter = xOffset + index * cellWidth + radius;
    if (qAbs(event->pos().x() - xCenter) <= radius) {
      newHover = index;
    }
  }

  if (newHover != hoverIndex_) {
    hoverIndex_ = newHover;
    update();
  }
}
