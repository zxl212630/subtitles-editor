#include "SubtitleListDelegate.h"

#include "SubtitleListModel.h"

#include <QMouseEvent>
#include <QPainter>
#include <QStyleOptionViewItem>

SubtitleListDelegate::SubtitleListDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

void SubtitleListDelegate::paint(QPainter *painter,
                                 const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const {
  painter->save();

  const bool isSelected = option.state & QStyle::State_Selected;
  const QRect rect = option.rect;

  // Hover state - use manual tracking via hoveredIndex_
  const bool isHovered = index == hoveredIndex_;

  // Background
  if (isSelected) {
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor("#1f2937"));
    painter->drawRoundedRect(rect.adjusted(4, 2, -4, -2), 5, 5);
  }

  // Data
  QString startTime = index.data(SubtitleListModel::StartTimeRole).toString();
  QString endTime = index.data(SubtitleListModel::EndTimeRole).toString();
  QString text = index.data(SubtitleListModel::TextRole).toString();

  // Timecode area (left)
  QRect timeRect(rect.left() + 12, rect.top() + 10, 100, 36);
  painter->setPen(QColor("#858e9f"));
  QFont timeFont = painter->font();
  timeFont.setFamily("Inter");
  timeFont.setPointSize(11);
  painter->setFont(timeFont);
  painter->drawText(timeRect.left(), timeRect.top() + 12, startTime);
  painter->drawText(timeRect.left(), timeRect.top() + 26, endTime);

  // Text area (middle)
  int textLeft = timeRect.right() + 12;
  int textRight = rect.right() - 12 - 36 - 12; // right padding - buttons - gap
  QRect textRect(textLeft, rect.top(), qMax(50, textRight - textLeft),
                 rect.height());
  painter->setPen(QColor("#d1d5db"));
  QFont textFont = painter->font();
  textFont.setFamily("Inter");
  textFont.setPointSize(12);
  painter->setFont(textFont);
  painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);

  // Action buttons area (right)
  const int btnBox = 18;
  int btnY = rect.top() + (rect.height() - btnBox) / 2;
  int btnX = rect.right() - 12 - 36;

  // Split button (scissors icon) - hover brightens
  QColor splitColor = (isHovered && hoveredButton_ == 1) ? QColor("#ffffff")
                                                         : QColor("#6b7280");
  painter->setPen(splitColor);
  painter->setBrush(Qt::NoBrush);
  QFont iconFont = painter->font();
  iconFont.setPointSize(12);
  painter->setFont(iconFont);
  QRect scissorsRect(btnX, btnY, btnBox, btnBox);
  painter->drawText(scissorsRect, Qt::AlignCenter, "\u2702"); // scissors

  // Delete button (x icon) - hover brightens (white instead of red)
  QColor deleteColor = (isHovered && hoveredButton_ == 2) ? QColor("#ffffff")
                                                          : QColor("#6b7280");
  painter->setPen(deleteColor);
  QRect deleteRect(btnX + 22, btnY, btnBox, btnBox);
  painter->drawText(deleteRect, Qt::AlignCenter, "\u2715"); // x mark

  painter->restore();
}

QSize SubtitleListDelegate::sizeHint(const QStyleOptionViewItem & /*option*/,
                                     const QModelIndex & /*index*/) const {
  return QSize(200, 56);
}

QString SubtitleListDelegate::formatTime(qint64 ms) {
  const int hours = ms / 3600000;
  const int minutes = (ms % 3600000) / 60000;
  const int seconds = (ms % 60000) / 1000;
  const int frames = (ms % 1000) / 10; // hundredths of a second
  return QString("%1:%2:%3:%4")
      .arg(hours, 2, 10, QChar('0'))
      .arg(minutes, 2, 10, QChar('0'))
      .arg(seconds, 2, 10, QChar('0'))
      .arg(frames, 2, 10, QChar('0'));
}

QString SubtitleListDelegate::getIdAtIndex(const QModelIndex &index) {
  return index.data(SubtitleListModel::IdRole).toString();
}

QRect SubtitleListDelegate::splitButtonRect(
    const QStyleOptionViewItem &option) const {
  const int btnBox = 18;
  QRect rect = option.rect;
  int btnY = rect.top() + (rect.height() - btnBox) / 2;
  int btnX = rect.right() - 12 - 36;
  return QRect(btnX, btnY, btnBox, btnBox);
}

QRect SubtitleListDelegate::deleteButtonRect(
    const QStyleOptionViewItem &option) const {
  const int btnBox = 18;
  QRect rect = option.rect;
  int btnY = rect.top() + (rect.height() - btnBox) / 2;
  int btnX = rect.right() - 12 - 36 + 22; // scissors + gap
  return QRect(btnX, btnY, btnBox, btnBox);
}

void SubtitleListDelegate::setHoveredIndex(const QModelIndex &index,
                                           int button) {
  if (hoveredIndex_ != index || hoveredButton_ != button) {
    QModelIndex old = hoveredIndex_;
    int oldButton = hoveredButton_;
    hoveredIndex_ = index;
    hoveredButton_ = button;
    if (old.isValid()) {
      emit sizeHintChanged(old);
    }
    if (index.isValid() && index != old) {
      emit sizeHintChanged(index);
    }
    // If same index but different button, still need to repaint
    if (index.isValid() && index == old && oldButton != button) {
      emit sizeHintChanged(index);
    }
  }
}

bool SubtitleListDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                       const QStyleOptionViewItem &option,
                                       const QModelIndex &index) {
  if (!index.isValid())
    return false;

  if (event->type() == QEvent::MouseButtonRelease) {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    QRect deleteBtn = deleteButtonRect(option);
    if (deleteBtn.contains(mouseEvent->pos())) {
      QString id = getIdAtIndex(index);
      emit deleteClicked(id);
      return true;
    }
  }

  return QStyledItemDelegate::editorEvent(event, model, option, index);
}
