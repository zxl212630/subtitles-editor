#include "SubtitleListDelegate.h"

#include "SubtitleListModel.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QTextEdit>

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
  const int btnBox = 14;
  int btnY = rect.top() + (rect.height() - btnBox) / 2;
  int btnX = rect.right() - 12 - 36;

  auto drawIcon = [&](const QString &path, int x, bool hovered) {
    QColor color = hovered ? QColor("#ffffff") : QColor("#6b7280");
    QIcon icon(path);
    QPixmap pix = icon.pixmap(btnBox, btnBox);
    if (pix.isNull())
      return;
    // Recolor: replace all non-transparent pixels with target color
    // CompositionMode_SourceIn keeps alpha, replaces RGB
    QPainter tp(&pix);
    tp.setRenderHint(QPainter::Antialiasing);
    tp.setCompositionMode(QPainter::CompositionMode_SourceIn);
    tp.fillRect(pix.rect(), color);
    tp.end();
    painter->drawPixmap(x, btnY, pix);
  };

  // Split button (scissors icon) - hover brightens
  drawIcon(":/icons/scissors.svg", btnX, isHovered && hoveredButton_ == 1);

  // Delete button (x icon) - hover brightens
  drawIcon(":/icons/close.svg", btnX + 22, isHovered && hoveredButton_ == 2);

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
  const int btnBox = 14;
  QRect rect = option.rect;
  int btnY = rect.top() + (rect.height() - btnBox) / 2;
  int btnX = rect.right() - 12 - 36;
  return QRect(btnX, btnY, btnBox, btnBox);
}

QRect SubtitleListDelegate::deleteButtonRect(
    const QStyleOptionViewItem &option) const {
  const int btnBox = 14;
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
  // Click handling moved to SubtitleListPanel::eventFilter for better stability
  // when an editor is active.
  return QStyledItemDelegate::editorEvent(event, model, option, index);
}

bool SubtitleListDelegate::getActiveEditorInfo(const QModelIndex &index,
                                               int &cursorPosition,
                                               QString &text) const {
  QString id = getIdAtIndex(index);
  if (!currentEditor_ || currentEditingId_ != id) {
    return false;
  }
  auto *edit = qobject_cast<QTextEdit *>(currentEditor_);
  if (!edit)
    return false;
  cursorPosition = lastCursorPos_; // Use tracked position
  text = edit->toPlainText();
  return true;
}

class SubtitleTextEdit : public QTextEdit {
  Q_OBJECT
public:
  explicit SubtitleTextEdit(QWidget *parent = nullptr) : QTextEdit(parent) {}

signals:
  void editingFinished();

protected:
  void keyPressEvent(QKeyEvent *event) override {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
      if (event->modifiers() & Qt::AltModifier) {
        insertPlainText("\n");
      } else {
        emit editingFinished();
      }
      return;
    }
    QTextEdit::keyPressEvent(event);
  }
};

QWidget *
SubtitleListDelegate::createEditor(QWidget *parent,
                                   const QStyleOptionViewItem & /*option*/,
                                   const QModelIndex &index) const {
  auto *editor = new SubtitleTextEdit(parent);
  currentEditor_ = editor;
  currentEditingId_ = getIdAtIndex(index);
  lastCursorPos_ = 0; // Initialize

  connect(editor, &QTextEdit::cursorPositionChanged, [this, editor]() {
    int newPos = editor->textCursor().position();
    int len = editor->toPlainText().length();
    // Logic: If the cursor suddenly jumps to the very end or beginning
    // (often happens on focus loss), but we already had a position in the
    // middle, ignore the jump to preserve the split point.
    if (len > 0 && (newPos == 0 || newPos == len)) {
      if (lastCursorPos_ > 0 && lastCursorPos_ < len) {
        qDebug() << "[SplitDebug] Ignoring cursor jump to boundary:" << newPos
                 << " (staying at:" << lastCursorPos_ << ")";
        return;
      }
    }
    lastCursorPos_ = newPos;
  });

  connect(editor, &QObject::destroyed, [this]() {
    qDebug() << "[SplitDebug] Editor destroyed for id:" << currentEditingId_;
    currentEditor_ = nullptr;
    currentEditingId_.clear();
    lastCursorPos_ = -1;
  });

  editor->setStyleSheet(
      "QTextEdit { background-color: #1a1a1a; color: #d1d5db; "
      "border: 1px solid #0ea5e9; border-radius: 4px; padding: 2px 6px; "
      "font-family: Inter, sans-serif; font-size: 12px; }");
  editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto *self = const_cast<SubtitleListDelegate *>(this);
  connect(editor, &SubtitleTextEdit::editingFinished, self, [self, editor]() {
    emit self->commitData(editor);
    emit self->closeEditor(editor, QAbstractItemDelegate::SubmitModelCache);
  });

  return editor;
}

void SubtitleListDelegate::setEditorData(QWidget *editor,
                                         const QModelIndex &index) const {
  QString text = index.data(SubtitleListModel::TextRole).toString();
  auto *textEdit = static_cast<SubtitleTextEdit *>(editor);
  textEdit->setPlainText(text);

  QString id = getIdAtIndex(index);
  int len = text.length();

  // Logic: Only reset cursor position to end if we don't already have a valid
  // middle-text position for THIS specific item. This protects against
  // re-initialization during click events.
  if (currentEditingId_ != id || lastCursorPos_ <= 0 || lastCursorPos_ >= len) {
    QTextCursor cursor = textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    textEdit->setTextCursor(cursor);
    lastCursorPos_ = cursor.position();
  }
}

void SubtitleListDelegate::setModelData(QWidget *editor,
                                        QAbstractItemModel *model,
                                        const QModelIndex &index) const {
  auto *textEdit = static_cast<SubtitleTextEdit *>(editor);
  model->setData(index, textEdit->toPlainText(), Qt::EditRole);
}

void SubtitleListDelegate::updateEditorGeometry(
    QWidget *editor, const QStyleOptionViewItem &option,
    const QModelIndex & /*index*/) const {
  const QRect rect = option.rect;
  QRect timeRect(rect.left() + 12, rect.top() + 10, 100, 36);
  int textLeft = timeRect.right() + 12;
  int textRight = rect.right() - 12 - 36 - 12;
  int textWidth = qMax(50, textRight - textLeft);
  QRect editRect(textLeft, rect.top() + 8, textWidth, rect.height() - 16);
  editor->setGeometry(editRect);
}

#include "SubtitleListDelegate.moc"
