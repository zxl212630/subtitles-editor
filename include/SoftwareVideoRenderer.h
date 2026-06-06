#pragma once

#include "FFmpegDecoder.h"
#include <QFocusEvent>
#include <QHash>
#include <QImage>
#include <QKeyEvent>
#include <QMargins>
#include <QMutex>
#include <QRectF>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QWidget>

class SubtitleLineEdit : public QTextEdit {
  Q_OBJECT
public:
  explicit SubtitleLineEdit(QWidget *parent = nullptr) : QTextEdit(parent) {
    setStyleSheet("background: transparent; border: none;");
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setAcceptRichText(false);
  }

  QString text() const { return toPlainText(); }
  void setText(const QString &t) { setPlainText(t); }

  int cursorPosition() const { return textCursor().position(); }
  void setCursorPosition(int pos) {
    QTextCursor c = textCursor();
    c.setPosition(pos);
    setTextCursor(c);
  }

  bool hasSelectedText() const { return textCursor().hasSelection(); }
  int selectionStart() const { return textCursor().selectionStart(); }
  int selectionLength() const { return textCursor().selectedText().length(); }

  void deselect() {
    QTextCursor c = textCursor();
    c.clearSelection();
    setTextCursor(c);
  }

  void setSelection(int start, int len) {
    QTextCursor c = textCursor();
    c.setPosition(start);
    // Determine the anchor mathematically. If len < 0, it selects backwards.
    c.setPosition(start + len, QTextCursor::KeepAnchor);
    setTextCursor(c);
  }

protected:
  void paintEvent(QPaintEvent *event) override {
    // Do nothing to prevent drawing native text, selection, and caret cursor
    Q_UNUSED(event);
  }

  void mousePressEvent(QMouseEvent *event) override {
    QTextEdit::mousePressEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    QTextEdit::mouseReleaseEvent(event);
  }

  void keyPressEvent(QKeyEvent *event) override {
    if (event->key() == Qt::Key_Escape) {
      emit escPressed();
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
      if (event->modifiers() & Qt::ShiftModifier) {
        QTextEdit::keyPressEvent(event); // Insert newline
      } else {
        emit returnPressed();
      }
      event->accept();
      return;
    }
    QTextEdit::keyPressEvent(event);
  }

  void focusOutEvent(QFocusEvent *event) override {
    emit focusLost();
    QTextEdit::focusOutEvent(event);
  }

signals:
  void escPressed();
  void focusLost();
  void returnPressed();
};

class SoftwareVideoRenderer : public QWidget {
  Q_OBJECT

public:
  explicit SoftwareVideoRenderer(QWidget *parent = nullptr);

  void renderFrame(const DecodedVideoFrame &frame);
  void clear();
  void setSubtitleText(const QString &text);
  void setSubtitleFont(const QFont &font);
  void setSubtitleBg(const QString &imagePath, bool is9Patch,
                     const QMargins &margins);
  void clearSubtitleBg();

  QSize videoSize() const { return videoSize_; }
  void setVideoSize(const QSize &size);

  // === 字幕对齐与排版包围框设置 ===
  void setSubtitleAlignment(int alignment);
  void setSubtitleNormalizedRect(const QRectF &rect);
  void setSubtitleRotation(double rotation);
  void setShowEditFrame(bool show);
  QRectF subtitleNormalizedRect() const { return subtitleNormalizedRect_; }
  double subtitleRotation() const { return subtitleRotation_; }
  QTransform getSubtitleTransform() const;

signals:
  void subtitleRectChanged(const QRectF &rect);
  void subtitleRotationChanged(double rotation);
  void subtitleClicked();
  void subtitleDoubleClicked();
  void subtitleTextEdited(const QString &text);
  void subtitleFontSizeChanged(int size);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int width) const override;

  // 鼠标交互重写
  void mousePressEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private:
  QByteArray currentRgbaData_;
  int currentWidth_ = 0;
  int currentHeight_ = 0;
  bool hasFrame_ = false;
  mutable QMutex imageMutex_;
  QSize videoSize_;

  QString subtitleText_;
  QFont subtitleFont_;
  mutable QMutex subtitleMutex_;

  QString bgImagePath_;
  bool bgIs9Patch_ = false;
  QMargins bgMargins_;
  QHash<QString, QImage> bgCache_;
  QMutex bgMutex_;

  // 字幕排版与拖拽状态变量
  int subtitleAlignment_ = 0x84; // Qt::AlignHCenter | Qt::AlignVCenter
  QRectF subtitleNormalizedRect_{0.1, 0.75, 0.8, 0.2};
  double subtitleRotation_ = 0.0;
  bool showEditFrame_ = true;

  enum DragMode {
    DragNone,
    DragMove,
    DragResizeTL,
    DragResizeTM,
    DragResizeTR,
    DragResizeML,
    DragResizeMR,
    DragResizeBL,
    DragResizeBM,
    DragResizeBR,
    DragRotate
  };
  DragMode dragMode_ = DragNone;
  QPoint dragStartPos_;
  QRectF dragStartNormalizedRect_;
  QTransform dragStartTransform_;

  QRect getTargetRect() const;
  QRect getSubtitlePixelRect() const;
  DragMode hitTest(const QPoint &pos) const;

  void drawNinePatch(QPainter &painter, const QImage &src, const QRect &target,
                     const QMargins &margins);

  double getNormalizedFontHeight() const;

  void commitEditing();
  void cancelEditing();
  void updateEditorGeometry();
  int cursorPosFromLocalPoint(const QPoint &localPos) const;

  SubtitleLineEdit *editor_ = nullptr;
  bool isEditing_ = false;
  QTimer cursorTimer_;
  bool cursorVisible_ = true;
  int editClickAnchor_ = 0;

  int dragStartFontSize_ = 24;
  double dragStartFontRefHeight_ = 0.05;
  int currentDragFontSize_ = 24;
};
