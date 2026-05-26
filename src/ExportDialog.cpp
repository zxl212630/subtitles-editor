#include "ExportDialog.h"
#include "AppMessageBox.h"
#include "SubtitleTrack.h"
#include "ThemeManager.h"
#include <QWindowKit/QWKWidgets/widgetwindowagent.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

extern "C" {
#include <libavcodec/avcodec.h>
}

ExportDialog::ExportDialog(QWidget *parent) : QDialog(parent) {
  setObjectName("ExportDialog");
  setMinimumSize(480, 560);
  resize(500, 600);

  windowAgent = new QWK::WidgetWindowAgent(this);
  windowAgent->setup(this);

  setupTitleBar();
  setupUi();
  retranslateUi();

  windowAgent->setTitleBar(titleBar);
}

ExportDialog::~ExportDialog() {}

void ExportDialog::setupTitleBar() {
  titleBar = new QFrame(this);
  titleBar->setFixedHeight(36);
  titleBar->setObjectName("TitleBar");

  auto *layout = new QHBoxLayout(titleBar);
  layout->setContentsMargins(12, 0, 12, 0);
  layout->setSpacing(0);

  layout->addStretch();

  titleLabel = new QLabel(tr("导出"), titleBar);
  titleLabel->setObjectName("ConfigTitleLeftLabel");
  layout->addWidget(titleLabel);

  layout->addStretch();
}

void ExportDialog::setSubtitleTrack(const SubtitleTrack *track) {
  track_ = track;
  if (track_ && track_->items().isEmpty()) {
    exportSubtitleChk_->setChecked(false);
    exportSubtitleChk_->setEnabled(false);
    subtitleSectionHeader_->setEnabled(false);
  } else {
    exportSubtitleChk_->setChecked(true);
    exportSubtitleChk_->setEnabled(true);
    subtitleSectionHeader_->setEnabled(true);
  }
  checkExportButtonEnabled();
}

void ExportDialog::setSourceVideo(const QString &videoPath,
                                  const QSize &videoSize, double fps,
                                  bool hasAudio, int audioSampleRate,
                                  int audioBitrate) {
  sourceVideoPath_ = videoPath;
  sourceVideoSize_ = videoSize;
  sourceFps_ = fps;
  sourceHasAudio_ = hasAudio;
  sourceAudioSampleRate_ = audioSampleRate;
  sourceAudioBitrate_ = audioBitrate;

  if (sourceVideoPath_.isEmpty() || sourceVideoSize_.isEmpty()) {
    // 无视频，禁用视频导出选项
    exportVideoChk_->setChecked(false);
    exportVideoChk_->setEnabled(false);
    exportVideoChk_->setToolTip(tr("当前未载入视频，无法导出视频"));
    videoSectionHeader_->setEnabled(false);
    videoExpanded_ = false;
  } else {
    exportVideoChk_->setChecked(true);
    exportVideoChk_->setEnabled(true);
    exportVideoChk_->setToolTip(QString());
    videoSectionHeader_->setEnabled(true);
    videoExpanded_ = true;
  }

  // 根据源视频初始化视频和音频预设列表
  initializeVideoPresets();
  initializeAudioPresets();

  updateAccordionStates();
  checkExportButtonEnabled();
  updatePathExtension();
}

