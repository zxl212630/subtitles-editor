#include "ExportDialog.h"
#include "AppMessageBox.h"
#include "SubtitleTrack.h"
#include "ThemeManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QStyle>
#include <QVBoxLayout>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <future>

static bool isEncoderAvailable(const char *name) {
  // MediaFoundation encoders (h264_mf, hevc_mf) fail to open in the main thread
  // because Qt initializes COM in STA mode on the main GUI thread.
  // Running this check in std::async launches a worker thread (which has no COM
  // or is MTA), allowing MediaFoundation to initialize correctly.
  auto future = std::async(std::launch::async, [name]() -> bool {
    const AVCodec *encoder = avcodec_find_encoder_by_name(name);
    if (!encoder) {
      return false;
    }

    AVCodecContext *ctx = avcodec_alloc_context3(encoder);
    if (!ctx) {
      return false;
    }

    ctx->width = 64;
    ctx->height = 64;
    ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    ctx->time_base = {1, 25};
    ctx->framerate = {25, 1};
    ctx->bit_rate = 1000000;

    int ret = avcodec_open2(ctx, encoder, nullptr);
    avcodec_free_context(&ctx);

    return ret >= 0;
  });

  try {
    return future.get();
  } catch (...) {
    return false;
  }
}

ExportDialog::ExportDialog(QWidget *parent) : BaseDialog(parent) {
  setObjectName("ExportDialog");
  setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
  setMinimumSize(580, 560);
  resize(600, 600);

  // 初始化折叠图标
  downArrowPixmap_ = QPixmap(":/icons/down-arrow.svg");
  QTransform trans;
  trans.rotate(-90);
  rightArrowPixmap_ =
      downArrowPixmap_.transformed(trans, Qt::SmoothTransformation);

  setupTitleBar();
  setupUi();
  retranslateUi();

  setupWindowAgent(titleBar);
}

ExportDialog::~ExportDialog() {}

