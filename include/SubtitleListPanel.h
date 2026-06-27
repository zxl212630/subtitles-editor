#pragma once

#include "SubtitleItem.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QGridLayout>
#include <QIcon>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QWidget>

class SubtitleTrack;
class SubtitleListModel;
class SubtitleListDelegate;
class QListView;
class QLabel;
class SubtitleActionOverlay;

class ColorButton : public QPushButton {
  Q_OBJECT
public:
  explicit ColorButton(QWidget *parent = nullptr) : QPushButton(parent) {
    setCursor(Qt::PointingHandCursor);
    connect(this, &QPushButton::clicked, this, &ColorButton::chooseColor);
  }

  void setColor(const QColor &color) {
    color_ = color;
    updateButton();
  }

  QColor color() const { return color_; }

signals:
  void colorChanged(const QColor &color);

private:
  void chooseColor() {
    QColor c = QColorDialog::getColor(color_, this, tr("Select Color"));
    if (c.isValid()) {
      setColor(c);
      emit colorChanged(c);
    }
  }

  void updateButton() {
    setStyleSheet(
        QString("QPushButton { background-color: %1; border: 1px solid #555; "
                "border-radius: 4px; min-height: 22px; max-height: 22px; }"
                "QPushButton:disabled { background-color: rgba(128, 128, 128, "
                "0.2); border: 1px solid #444; }")
            .arg(color_.name()));
  }

  QColor color_ = Qt::white;
};

class SubtitleListPanel : public QWidget {
  Q_OBJECT

public:
  explicit SubtitleListPanel(QWidget *parent = nullptr);

  void setTrack(SubtitleTrack *track);
  void setVideoFps(double fps);
  void setTotalDuration(qint64 ms);
  void updateSpeakerColumnVisibility();

signals:
  void itemSelected(const QString &id);
  void itemDeleteRequested(const QString &id);
  void itemSeekRequested(const QString &id, qint64 startMs);
  void itemDoubleClicked(const QString &id, qint64 startMs);

private:
  void setupUi();
  void retranslateUi();
  void onItemClicked(const QModelIndex &index);
  void onItemDoubleClicked(const QModelIndex &index);
  void onTrackItemSelected(const QString &id);
  bool eventFilter(QObject *watched, QEvent *event) override;
  void leaveEvent(QEvent *event) override;

  SubtitleTrack *track_ = nullptr;
  SubtitleListModel *model_ = nullptr;
  SubtitleListDelegate *delegate_ = nullptr;
  QListView *listView_ = nullptr;
  QLineEdit *searchEdit_ = nullptr;
  QPushButton *searchClearBtn_ = nullptr;
  SubtitleActionOverlay *actionOverlay_ = nullptr;

  QPushButton *tabSubtitle_ = nullptr;
  QPushButton *tabPreset_ = nullptr;
  QPushButton *tabBubble_ = nullptr;
  QPushButton *tabCustom_ = nullptr;
  QPushButton *tabAnimation_ = nullptr;
  QLabel *headerTime_ = nullptr;
  QLabel *headerSpeaker_ = nullptr;
  QLabel *headerText_ = nullptr;
  QLabel *headerAction_ = nullptr;

  void showSpeakerMenu(const QModelIndex &index, const QPoint &globalPos);

  double videoFps_ = 25.0;
  qint64 totalDurationMs_ = 0;

  QStackedWidget *stackedWidget_ = nullptr;
  QString currentSelectedId_;
  bool isUpdatingControls_ = false;
  QWidget *customStyleContainer_ = nullptr;

  // Custom styling controls
  QCheckBox *fillEnableCheck_ = nullptr;
  class QFormLayout *fillForm_ = nullptr;
  class QFormLayout *bgForm_ = nullptr;
  QComboBox *fillTypeCombo_ = nullptr;
  ColorButton *fillColorBtn_ = nullptr;
  ColorButton *fillColor2Btn_ = nullptr;
  QSlider *fillAngleSlider_ = nullptr;
  QSpinBox *fillAngleSpin_ = nullptr;
  QSlider *textOpacitySlider_ = nullptr;

  QCheckBox *strokeEnableCheck_ = nullptr;
  QSpinBox *strokeWidthSpin_ = nullptr;
  ColorButton *strokeColorBtn_ = nullptr;
  QSlider *strokeOpacitySlider_ = nullptr;

  QCheckBox *shadowEnableCheck_ = nullptr;
  QSpinBox *shadowOffsetXSpin_ = nullptr;
  QSpinBox *shadowOffsetYSpin_ = nullptr;
  QSlider *shadowBlurSlider_ = nullptr;
  ColorButton *shadowColorBtn_ = nullptr;
  QSlider *shadowOpacitySlider_ = nullptr;

  QCheckBox *bgEnableCheck_ = nullptr;
  ColorButton *bgColorBtn_ = nullptr;
  QSlider *bgOpacitySlider_ = nullptr;
  QSlider *bgRoundnessSlider_ = nullptr;
  QSpinBox *bgPaddingXSpin_ = nullptr;
  QSpinBox *bgPaddingYSpin_ = nullptr;
  QSpinBox *bgOffsetXSpin_ = nullptr;
  QSpinBox *bgOffsetYSpin_ = nullptr;

  // 气泡控件成员
  QCheckBox *bubbleEnableCheck_ = nullptr;
  QLineEdit *bubbleImagePathEdit_ = nullptr;
  QPushButton *bubbleImageBrowse_ = nullptr;
  QSpinBox *bubblePaddingLeftSpin_ = nullptr;
  QSpinBox *bubblePaddingRightSpin_ = nullptr;
  QSpinBox *bubblePaddingTopSpin_ = nullptr;
  QSpinBox *bubblePaddingBottomSpin_ = nullptr;
  QSpinBox *bubbleSliceLeftSpin_ = nullptr;
  QSpinBox *bubbleSliceRightSpin_ = nullptr;
  QSpinBox *bubbleSliceTopSpin_ = nullptr;
  QSpinBox *bubbleSliceBottomSpin_ = nullptr;

  QComboBox *presetTypeCombo_ = nullptr;
  QListWidget *presetListWidget_ = nullptr;
  QListWidget *bubbleListWidget_ = nullptr;
  QPushButton *savePresetBtn_ = nullptr;

  QIcon createPresetIcon(const SubtitleItem &style, const QSize &size);
  QString generateSvgForPreset(const SubtitleItem &style);
  QString writeSvgPresetFile(const QString &name, const SubtitleItem &style);
  void populatePresets();
  void populateBubbles();
  QWidget *createCustomStylePanel();
  QWidget *createPresetStylePanel();
  QWidget *createBubbleStylePanel();
  void loadStyleFromItem(const SubtitleItem &item);
  void updateFillTypeFields();
  void applyCustomStyleToActiveItem();
  void loadCustomPresets();
  void showPresetContextMenu(int idx, const QPoint &pos);
  void addPresetCard(const QString &name, const SubtitleItem &style,
                     bool isCustom = false, int customIndex = -1);
  void addBubbleCard(const QString &name, const QString &imagePath, int padLeft,
                     int padRight, int padTop, int padBottom, int sliceLeft,
                     int sliceRight, int sliceTop, int sliceBottom);
};