void ExportDialog::initializeVideoPresets() {
  videoResolutionCombo_->clear();
  videoFpsCombo_->clear();

  // 1. 分辨率列表初始化
  struct ResItem {
    QString label;
    QSize size;
  };
  QList<ResItem> standards = {{"3840x2160 (4K)", QSize(3840, 2160)},
                              {"2560x1440 (2K)", QSize(2560, 1440)},
                              {"1920x1080 (1080p)", QSize(1920, 1080)},
                              {"1280x720 (720p)", QSize(1280, 720)},
                              {"854x480 (480p)", QSize(854, 480)}};

  bool sourceAdded = false;
  if (!sourceVideoSize_.isEmpty()) {
    // 检查源分辨率是否属于常用分辨率
    int sourceIndex = -1;
    for (int i = 0; i < standards.size(); ++i) {
      if (standards[i].size == sourceVideoSize_) {
        sourceIndex = i;
        break;
      }
    }

    if (sourceIndex != -1) {
      // 在已有选项中标记原始
      standards[sourceIndex].label = QString("%1x%2 (原始)")
                                         .arg(sourceVideoSize_.width())
                                         .arg(sourceVideoSize_.height());
    } else {
      // 动态插入非标准源分辨率到首位
      ResItem item = {QString("%1x%2 (原始)")
                          .arg(sourceVideoSize_.width())
                          .arg(sourceVideoSize_.height()),
                      sourceVideoSize_};
      standards.insert(0, item);
      sourceAdded = true;
    }
  }

  for (const auto &item : standards) {
    videoResolutionCombo_->addItem(item.label, QVariant::fromValue(item.size));
  }

  // 默认选中带“原始”标记的分辨率
  if (!sourceVideoSize_.isEmpty()) {
    if (sourceAdded) {
      videoResolutionCombo_->setCurrentIndex(0);
    } else {
      for (int i = 0; i < videoResolutionCombo_->count(); ++i) {
        if (videoResolutionCombo_->itemData(i).toSize() == sourceVideoSize_) {
          videoResolutionCombo_->setCurrentIndex(i);
          break;
        }
      }
    }
  }

  // 2. 帧率列表初始化
  struct FpsItem {
    QString label;
    double fps;
  };
  QList<FpsItem> fpsStandards = {{"60 fps", 60.0},
                                 {"50 fps", 50.0},
                                 {"30 fps", 30.0},
                                 {"25 fps", 25.0},
                                 {"24 fps", 24.0}};

  bool sourceFpsAdded = false;
  if (sourceFps_ > 0.0) {
    int sourceFpsIndex = -1;
    // 粗略匹配 (允许小误差如 29.97 与 30，但如果是 29.97
    // 这种非标准我们应该展示具体值)
    for (int i = 0; i < fpsStandards.size(); ++i) {
      if (qAbs(fpsStandards[i].fps - sourceFps_) < 0.05) {
        sourceFpsIndex = i;
        break;
      }
    }

    if (sourceFpsIndex != -1) {
      fpsStandards[sourceFpsIndex].label =
          QString("%1 fps (原始)").arg(qRound(sourceFps_));
    } else {
      // 动态插入非标准帧率
      FpsItem item = {QString("%1 fps (原始)").arg(sourceFps_, 0, 'f', 2),
                      sourceFps_};
      fpsStandards.insert(0, item);
      sourceFpsAdded = true;
    }
  }

  for (const auto &item : fpsStandards) {
    videoFpsCombo_->addItem(item.label, item.fps);
  }

  if (sourceFps_ > 0.0) {
    if (sourceFpsAdded) {
      videoFpsCombo_->setCurrentIndex(0);
    } else {
      for (int i = 0; i < videoFpsCombo_->count(); ++i) {
        if (qAbs(videoFpsCombo_->itemData(i).toDouble() - sourceFps_) < 0.05) {
          videoFpsCombo_->setCurrentIndex(i);
          break;
        }
      }
    }
  }
}