void ExportDialog::setSubtitleTrack(const SubtitleTrack *track) {
  track_ = track;
  if (track_ && track_->items().isEmpty()) {
    exportSubtitleChk_->setChecked(false);
    exportSubtitleChk_->setEnabled(false);
    subtitleSectionHeader_->setEnabled(false);
  } else {
    exportSubtitleChk_->setChecked(false);
    exportSubtitleChk_->setEnabled(true);
    subtitleSectionHeader_->setEnabled(true);
  }

  if (pathEdit_ && pathEdit_->text().isEmpty()) {
    pathEdit_->setText(QDir::toNativeSeparators(
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)));
  }

  if (titleEdit_ && titleEdit_->text().isEmpty()) {
    titleEdit_->setText("subtitles");
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

    // 如果有视频路径，且当前路径为空或为默认Documents路径，则智能覆盖为视频所在目录
    QString currentPath = pathEdit_->text().trimmed();
    QString docPath = QDir::toNativeSeparators(
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    if (currentPath.isEmpty() || currentPath == docPath) {
      pathEdit_->setText(
          QDir::toNativeSeparators(QFileInfo(sourceVideoPath_).absolutePath()));
    }

    // 初始化导出标题为视频文件名
    if (titleEdit_) {
      titleEdit_->setText(QFileInfo(sourceVideoPath_).completeBaseName());
    }
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
      standards[sourceIndex].label = tr("%1x%2 (原始)")
                                         .arg(sourceVideoSize_.width())
                                         .arg(sourceVideoSize_.height());
    } else {
      // 动态插入非标准源分辨率到首位
      ResItem item = {tr("%1x%2 (原始)")
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
          tr("%1 fps (原始)").arg(qRound(sourceFps_));
    } else {
      // 动态插入非标准帧率
      FpsItem item = {tr("%1 fps (原始)").arg(sourceFps_, 0, 'f', 2),
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
      standards[index].label = tr("%1 kbps (原始)").arg(sourceBrKbps);
    } else {
      BitrateItem item = {tr("%1 kbps (原始)").arg(sourceBrKbps), sourceBrKbps};
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
          tr("%1 Hz (原始)").arg(sourceAudioSampleRate_);
    } else {
      SampleItem item = {tr("%1 Hz (原始)").arg(sourceAudioSampleRate_),
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

  scrollArea_ = new QScrollArea(contentWidget);
  scrollArea_->setWidgetResizable(true);
  scrollArea_->setFrameShape(QFrame::NoFrame);
  scrollArea_->setObjectName("PropertyScrollArea");

  QWidget *scrollContent = new QWidget(scrollArea_);
  scrollContent->setObjectName("PropertyScrollContent");
  QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContent);
  scrollLayout->setContentsMargins(0, 0, 10, 0);
  scrollLayout->setSpacing(15);

  // --- 1. 导出标题与路径选择区 (移动到最顶端，使用 QFormLayout) ---
  QFrame *pathFrame = new QFrame(scrollContent);
  pathFrame->setObjectName("ExportSectionFrame");

  QFormLayout *pathForm = new QFormLayout(pathFrame);
  pathForm->setContentsMargins(15, 15, 15, 15);
  pathForm->setSpacing(10);

  // 导出标题
  exportTitleLabel_ = new QLabel(tr("导出标题"), pathFrame);
  exportTitleLabel_->setObjectName("ConfigFieldLabel");

  titleEdit_ = new QLineEdit(pathFrame);
  titleEdit_->setPlaceholderText(tr("输入导出文件名（不含后缀）"));
  connect(titleEdit_, &QLineEdit::textChanged, this,
          &ExportDialog::checkExportButtonEnabled);

  pathForm->addRow(exportTitleLabel_, titleEdit_);

  // 输出路径
  pathTitle_ = new QLabel(tr("输出路径"), pathFrame);
  pathTitle_->setObjectName("ConfigFieldLabel");

  QHBoxLayout *pathLineLayout = new QHBoxLayout();
  pathLineLayout->setContentsMargins(0, 0, 0, 0);
  pathLineLayout->setSpacing(0); // 拼接无缝隙

  pathEdit_ = new QLineEdit(pathFrame);
  pathEdit_->setObjectName("SpeakerFolderEdit");
  connect(pathEdit_, &QLineEdit::textChanged, this,
          &ExportDialog::checkExportButtonEnabled);

  browseBtn_ = new QPushButton(tr("浏览..."), pathFrame);
  browseBtn_->setObjectName("SpeakerBrowseButton");
  connect(browseBtn_, &QPushButton::clicked, this,
          &ExportDialog::onBrowseClicked);

  pathLineLayout->addWidget(pathEdit_, 1);
  pathLineLayout->addWidget(browseBtn_);

  pathForm->addRow(pathTitle_, pathLineLayout);
  scrollLayout->addWidget(pathFrame);

  // --- 2. 导出视频折叠区域 ---
  exportVideoChk_ = new QCheckBox(scrollContent);
  exportVideoChk_->setObjectName("ExportVideoCheck");
  connect(exportVideoChk_, &QCheckBox::stateChanged, this,
          &ExportDialog::onVideoCheckChanged);

  videoSectionHeader_ = new QPushButton(scrollContent);
  videoSectionHeader_->setObjectName("ExportSectionHeader");
  videoSectionHeader_->setFlat(true);

  // 使用内部布局管理文字和 SVG 箭头
  QHBoxLayout *videoHeaderInnerLayout = new QHBoxLayout(videoSectionHeader_);
  videoHeaderInnerLayout->setContentsMargins(2, 0, 8, 0);
  videoHeaderInnerLayout->setSpacing(6);

  videoHeaderLabel_ = new QLabel(videoSectionHeader_);

  videoHeaderIcon_ = new QLabel(videoSectionHeader_);
  videoHeaderIcon_->setObjectName("ExportSectionIcon");
  videoHeaderIcon_->setFixedSize(14, 14);
  videoHeaderIcon_->setScaledContents(true);

  videoHeaderInnerLayout->addWidget(videoHeaderLabel_, 0, Qt::AlignVCenter);
  videoHeaderInnerLayout->addWidget(videoHeaderIcon_, 0, Qt::AlignVCenter);
  videoHeaderInnerLayout->addStretch();

  connect(videoSectionHeader_, &QPushButton::clicked, this, [this]() {
    if (exportVideoChk_->isEnabled()) {
      videoExpanded_ = !videoExpanded_;
      updateAccordionStates();
    }
  });

  QHBoxLayout *videoHeaderLayout = new QHBoxLayout();
  videoHeaderLayout->setContentsMargins(0, 0, 0, 0);
  videoHeaderLayout->setSpacing(4);
  videoHeaderLayout->addWidget(exportVideoChk_);
  videoHeaderLayout->addWidget(videoSectionHeader_, 1);
  scrollLayout->addLayout(videoHeaderLayout);

  videoSectionFrame_ = new QFrame(scrollContent);
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

  videoFormatLabel_ = new QLabel(tr("视频格式"), videoSectionFrame_);
  videoFormatLabel_->setObjectName("ConfigFieldLabel");
  videoForm->addRow(videoFormatLabel_, videoFormatCombo_);

  videoCodecCombo_ = new QComboBox(videoSectionFrame_);
  // Populate H.264 encoders dynamically based on availability
  bool addedH264 = false;
  if (isEncoderAvailable("h264_videotoolbox")) {
    videoCodecCombo_->addItem(tr("H.264 (硬件 - VideoToolbox)"),
                              "h264_videotoolbox");
    addedH264 = true;
  }
  if (isEncoderAvailable("h264_mf")) {
    videoCodecCombo_->addItem(tr("H.264 (硬件 - MediaFoundation)"), "h264_mf");
    addedH264 = true;
  }
  if (isEncoderAvailable("h264_nvenc")) {
    videoCodecCombo_->addItem(tr("H.264 (硬件 - NVIDIA NVENC)"), "h264_nvenc");
    addedH264 = true;
  }
  if (isEncoderAvailable("libx264")) {
    videoCodecCombo_->addItem(tr("H.264 (软件 - libx264)"), "libx264");
    addedH264 = true;
  }
  if (!addedH264) {
    if (const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264)) {
      if (isEncoderAvailable(codec->name)) {
        videoCodecCombo_->addItem(QString("H.264 (%1)").arg(codec->name),
                                  codec->name);
        addedH264 = true;
      }
    }
  }

  // Populate HEVC encoders dynamically based on availability
  bool addedHevc = false;
  if (isEncoderAvailable("hevc_videotoolbox")) {
    videoCodecCombo_->addItem(tr("HEVC (硬件 - VideoToolbox)"),
                              "hevc_videotoolbox");
    addedHevc = true;
  }
  if (isEncoderAvailable("hevc_mf")) {
    videoCodecCombo_->addItem(tr("HEVC (硬件 - MediaFoundation)"), "hevc_mf");
    addedHevc = true;
  }
  if (isEncoderAvailable("hevc_nvenc")) {
    videoCodecCombo_->addItem(tr("HEVC (硬件 - NVIDIA NVENC)"), "hevc_nvenc");
    addedHevc = true;
  }
  if (isEncoderAvailable("libx265")) {
    videoCodecCombo_->addItem(tr("HEVC (软件 - libx265)"), "libx265");
    addedHevc = true;
  }
  if (!addedHevc) {
    if (const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_HEVC)) {
      if (isEncoderAvailable(codec->name)) {
        videoCodecCombo_->addItem(QString("HEVC (%1)").arg(codec->name),
                                  codec->name);
        addedHevc = true;
      }
    }
  }

  videoCodecLabel_ = new QLabel(tr("视频编码"), videoSectionFrame_);
  videoCodecLabel_->setObjectName("ConfigFieldLabel");
  videoForm->addRow(videoCodecLabel_, videoCodecCombo_);

  videoResolutionCombo_ = new QComboBox(videoSectionFrame_);
  videoResolutionLabel_ = new QLabel(tr("分辨率"), videoSectionFrame_);
  videoResolutionLabel_->setObjectName("ConfigFieldLabel");
  videoForm->addRow(videoResolutionLabel_, videoResolutionCombo_);

  videoFpsCombo_ = new QComboBox(videoSectionFrame_);
  videoFpsLabel_ = new QLabel(tr("帧率"), videoSectionFrame_);
  videoFpsLabel_->setObjectName("ConfigFieldLabel");
  videoForm->addRow(videoFpsLabel_, videoFpsCombo_);

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

  videoQualityLabel_ = new QLabel(tr("视频质量"), videoSectionFrame_);
  videoQualityLabel_->setObjectName("ConfigFieldLabel");
  videoForm->addRow(videoQualityLabel_, videoQualityCombo_);

  // 自定义码率输入子面板
  customBitrateFrame_ = new QFrame(videoSectionFrame_);
  QHBoxLayout *bitrateLayout = new QHBoxLayout(customBitrateFrame_);
  bitrateLayout->setContentsMargins(0, 0, 0, 0);
  customBitrateEdit_ = new QLineEdit("8000", customBitrateFrame_);
  customBitrateEdit_->setValidator(new QIntValidator(100, 100000, this));
  customBitrateEdit_->setFixedWidth(80);
  kbpsLabel_ = new QLabel("kbps", customBitrateFrame_);
  kbpsLabel_->setObjectName("ConfigFieldLabel");
  bitrateLayout->addWidget(customBitrateEdit_);
  bitrateLayout->addWidget(kbpsLabel_);
  bitrateLayout->addStretch();
  customBitrateFrame_->setVisible(false);
  videoForm->addRow(QString(), customBitrateFrame_);

  // 音频处理
  audioBitrateCombo_ = new QComboBox(videoSectionFrame_);
  audioBitrateLabel_ = new QLabel(tr("音频码率"), videoSectionFrame_);
  audioBitrateLabel_->setObjectName("ConfigFieldLabel");
  videoForm->addRow(audioBitrateLabel_, audioBitrateCombo_);

  audioSampleRateCombo_ = new QComboBox(videoSectionFrame_);
  audioSampleRateLabel_ = new QLabel(tr("音频采样率"), videoSectionFrame_);
  audioSampleRateLabel_->setObjectName("ConfigFieldLabel");
  videoForm->addRow(audioSampleRateLabel_, audioSampleRateCombo_);

  scrollLayout->addWidget(videoSectionFrame_);

  // --- 3. 导出字幕折叠区域 ---
  exportSubtitleChk_ = new QCheckBox(scrollContent);
  exportSubtitleChk_->setObjectName("ExportSubtitleCheck");
  connect(exportSubtitleChk_, &QCheckBox::stateChanged, this,
          &ExportDialog::onSubtitleCheckChanged);

  subtitleSectionHeader_ = new QPushButton(scrollContent);
  subtitleSectionHeader_->setObjectName("ExportSectionHeader");
  subtitleSectionHeader_->setFlat(true);

  // 使用内部布局管理文字和 SVG 箭头
  QHBoxLayout *subtitleHeaderInnerLayout =
      new QHBoxLayout(subtitleSectionHeader_);
  subtitleHeaderInnerLayout->setContentsMargins(2, 0, 8, 0);
  subtitleHeaderInnerLayout->setSpacing(6);

  subtitleHeaderLabel_ = new QLabel(subtitleSectionHeader_);

  subtitleHeaderIcon_ = new QLabel(subtitleSectionHeader_);
  subtitleHeaderIcon_->setObjectName("ExportSectionIcon");
  subtitleHeaderIcon_->setFixedSize(14, 14);
  subtitleHeaderIcon_->setScaledContents(true);

  subtitleHeaderInnerLayout->addWidget(subtitleHeaderLabel_, 0,
                                       Qt::AlignVCenter);
  subtitleHeaderInnerLayout->addWidget(subtitleHeaderIcon_, 0,
                                       Qt::AlignVCenter);
  subtitleHeaderInnerLayout->addStretch();

  connect(subtitleSectionHeader_, &QPushButton::clicked, this, [this]() {
    if (exportSubtitleChk_->isEnabled()) {
      subtitleExpanded_ = !subtitleExpanded_;
      updateAccordionStates();
    }
  });

  QHBoxLayout *subHeaderLayout = new QHBoxLayout();
  subHeaderLayout->setContentsMargins(0, 0, 0, 0);
  subHeaderLayout->setSpacing(4);
  subHeaderLayout->addWidget(exportSubtitleChk_);
  subHeaderLayout->addWidget(subtitleSectionHeader_, 1);
  scrollLayout->addLayout(subHeaderLayout);

  subtitleSectionFrame_ = new QFrame(scrollContent);
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

  subtitleFormatLabel_ = new QLabel(tr("字幕格式"), subtitleSectionFrame_);
  subtitleFormatLabel_->setObjectName("ConfigFieldLabel");
  subForm->addRow(subtitleFormatLabel_, subtitleFormatCombo_);

  scrollLayout->addWidget(subtitleSectionFrame_);

  scrollLayout->addStretch();
  scrollArea_->setWidget(scrollContent);
  contentLayout->addWidget(scrollArea_);

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
  if (titleLabel) {
    titleLabel->setText(tr("导出"));
  }

  // 刷新 Header 折叠项文本
  updateAccordionStates();

  // 刷新 Label 文本
  if (videoFormatLabel_)
    videoFormatLabel_->setText(tr("视频格式"));
  if (videoCodecLabel_)
    videoCodecLabel_->setText(tr("视频编码"));
  if (videoResolutionLabel_)
    videoResolutionLabel_->setText(tr("分辨率"));
  if (videoFpsLabel_)
    videoFpsLabel_->setText(tr("帧率"));
  if (videoQualityLabel_)
    videoQualityLabel_->setText(tr("视频质量"));
  if (audioBitrateLabel_)
    audioBitrateLabel_->setText(tr("音频码率"));
  if (audioSampleRateLabel_)
    audioSampleRateLabel_->setText(tr("音频采样率"));
  if (subtitleFormatLabel_)
    subtitleFormatLabel_->setText(tr("字幕格式"));
  if (kbpsLabel_)
    kbpsLabel_->setText("kbps");
  if (pathTitle_)
    pathTitle_->setText(tr("输出路径"));
  if (exportTitleLabel_)
    exportTitleLabel_->setText(tr("导出标题"));
  if (titleEdit_)
    titleEdit_->setPlaceholderText(tr("输入导出文件名（不含后缀）"));

  if (browseBtn_)
    browseBtn_->setText(tr("浏览..."));
  if (cancelBtn_)
    cancelBtn_->setText(tr("取消"));
  if (exportBtn_)
    exportBtn_->setText(tr("导出"));

  // 刷新静态下拉选项文本
  if (videoFormatCombo_) {
    videoFormatCombo_->setItemText(0, "MP4");
    videoFormatCombo_->setItemText(1, "MOV");
  }

  if (videoQualityCombo_) {
    videoQualityCombo_->setItemText(0, tr("高质量"));
    videoQualityCombo_->setItemText(1, tr("中等质量 (推荐)"));
    videoQualityCombo_->setItemText(2, tr("较低质量"));
    videoQualityCombo_->setItemText(3, tr("自定义码率"));
  }

  if (subtitleFormatCombo_) {
    subtitleFormatCombo_->setItemText(0, "SRT (*.srt)");
    subtitleFormatCombo_->setItemText(1, "TXT (*.txt)");
    subtitleFormatCombo_->setItemText(2, "Premiere Pro XML (*.xml)");
    subtitleFormatCombo_->setItemText(3, "Final Cut Pro XML (*.fcpxml)");
  }

  // 刷新视频编码下拉选项文本
  if (videoCodecCombo_) {
    QString selectedCodec = videoCodecCombo_->currentData().toString();
    videoCodecCombo_->clear();

    // Populate H.264 encoders dynamically based on availability
    bool addedH264 = false;
    if (isEncoderAvailable("h264_videotoolbox")) {
      videoCodecCombo_->addItem(tr("H.264 (硬件 - VideoToolbox)"),
                                "h264_videotoolbox");
      addedH264 = true;
    }
    if (isEncoderAvailable("h264_mf")) {
      videoCodecCombo_->addItem(tr("H.264 (硬件 - MediaFoundation)"),
                                "h264_mf");
      addedH264 = true;
    }
    if (isEncoderAvailable("h264_nvenc")) {
      videoCodecCombo_->addItem(tr("H.264 (硬件 - NVIDIA NVENC)"),
                                "h264_nvenc");
      addedH264 = true;
    }
    if (isEncoderAvailable("libx264")) {
      videoCodecCombo_->addItem(tr("H.264 (软件 - libx264)"), "libx264");
      addedH264 = true;
    }
    if (!addedH264) {
      if (const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264)) {
        if (isEncoderAvailable(codec->name)) {
          videoCodecCombo_->addItem(QString("H.264 (%1)").arg(codec->name),
                                    codec->name);
          addedH264 = true;
        }
      }
    }

    // Populate HEVC encoders dynamically based on availability
    bool addedHevc = false;
    if (isEncoderAvailable("hevc_videotoolbox")) {
      videoCodecCombo_->addItem(tr("HEVC (硬件 - VideoToolbox)"),
                                "hevc_videotoolbox");
      addedHevc = true;
    }
    if (isEncoderAvailable("hevc_mf")) {
      videoCodecCombo_->addItem(tr("HEVC (硬件 - MediaFoundation)"), "hevc_mf");
      addedHevc = true;
    }
    if (isEncoderAvailable("hevc_nvenc")) {
      videoCodecCombo_->addItem(tr("HEVC (硬件 - NVIDIA NVENC)"), "hevc_nvenc");
      addedHevc = true;
    }
    if (isEncoderAvailable("libx265")) {
      videoCodecCombo_->addItem(tr("HEVC (软件 - libx265)"), "libx265");
      addedHevc = true;
    }
    if (!addedHevc) {
      if (const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_HEVC)) {
        if (isEncoderAvailable(codec->name)) {
          videoCodecCombo_->addItem(QString("HEVC (%1)").arg(codec->name),
                                    codec->name);
          addedHevc = true;
        }
      }
    }

    for (int i = 0; i < videoCodecCombo_->count(); ++i) {
      if (videoCodecCombo_->itemData(i).toString() == selectedCodec) {
        videoCodecCombo_->setCurrentIndex(i);
        break;
      }
    }
  }

  // 刷新动态预设 (保持选中)
  if (videoResolutionCombo_ && videoFpsCombo_) {
    QSize selectedRes = videoResolutionCombo_->currentData().toSize();
    double selectedFps = videoFpsCombo_->currentData().toDouble();
    initializeVideoPresets();

    for (int i = 0; i < videoResolutionCombo_->count(); ++i) {
      if (videoResolutionCombo_->itemData(i).toSize() == selectedRes) {
        videoResolutionCombo_->setCurrentIndex(i);
        break;
      }
    }
    for (int i = 0; i < videoFpsCombo_->count(); ++i) {
      if (qAbs(videoFpsCombo_->itemData(i).toDouble() - selectedFps) < 0.05) {
        videoFpsCombo_->setCurrentIndex(i);
        break;
      }
    }
  }

  if (audioBitrateCombo_ && audioSampleRateCombo_) {
    int selectedBr = audioBitrateCombo_->currentData().toInt();
    int selectedSr = audioSampleRateCombo_->currentData().toInt();
    initializeAudioPresets();

    for (int i = 0; i < audioBitrateCombo_->count(); ++i) {
      if (audioBitrateCombo_->itemData(i).toInt() == selectedBr) {
        audioBitrateCombo_->setCurrentIndex(i);
        break;
      }
    }
    for (int i = 0; i < audioSampleRateCombo_->count(); ++i) {
      if (audioSampleRateCombo_->itemData(i).toInt() == selectedSr) {
        audioSampleRateCombo_->setCurrentIndex(i);
        break;
      }
    }
  }

  checkExportButtonEnabled();
  updatePathExtension();
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

  // 刷新 Header 上的文字与图标
  if (videoHeaderLabel_) {
    videoHeaderLabel_->setText(tr("视频设置"));
  }
  if (videoHeaderIcon_) {
    bool isExpanded = videoExpanded_ && exportVideoChk_->isChecked();
    videoHeaderIcon_->setPixmap(isExpanded ? downArrowPixmap_
                                           : rightArrowPixmap_);
  }

  if (subtitleHeaderLabel_) {
    subtitleHeaderLabel_->setText(tr("字幕设置"));
  }
  if (subtitleHeaderIcon_) {
    bool isExpanded = subtitleExpanded_ && exportSubtitleChk_->isChecked();
    subtitleHeaderIcon_->setPixmap(isExpanded ? downArrowPixmap_
                                              : rightArrowPixmap_);
  }
}

