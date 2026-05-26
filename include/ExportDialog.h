#pragma once

#include "VideoExporter.h"
#include <QDialog>
#include <QSize>
#include <QString>

namespace QWK {
class WidgetWindowAgent;
}
class QFrame;
class QLabel;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class SubtitleTrack;

class ExportDialog : public QDialog {
  Q_OBJECT

public:
  explicit ExportDialog(QWidget *parent = nullptr);
  ~ExportDialog() override;

  void setSubtitleTrack(const SubtitleTrack *track);
  void setSourceVideo(const QString &videoPath, const QSize &videoSize,
                      double fps, bool hasAudio, int audioSampleRate,
                      int audioBitrate);

  bool isVideoSelected() const;
  bool isSubtitleSelected() const;

  VideoExportConfig videoConfig() const;
  QString subtitleFormat() const; // "srt" | "txt" | "xml" | "fcpxml"
  QString outputPath() const;     // 主要输出路径 (视频或字幕)

protected:
  void changeEvent(QEvent *event) override;

private slots:
  void onVideoCheckChanged(int state);
  void onSubtitleCheckChanged(int state);
  void onBrowseClicked();
  void onQualityModeChanged(int index);
  void onFormatChanged(int index);
  void onExportClicked();

private:
  void setupUi();
  void retranslateUi();
  void setupTitleBar();
  void updateAccordionStates();
  void checkExportButtonEnabled();
  void updatePathExtension();
  void initializeVideoPresets();
  void initializeAudioPresets();

  const SubtitleTrack *track_ = nullptr;

  // 源视频属性
  QString sourceVideoPath_;
  QSize sourceVideoSize_;
  double sourceFps_ = 0.0;
  bool sourceHasAudio_ = false;
  int sourceAudioSampleRate_ = 0;
  int sourceAudioBitrate_ = 0;

  // UI 元素
  QCheckBox *exportVideoChk_ = nullptr;
  QFrame *videoSectionFrame_ = nullptr;
  QPushButton *videoSectionHeader_ = nullptr;
  bool videoExpanded_ = true;

  QComboBox *videoFormatCombo_ = nullptr;
  QComboBox *videoCodecCombo_ = nullptr;
  QComboBox *videoResolutionCombo_ = nullptr;
  QComboBox *videoFpsCombo_ = nullptr;
  QComboBox *videoQualityCombo_ = nullptr;

  // 自定义码率输入区域
  QFrame *customBitrateFrame_ = nullptr;
  QLineEdit *customBitrateEdit_ = nullptr;

  QCheckBox *exportSubtitleChk_ = nullptr;
  QFrame *subtitleSectionFrame_ = nullptr;
  QPushButton *subtitleSectionHeader_ = nullptr;
  bool subtitleExpanded_ = false;

  QComboBox *subtitleFormatCombo_ = nullptr;

  // 音频设置 (简化下拉框)
  QComboBox *audioBitrateCombo_ = nullptr;
  QComboBox *audioSampleRateCombo_ = nullptr;

  // 路径选择
  QLineEdit *pathEdit_ = nullptr;
  QPushButton *browseBtn_ = nullptr;
  QLabel *pathHintLabel_ = nullptr;

  QPushButton *exportBtn_ = nullptr;
  QPushButton *cancelBtn_ = nullptr;

  QWK::WidgetWindowAgent *windowAgent = nullptr;
  QFrame *titleBar = nullptr;
  QLabel *titleLabel = nullptr;
};