void ExportDialog::initializeAudioPresets() {
  audioBitrateCombo_->clear();
  audioSampleRateCombo_->clear();

  if (!sourceHasAudio_) {
    audioBitrateCombo_->setEnabled(false);
    audioSampleRateCombo_->setEnabled(false);
    audioBitrateCombo_->addItem(tr("无音频轨道"));
    audioSampleRateCombo_->addItem(tr("无音频轨道"));
    return;
  }

  audioBitrateCombo_->setEnabled(true);
  audioSampleRateCombo_->setEnabled(true);

  // 1. 音频码率初始化
  struct BitrateItem {
    QString label;
    int kbps;
  };
  QList<BitrateItem> standards = {{"320 kbps", 320},
                                  {"256 kbps", 256},
                                  {"192 kbps", 192},
                                  {"128 kbps", 128},
                                  {"96 kbps", 96}};

  int sourceBrKbps = sourceAudioBitrate_ / 1000;
  bool sourceBrAdded = false;
  if (sourceBrKbps > 0) {
    int index = -1;
    for (int i = 0; i < standards.size(); ++i) {
      if (standards[i].kbps == sourceBrKbps) {
        index = i;
        break;
      }
    }
    if (index != -1) {
      standards[index].label = QString("%1 kbps (原始)").arg(sourceBrKbps);
    } else {
      BitrateItem item = {QString("%1 kbps (原始)").arg(sourceBrKbps),
                          sourceBrKbps};
      standards.insert(0, item);
      sourceBrAdded = true;
    }
  }

  // 提供保留原始流选项 (0 = stream copy 或保持原样码率)
  // 如果支持流拷贝，把它列为第一项
  videoQualityCombo_->setProperty("hasAudio", true);

  for (const auto &item : standards) {
    audioBitrateCombo_->addItem(item.label, item.kbps);
  }

  // 默认选中原始码率
  if (sourceBrKbps > 0) {
    if (sourceBrAdded) {
      audioBitrateCombo_->setCurrentIndex(0);
    } else {
      for (int i = 0; i < audioBitrateCombo_->count(); ++i) {
        if (audioBitrateCombo_->itemData(i).toInt() == sourceBrKbps) {
          audioBitrateCombo_->setCurrentIndex(i);
          break;
        }
      }
    }
  }

  // 2. 音频采样率初始化
  struct SampleItem {
    QString label;
    int rate;
  };
  QList<SampleItem> sampleStandards = {{"48000 Hz", 48000},
                                       {"44100 Hz", 44100},
                                       {"32000 Hz", 32000},
                                       {"22050 Hz", 22050}};

  bool sourceSrAdded = false;
  if (sourceAudioSampleRate_ > 0) {
    int index = -1;
    for (int i = 0; i < sampleStandards.size(); ++i) {
      if (sampleStandards[i].rate == sourceAudioSampleRate_) {
        index = i;
        break;
      }
    }
    if (index != -1) {
      sampleStandards[index].label =
          QString("%1 Hz (原始)").arg(sourceAudioSampleRate_);
    } else {
      SampleItem item = {QString("%1 Hz (原始)").arg(sourceAudioSampleRate_),
                         sourceAudioSampleRate_};
      sampleStandards.insert(0, item);
      sourceSrAdded = true;
    }
  }

  for (const auto &item : sampleStandards) {
    audioSampleRateCombo_->addItem(item.label, item.rate);
  }

  if (sourceAudioSampleRate_ > 0) {
    if (sourceSrAdded) {
      audioSampleRateCombo_->setCurrentIndex(0);
    } else {
      for (int i = 0; i < audioSampleRateCombo_->count(); ++i) {
        if (audioSampleRateCombo_->itemData(i).toInt() ==
            sourceAudioSampleRate_) {
          audioSampleRateCombo_->setCurrentIndex(i);
          break;
        }
      }
    }
  }
}