void ExportDialog::onQualityModeChanged(int index) {
  int mode = videoQualityCombo_->itemData(index).toInt();
  customBitrateFrame_->setVisible(mode ==
                                  VideoExportConfig::QualityCustomBitrate);
}

void ExportDialog::onFormatChanged(int index) {
  Q_UNUSED(index)
  updatePathExtension();
}

void ExportDialog::updatePathExtension() {
  // 留空，根据用户反馈，文件夹下面不需要显示视频或者字幕文件的完整路径提示
}

void ExportDialog::onBrowseClicked() {
  QString dir = QFileDialog::getExistingDirectory(this, tr("选择导出目录"),
                                                  pathEdit_->text());
  if (!dir.isEmpty()) {
    pathEdit_->setText(dir);
  }
}

void ExportDialog::checkExportButtonEnabled() {
  bool anyChecked =
      (exportVideoChk_->isChecked() || exportSubtitleChk_->isChecked());
  bool titleOk = titleEdit_ && !titleEdit_->text().trimmed().isEmpty();
  bool pathOk = !pathEdit_->text().trimmed().isEmpty();
  exportBtn_->setEnabled(anyChecked && titleOk && pathOk);
}

void ExportDialog::onExportClicked() {
  QString dirPath = pathEdit_->text().trimmed();
  if (dirPath.isEmpty()) {
    AppMessageBox::warning(this, tr("导出"), tr("保存路径不能为空。"));
    return;
  }

  QDir dir(dirPath);
  if (!dir.exists()) {
    AppMessageBox::warning(this, tr("导出"),
                           tr("保存目录不存在，请选择合法的路径。"));
    return;
  }

  QString title = titleEdit_->text().trimmed();
  if (title.isEmpty()) {
    AppMessageBox::warning(this, tr("导出"), tr("导出标题不能为空。"));
    return;
  }

  // 检测目标文件是否已存在，如果已存在，则自动在标题后面追加精确到秒的时间戳，如
  // 20260526172815
  bool videoExists = false;
  bool subtitleExists = false;
  QString cleanTitle = title;

  if (exportVideoChk_->isChecked()) {
    QString ext = videoFormatCombo_->currentData().toString();
    QString videoPath = dirPath + "/" + cleanTitle + "." + ext;
    if (QFile::exists(videoPath)) {
      videoExists = true;
    }
  }
  if (exportSubtitleChk_->isChecked()) {
    QString ext = subtitleFormatCombo_->currentData().toString();
    QString subPath = dirPath + "/" + cleanTitle + "." + ext;
    if (QFile::exists(subPath)) {
      subtitleExists = true;
    }
  }

  if (videoExists || subtitleExists) {
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMddHHmmss");
    titleEdit_->setText(cleanTitle + "-" + timestamp);
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

QString ExportDialog::outputPath() const {
  QString dir = pathEdit_->text().trimmed();
  if (dir.isEmpty())
    return QString();

  QString title = titleEdit_ ? titleEdit_->text().trimmed() : QString();
  if (title.isEmpty()) {
    if (!sourceVideoPath_.isEmpty()) {
      title = QFileInfo(sourceVideoPath_).completeBaseName();
    } else if (track_ && !track_->items().isEmpty()) {
      title = "subtitles";
    } else {
      title = "export";
    }
  }

  if (exportVideoChk_->isChecked()) {
    QString ext = videoFormatCombo_->currentData().toString();
    return dir + "/" + title + "." + ext;
  } else if (exportSubtitleChk_->isChecked()) {
    QString ext = subtitleFormatCombo_->currentData().toString();
    return dir + "/" + title + "." + ext;
  }

  return dir;
}

VideoExportConfig ExportDialog::videoConfig() const {
  VideoExportConfig config;
  config.inputPath = sourceVideoPath_;
  config.outputPath = outputPath();
  config.videoCodec = videoCodecCombo_->currentData().toString();

  int qIdx = videoQualityCombo_->currentIndex();
  config.qualityMode = static_cast<VideoExportConfig::QualityMode>(
      videoQualityCombo_->itemData(qIdx).toInt());
  config.customBitrateKbps = customBitrateEdit_->text().toInt();

  QSize targetSize = videoResolutionCombo_->currentData().toSize();
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