void ExportDialog::setupUi() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  mainLayout->addWidget(titleBar);

  QWidget *contentWidget = new QWidget(this);
  contentWidget->setObjectName("ConfigContentWidget");
  QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
  contentLayout->setContentsMargins(30, 20, 30, 20);
  contentLayout->setSpacing(15);

  // --- 1. 导出视频折叠区域 ---
  exportVideoChk_ = new QCheckBox(contentWidget);
  exportVideoChk_->setStyleSheet("font-weight: bold; font-size: 13px;");
  connect(exportVideoChk_, &QCheckBox::stateChanged, this,
          &ExportDialog::onVideoCheckChanged);

  videoSectionHeader_ = new QPushButton(contentWidget);
  videoSectionHeader_->setFlat(true);
  videoSectionHeader_->setStyleSheet(
      "QPushButton {"
      "  text-align: left;"
      "  font-weight: bold;"
      "  font-size: 13px;"
      "  padding: 6px 8px;"
      "  border: none;"
      "  color: palette(text);"
      "}"
      "QPushButton:hover {"
      "  background-color: rgba(255, 255, 255, 0.05);"
      "  border-radius: 4px;"
      "}");
  connect(videoSectionHeader_, &QPushButton::clicked, this, [this]() {
    if (exportVideoChk_->isEnabled()) {
      videoExpanded_ = !videoExpanded_;
      updateAccordionStates();
    }
  });

  QHBoxLayout *videoHeaderLayout = new QHBoxLayout();
  videoHeaderLayout->setContentsMargins(0, 0, 0, 0);
  videoHeaderLayout->addWidget(exportVideoChk_);
  videoHeaderLayout->addWidget(videoSectionHeader_, 1);
  contentLayout->addLayout(videoHeaderLayout);

  videoSectionFrame_ = new QFrame(contentWidget);
  videoSectionFrame_->setObjectName("ExportSectionFrame");

  QFormLayout *videoForm = new QFormLayout(videoSectionFrame_);
  videoForm->setContentsMargins(15, 15, 15, 15);
  videoForm->setSpacing(10);

  videoFormatCombo_ = new QComboBox(videoSectionFrame_);
  videoFormatCombo_->addItem("MP4", "mp4");
  videoFormatCombo_->addItem("MOV", "mov");
  connect(videoFormatCombo_,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ExportDialog::onFormatChanged);

  QLabel *l1 = new QLabel(tr("视频格式"), videoSectionFrame_);
  l1->setObjectName("ConfigFieldLabel");
  videoForm->addRow(l1, videoFormatCombo_);

  videoCodecCombo_ = new QComboBox(videoSectionFrame_);
  // 探测 VideoToolbox 支持
  bool hasH264Vt = avcodec_find_encoder_by_name("h264_videotoolbox") != nullptr;
  bool hasHevcVt = avcodec_find_encoder_by_name("hevc_videotoolbox") != nullptr;

  if (hasH264Vt) {
    videoCodecCombo_->addItem(tr("H.264 (硬件加速)"), "h264_videotoolbox");
  }
  videoCodecCombo_->addItem(tr("H.264 (CPU)"), "libx264");

  if (hasHevcVt) {
    videoCodecCombo_->addItem(tr("H.265 / HEVC (硬件加速)"),
                              "hevc_videotoolbox");
  }
  videoCodecCombo_->addItem(tr("H.265 / HEVC (CPU)"), "libx265");

  QLabel *l2 = new QLabel(tr("视频编码"), videoSectionFrame_);
  l2->setObjectName("ConfigFieldLabel");
  videoForm->addRow(l2, videoCodecCombo_);

  videoResolutionCombo_ = new QComboBox(videoSectionFrame_);
  QLabel *l3 = new QLabel(tr("分辨率"), videoSectionFrame_);
  l3->setObjectName("ConfigFieldLabel");
  videoForm->addRow(l3, videoResolutionCombo_);

  videoFpsCombo_ = new QComboBox(videoSectionFrame_);
  QLabel *l4 = new QLabel(tr("帧率"), videoSectionFrame_);
  l4->setObjectName("ConfigFieldLabel");
  videoForm->addRow(l4, videoFpsCombo_);

  // 画质/码率控制
  videoQualityCombo_ = new QComboBox(videoSectionFrame_);
  videoQualityCombo_->addItem(tr("高质量"), VideoExportConfig::QualityHigh);
  videoQualityCombo_->addItem(tr("中等质量 (推荐)"),
                              VideoExportConfig::QualityMedium);
  videoQualityCombo_->addItem(tr("较低质量"), VideoExportConfig::QualityLow);
  videoQualityCombo_->addItem(tr("自定义码率"),
                              VideoExportConfig::QualityCustomBitrate);
  videoQualityCombo_->setCurrentIndex(1); // 默认中等
  connect(videoQualityCombo_,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ExportDialog::onQualityModeChanged);

  QLabel *l5 = new QLabel(tr("视频质量"), videoSectionFrame_);
  l5->setObjectName("ConfigFieldLabel");
  videoForm->addRow(l5, videoQualityCombo_);

  // 自定义码率输入子面板
  customBitrateFrame_ = new QFrame(videoSectionFrame_);
  QHBoxLayout *bitrateLayout = new QHBoxLayout(customBitrateFrame_);
  bitrateLayout->setContentsMargins(0, 0, 0, 0);
  customBitrateEdit_ = new QLineEdit("8000", customBitrateFrame_);
  customBitrateEdit_->setValidator(new QIntValidator(100, 100000, this));
  customBitrateEdit_->setFixedWidth(80);
  QLabel *kbpsLabel = new QLabel("kbps", customBitrateFrame_);
  kbpsLabel->setObjectName("ConfigFieldLabel");
  bitrateLayout->addWidget(customBitrateEdit_);
  bitrateLayout->addWidget(kbpsLabel);
  bitrateLayout->addStretch();
  customBitrateFrame_->setVisible(false);
  videoForm->addRow(QString(), customBitrateFrame_);

  // 音频处理
  audioBitrateCombo_ = new QComboBox(videoSectionFrame_);
  QLabel *l6 = new QLabel(tr("音频码率"), videoSectionFrame_);
  l6->setObjectName("ConfigFieldLabel");
  videoForm->addRow(l6, audioBitrateCombo_);

  audioSampleRateCombo_ = new QComboBox(videoSectionFrame_);
  QLabel *l7 = new QLabel(tr("音频采样率"), videoSectionFrame_);
  l7->setObjectName("ConfigFieldLabel");
  videoForm->addRow(l7, audioSampleRateCombo_);

  contentLayout->addWidget(videoSectionFrame_);

  // --- 2. 导出字幕折叠区域 ---
  exportSubtitleChk_ = new QCheckBox(contentWidget);
  exportSubtitleChk_->setStyleSheet("font-weight: bold; font-size: 13px;");
  connect(exportSubtitleChk_, &QCheckBox::stateChanged, this,
          &ExportDialog::onSubtitleCheckChanged);

  subtitleSectionHeader_ = new QPushButton(contentWidget);
  subtitleSectionHeader_->setFlat(true);
  subtitleSectionHeader_->setStyleSheet(
      "QPushButton {"
      "  text-align: left;"
      "  font-weight: bold;"
      "  font-size: 13px;"
      "  padding: 6px 8px;"
      "  border: none;"
      "  color: palette(text);"
      "}"
      "QPushButton:hover {"
      "  background-color: rgba(255, 255, 255, 0.05);"
      "  border-radius: 4px;"
      "}");
  connect(subtitleSectionHeader_, &QPushButton::clicked, this, [this]() {
    if (exportSubtitleChk_->isEnabled()) {
      subtitleExpanded_ = !subtitleExpanded_;
      updateAccordionStates();
    }
  });

  QHBoxLayout *subHeaderLayout = new QHBoxLayout();
  subHeaderLayout->setContentsMargins(0, 0, 0, 0);
  subHeaderLayout->addWidget(exportSubtitleChk_);
  subHeaderLayout->addWidget(subtitleSectionHeader_, 1);
  contentLayout->addLayout(subHeaderLayout);

  subtitleSectionFrame_ = new QFrame(contentWidget);
  subtitleSectionFrame_->setObjectName("ExportSectionFrame");

  QFormLayout *subForm = new QFormLayout(subtitleSectionFrame_);
  subForm->setContentsMargins(15, 15, 15, 15);
  subForm->setSpacing(10);

  subtitleFormatCombo_ = new QComboBox(subtitleSectionFrame_);
  subtitleFormatCombo_->addItem("SRT (*.srt)", "srt");
  subtitleFormatCombo_->addItem("TXT (*.txt)", "txt");
  subtitleFormatCombo_->addItem("Premiere Pro XML (*.xml)", "xml");
  subtitleFormatCombo_->addItem("Final Cut Pro XML (*.fcpxml)", "fcpxml");
  connect(subtitleFormatCombo_,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ExportDialog::updatePathExtension);

  QLabel *l8 = new QLabel(tr("字幕格式"), subtitleSectionFrame_);
  l8->setObjectName("ConfigFieldLabel");
  subForm->addRow(l8, subtitleFormatCombo_);

  contentLayout->addWidget(subtitleSectionFrame_);

  // --- 3. 输出路径选择区 ---
  QFrame *pathFrame = new QFrame(contentWidget);
  QVBoxLayout *pathLayout = new QVBoxLayout(pathFrame);
  pathLayout->setContentsMargins(0, 5, 0, 0);
  pathLayout->setSpacing(5);

  QLabel *pathTitle = new QLabel(tr("输出路径"), pathFrame);
  pathTitle->setObjectName("ConfigFieldLabel");
  pathLayout->addWidget(pathTitle);

  QHBoxLayout *pathLineLayout = new QHBoxLayout();
  pathEdit_ = new QLineEdit(pathFrame);
  connect(pathEdit_, &QLineEdit::textChanged, this,
          &ExportDialog::checkExportButtonEnabled);

  browseBtn_ = new QPushButton(tr("浏览..."), pathFrame);
  connect(browseBtn_, &QPushButton::clicked, this,
          &ExportDialog::onBrowseClicked);
  pathLineLayout->addWidget(pathEdit_);
  pathLineLayout->addWidget(browseBtn_);
  pathLayout->addLayout(pathLineLayout);

  pathHintLabel_ = new QLabel(pathFrame);
  pathHintLabel_->setStyleSheet(
      "color: palette(placeholder-text); font-size: 11px;");
  pathLayout->addWidget(pathHintLabel_);
  contentLayout->addWidget(pathFrame);

  mainLayout->addWidget(contentWidget, 1);

  // --- 4. 底部确定取消按钮 ---
  QWidget *footer = new QWidget(this);
  footer->setObjectName("ConfigFooter");
  footer->setFixedHeight(60);
  QHBoxLayout *footerLayout = new QHBoxLayout(footer);
  footerLayout->setContentsMargins(20, 0, 20, 0);

  footerLayout->addStretch();

  cancelBtn_ = new QPushButton(tr("取消"), footer);
  cancelBtn_->setObjectName("ConfigCancelButton");
  cancelBtn_->setFixedWidth(100);
  connect(cancelBtn_, &QPushButton::clicked, this, &QDialog::reject);

  exportBtn_ = new QPushButton(tr("导出"), footer);
  exportBtn_->setObjectName("ConfigOkButton");
  exportBtn_->setFixedWidth(100);
  exportBtn_->setDefault(true);
  connect(exportBtn_, &QPushButton::clicked, this,
          &ExportDialog::onExportClicked);

  footerLayout->addWidget(cancelBtn_);
  footerLayout->addWidget(exportBtn_);

  mainLayout->addWidget(footer);

  updateAccordionStates();
}

void ExportDialog::retranslateUi() {
  setWindowTitle(tr("导出"));
  videoSectionHeader_->setText(tr("视频设置"));
  subtitleSectionHeader_->setText(tr("字幕设置"));
  exportBtn_->setText(tr("导出"));
  checkExportButtonEnabled();
}

void ExportDialog::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QDialog::changeEvent(event);
}

void ExportDialog::onVideoCheckChanged(int state) {
  bool checked = (state == Qt::Checked);
  videoSectionFrame_->setEnabled(checked);
  if (checked) {
    videoExpanded_ = true;
  } else {
    videoExpanded_ = false;
  }
  updateAccordionStates();
  checkExportButtonEnabled();
  updatePathExtension();
}

void ExportDialog::onSubtitleCheckChanged(int state) {
  bool checked = (state == Qt::Checked);
  subtitleSectionFrame_->setEnabled(checked);
  if (checked) {
    subtitleExpanded_ = true;
  } else {
    subtitleExpanded_ = false;
  }
  updateAccordionStates();
  checkExportButtonEnabled();
  updatePathExtension();
}

void ExportDialog::updateAccordionStates() {
  // 根据展开状态设置折叠面板的隐显
  videoSectionFrame_->setVisible(videoExpanded_ &&
                                 exportVideoChk_->isChecked());
  subtitleSectionFrame_->setVisible(subtitleExpanded_ &&
                                    exportSubtitleChk_->isChecked());

  // 修改 Header 上的文字指示符
  QString videoPrefix =
      (videoExpanded_ && exportVideoChk_->isChecked()) ? "▼ " : "▶ ";
  videoSectionHeader_->setText(videoPrefix + tr("视频设置"));

  QString subtitlePrefix =
      (subtitleExpanded_ && exportSubtitleChk_->isChecked()) ? "▼ " : "▶ ";
  subtitleSectionHeader_->setText(subtitlePrefix + tr("字幕设置"));

  adjustSize();
}

void ExportDialog::onQualityModeChanged(int index) {
  int mode = videoQualityCombo_->itemData(index).toInt();
  customBitrateFrame_->setVisible(mode ==
                                  VideoExportConfig::QualityCustomBitrate);
  adjustSize();
}

void ExportDialog::onFormatChanged(int index) {
  Q_UNUSED(index)
  updatePathExtension();
}

void ExportDialog::updatePathExtension() {
  QString path = pathEdit_->text().trimmed();
  if (path.isEmpty())
    return;

  QFileInfo info(path);
  QString base = info.absolutePath() + "/" + info.completeBaseName();

  if (exportVideoChk_->isChecked()) {
    // 视频主导扩展名
    QString ext = videoFormatCombo_->currentData().toString();
    pathEdit_->setText(base + "." + ext);
    pathHintLabel_->setText(
        exportSubtitleChk_->isChecked()
            ? tr("提示：字幕文件将自动保存至同一目录下（文件名与视频相同）")
            : QString());
  } else if (exportSubtitleChk_->isChecked()) {
    // 字幕主导扩展名
    QString ext = subtitleFormatCombo_->currentData().toString();
    pathEdit_->setText(base + "." + ext);
    pathHintLabel_->setText(QString());
  }
}

void ExportDialog::onBrowseClicked() {
  QString filter;
  QString defaultExt;

  if (exportVideoChk_->isChecked()) {
    QString ext = videoFormatCombo_->currentData().toString();
    if (ext == "mov") {
      filter = tr("QuickTime 视频 (*.mov)");
      defaultExt = "mov";
    } else {
      filter = tr("MPEG-4 视频 (*.mp4)");
      defaultExt = "mp4";
    }
  } else if (exportSubtitleChk_->isChecked()) {
    QString format = subtitleFormatCombo_->currentData().toString();
    if (format == "txt") {
      filter = tr("TXT 文本 (*.txt)");
      defaultExt = "txt";
    } else if (format == "xml") {
      filter = tr("Premiere XML (*.xml)");
      defaultExt = "xml";
    } else if (format == "fcpxml") {
      filter = tr("Final Cut Pro XML (*.fcpxml)");
      defaultExt = "fcpxml";
    } else {
      filter = tr("SRT 字幕 (*.srt)");
      defaultExt = "srt";
    }
  } else {
    return;
  }

  QString path = QFileDialog::getSaveFileName(this, tr("选择保存路径"),
                                              pathEdit_->text(), filter);
  if (!path.isEmpty()) {
    QFileInfo info(path);
    if (info.suffix().isEmpty()) {
      path += "." + defaultExt;
    }
    pathEdit_->setText(path);
  }
}

void ExportDialog::checkExportButtonEnabled() {
  bool anyChecked =
      (exportVideoChk_->isChecked() || exportSubtitleChk_->isChecked());
  bool pathOk = !pathEdit_->text().trimmed().isEmpty();
  exportBtn_->setEnabled(anyChecked && pathOk);
}

void ExportDialog::onExportClicked() {
  QString path = pathEdit_->text().trimmed();
  if (path.isEmpty()) {
    AppMessageBox::warning(this, tr("导出"), tr("保存路径不能为空。"));
    return;
  }

  QFileInfo info(path);
  QDir dir = info.dir();
  if (!dir.exists()) {
    AppMessageBox::warning(this, tr("导出"),
                           tr("保存目录不存在，请选择合法的路径。"));
    return;
  }

  accept();
}

bool ExportDialog::isVideoSelected() const {
  return exportVideoChk_->isChecked();
}

bool ExportDialog::isSubtitleSelected() const {
  return exportSubtitleChk_->isChecked();
}

QString ExportDialog::subtitleFormat() const {
  return subtitleFormatCombo_->currentData().toString();
}

QString ExportDialog::outputPath() const { return pathEdit_->text().trimmed(); }

VideoExportConfig ExportDialog::videoConfig() const {
  VideoExportConfig config;
  config.inputPath = sourceVideoPath_;
  config.outputPath = pathEdit_->text().trimmed();
  config.videoCodec = videoCodecCombo_->currentData().toString();

  int qIdx = videoQualityCombo_->currentIndex();
  config.qualityMode = static_cast<VideoExportConfig::QualityMode>(
      videoQualityCombo_->itemData(qIdx).toInt());
  config.customBitrateKbps = customBitrateEdit_->text().toInt();

  QSize targetSize = videoResolutionCombo_->currentData().toSize();
  // 若选择的是与原始一致，即 standards 里的 standards[sourceIndex]，它的值就是
  // sourceVideoSize_。 为了让 VideoExporter 了解是否有缩放，如果是
  // sourceVideoSize_，我们也可以传 0（Exporter 识别 0 为保持原始）。
  if (targetSize == sourceVideoSize_) {
    config.outputWidth = 0;
    config.outputHeight = 0;
  } else {
    config.outputWidth = targetSize.width();
    config.outputHeight = targetSize.height();
  }

  double targetFps = videoFpsCombo_->currentData().toDouble();
  if (qAbs(targetFps - sourceFps_) < 0.05) {
    config.outputFps = 0.0;
  } else {
    config.outputFps = targetFps;
  }

  // 音频配置
  if (sourceHasAudio_) {
    config.exportAudio = true;

    // 如果是原始，下拉框传的值对应的 kbps 和 rate 就是 source 属性。
    // 我们同样把它们映射为 0 表示保持与源视频一致。
    int audioBrKbps = audioBitrateCombo_->currentData().toInt();
    if (audioBrKbps == sourceAudioBitrate_ / 1000) {
      config.audioBitrateKbps = 0;
    } else {
      config.audioBitrateKbps = audioBrKbps;
    }

    int audioSr = audioSampleRateCombo_->currentData().toInt();
    if (audioSr == sourceAudioSampleRate_) {
      config.audioSampleRate = 0;
    } else {
      config.audioSampleRate = audioSr;
    }
  } else {
    config.exportAudio = false;
  }

  return config;
}
