#include "AsrConfigDialog.h"
#include "AppMessageBox.h"
#include "ConfigManager.h"
#include "TranslationManager.h"
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFrame>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>

AsrConfigDialog::AsrConfigDialog(QWidget *parent) : BaseDialog(parent) {
  setWindowTitle(tr("语音识别配置"));
  setMinimumSize(460, 500);
  resize(480, 540);

  setObjectName("AsrConfigDialog");

  setupTitleBar();
  setupUi();
  retranslateUi();
  loadDefaultConfig();

  connect(btnCancel_, &QPushButton::clicked, this, &QDialog::reject);
  connect(btnOk_, &QPushButton::clicked, this, &QDialog::accept);

  connect(&TranslationManager::instance(), &TranslationManager::languageChanged,
          this, [this]() { retranslateUi(); });

  setupWindowAgent(titleBar);
}

AsrConfigDialog::~AsrConfigDialog() {
  if (isDownloading_ && reply_) {
    reply_->abort();
  }
}

void AsrConfigDialog::setupUi() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  mainLayout->addWidget(titleBar);

  auto *contentWidget = new QWidget(this);
  contentWidget->setObjectName("ConfigContentWidget");
  auto *contentLayout = new QVBoxLayout(contentWidget);
  contentLayout->setContentsMargins(30, 15, 30, 15);
  contentLayout->setSpacing(12);

  // ASR Provider Selection
  asrProvLabel_ = new QLabel(tr("识别提供商"), contentWidget);
  asrProvLabel_->setObjectName("ConfigFieldLabel");
  contentLayout->addWidget(asrProvLabel_);

  asrProviderCombo_ = new QComboBox(contentWidget);
  asrProviderCombo_->setFixedHeight(32);
  asrProviderCombo_->addItem(tr("本地 ASR (Whisper)"), "local_whisper");
  asrProviderCombo_->addItem(tr("腾讯云 ASR"), "tencent_asr");
  asrProviderCombo_->setObjectName("ConfigComboBox");
  contentLayout->addWidget(asrProviderCombo_);

  // ============================================================
  // Tencent ASR Container
  // ============================================================
  tencentContainer_ = new QWidget(contentWidget);
  auto *tencentLayout = new QVBoxLayout(tencentContainer_);
  tencentLayout->setContentsMargins(0, 0, 0, 0);
  tencentLayout->setSpacing(12);

  engineLabel_ = new QLabel(tr("引擎模型类型"), tencentContainer_);
  engineLabel_->setObjectName("ConfigFieldLabel");
  tencentLayout->addWidget(engineLabel_);

  engineModelTypeCombo_ = new QComboBox(tencentContainer_);
  engineModelTypeCombo_->setFixedHeight(32);
  engineModelTypeCombo_->setObjectName("ConfigComboBox");
  engineModelTypeCombo_->addItem("16k_zh_en(中英粤+9种方言大模型)",
                                 "16k_zh_en");
  engineModelTypeCombo_->addItem("16k_zh_large(普方英大模型)", "16k_zh_large");
  engineModelTypeCombo_->addItem("16k_multi_lang(多语种大模型)",
                                 "16k_multi_lang");
  engineModelTypeCombo_->addItem("16k_zh(中文普通话通用)", "16k_zh");
  engineModelTypeCombo_->addItem("16k_en(英语)", "16k_en");
  engineModelTypeCombo_->addItem("16k_en_large(英语大模型)", "16k_en_large");
  engineModelTypeCombo_->addItem("16k_yue(粤语)", "16k_yue");
  engineModelTypeCombo_->addItem("16k_zh-PY(中英粤混合)", "16k_zh-PY");
  engineModelTypeCombo_->addItem("16k_zh-TW(中文繁体)", "16k_zh-TW");
  engineModelTypeCombo_->addItem("16k_ja(日语)", "16k_ja");
  engineModelTypeCombo_->addItem("16k_ko(韩语)", "16k_ko");
  engineModelTypeCombo_->addItem("16k_vi(越南语)", "16k_vi");
  engineModelTypeCombo_->addItem("16k_ms(马来语)", "16k_ms");
  engineModelTypeCombo_->addItem("16k_id(印度尼西亚语)", "16k_id");
  engineModelTypeCombo_->addItem("16k_fil(菲律宾语)", "16k_fil");
  engineModelTypeCombo_->addItem("16k_th(泰语)", "16k_th");
  engineModelTypeCombo_->addItem("16k_pt(葡萄牙语)", "16k_pt");
  engineModelTypeCombo_->addItem("16k_tr(土耳其语)", "16k_tr");
  engineModelTypeCombo_->addItem("16k_ar(阿拉伯语)", "16k_ar");
  engineModelTypeCombo_->addItem("16k_es(西班牙语)", "16k_es");
  engineModelTypeCombo_->addItem("16k_hi(印地语)", "16k_hi");
  engineModelTypeCombo_->addItem("16k_fr(法语)", "16k_fr");
  engineModelTypeCombo_->addItem("16k_de(德语)", "16k_de");
  engineModelTypeCombo_->addItem("16k_zh_medical(中文医疗)", "16k_zh_medical");
  tencentLayout->addWidget(engineModelTypeCombo_);

  maxLenLabel_ = new QLabel(tr("单行字幕最大字数"), tencentContainer_);
  maxLenLabel_->setObjectName("ConfigFieldLabel");
  tencentLayout->addWidget(maxLenLabel_);

  sentenceMaxLengthSpin_ = new QSpinBox(tencentContainer_);
  sentenceMaxLengthSpin_->setFixedHeight(32);
  sentenceMaxLengthSpin_->setRange(6, 40);
  sentenceMaxLengthSpin_->setValue(16);
  sentenceMaxLengthSpin_->setObjectName("ConfigSpinBox");
  tencentLayout->addWidget(sentenceMaxLengthSpin_);

  speakerDiarizationLabel_ = new QLabel(tr("说话人识别"), tencentContainer_);
  speakerDiarizationLabel_->setObjectName("ConfigFieldLabel");
  tencentLayout->addWidget(speakerDiarizationLabel_);

  speakerDiarizationCheck_ =
      new QCheckBox(tr("开启说话人识别"), tencentContainer_);
  speakerDiarizationCheck_->setFixedHeight(32);
  speakerDiarizationCheck_->setObjectName("ConfigCheckBox");
  tencentLayout->addWidget(speakerDiarizationCheck_);

  contentLayout->addWidget(tencentContainer_);

  // ============================================================
  // Whisper Local ASR Container
  // ============================================================
  whisperContainer_ = new QWidget(contentWidget);
  auto *whisperLayout = new QVBoxLayout(whisperContainer_);
  whisperLayout->setContentsMargins(0, 0, 0, 0);
  whisperLayout->setSpacing(12);

  whisperModelLabel_ = new QLabel(tr("语音识别模型"), whisperContainer_);
  whisperModelLabel_->setObjectName("ConfigFieldLabel");
  whisperLayout->addWidget(whisperModelLabel_);

  whisperModelCombo_ = new QComboBox(whisperContainer_);
  whisperModelCombo_->setFixedHeight(32);
  whisperModelCombo_->addItem("base (~148MB)", "base");
  whisperModelCombo_->addItem("small (~466MB)", "small");
  whisperModelCombo_->addItem("medium (~1.5GB)", "medium");
  whisperModelCombo_->addItem("large-v3 (~2.9GB)", "large-v3");
  whisperModelCombo_->addItem("large-v3-turbo (~1.5GB)", "large-v3-turbo");
  whisperModelCombo_->setObjectName("ConfigComboBox");
  whisperLayout->addWidget(whisperModelCombo_);

  // Model status and download row
  auto *statusLayout = new QHBoxLayout();
  statusLayout->setSpacing(10);

  whisperStatusLabel_ = new QLabel(whisperContainer_);
  whisperStatusLabel_->setObjectName("ConfigFieldLabel");
  statusLayout->addWidget(whisperStatusLabel_);

  btnDownload_ = new QPushButton(tr("下载模型"), whisperContainer_);
  btnDownload_->setFixedHeight(30);
  btnDownload_->setFixedWidth(100);
  statusLayout->addWidget(btnDownload_);
  statusLayout->addStretch();
  whisperLayout->addLayout(statusLayout);

  whisperProgressBar_ = new QProgressBar(whisperContainer_);
  whisperProgressBar_->setFixedHeight(20);
  whisperProgressBar_->setRange(0, 100);
  whisperProgressBar_->setValue(0);
  whisperProgressBar_->setTextVisible(true);
  whisperProgressBar_->hide();
  whisperLayout->addWidget(whisperProgressBar_);

  whisperLangLabel_ = new QLabel(tr("识别语言"), whisperContainer_);
  whisperLangLabel_->setObjectName("ConfigFieldLabel");
  whisperLayout->addWidget(whisperLangLabel_);

  whisperLangCombo_ = new QComboBox(whisperContainer_);
  whisperLangCombo_->setFixedHeight(32);
  whisperLangCombo_->addItem(tr("自动检测"), "auto");
  whisperLangCombo_->addItem(tr("中文"), "zh");
  whisperLangCombo_->addItem(tr("英文"), "en");
  whisperLangCombo_->addItem(tr("日文"), "ja");
  whisperLangCombo_->addItem(tr("韩文"), "ko");
  whisperLangCombo_->setObjectName("ConfigComboBox");
  whisperLayout->addWidget(whisperLangCombo_);

  whisperMaxLenLabel_ = new QLabel(tr("单行字幕最大字数"), whisperContainer_);
  whisperMaxLenLabel_->setObjectName("ConfigFieldLabel");
  whisperLayout->addWidget(whisperMaxLenLabel_);

  whisperMaxLenSpin_ = new QSpinBox(whisperContainer_);
  whisperMaxLenSpin_->setFixedHeight(32);
  whisperMaxLenSpin_->setRange(6, 40);
  whisperMaxLenSpin_->setValue(16);
  whisperMaxLenSpin_->setObjectName("ConfigSpinBox");
  whisperLayout->addWidget(whisperMaxLenSpin_);

  whisperThreadsLabel_ = new QLabel(tr("计算线程数"), whisperContainer_);
  whisperThreadsLabel_->setObjectName("ConfigFieldLabel");
  whisperLayout->addWidget(whisperThreadsLabel_);

  whisperThreadsSpin_ = new QSpinBox(whisperContainer_);
  whisperThreadsSpin_->setFixedHeight(32);
  whisperThreadsSpin_->setRange(1, 64);
  whisperThreadsSpin_->setValue(4);
  whisperThreadsSpin_->setObjectName("ConfigSpinBox");
  whisperLayout->addWidget(whisperThreadsSpin_);

  contentLayout->addWidget(whisperContainer_);

  contentLayout->addStretch();
  mainLayout->addWidget(contentWidget, 1);

  // Footer
  auto *footer = new QWidget(this);
  footer->setObjectName("ConfigFooter");
  footer->setFixedHeight(60);
  auto *footerLayout = new QHBoxLayout(footer);
  footerLayout->setContentsMargins(20, 0, 20, 0);

  footerLayout->addStretch();

  btnCancel_ = new QPushButton(tr("取消"), footer);
  btnCancel_->setObjectName("ConfigCancelButton");
  btnCancel_->setFixedWidth(100);

  btnOk_ = new QPushButton(tr("确定"), footer);
  btnOk_->setObjectName("ConfigOkButton");
  btnOk_->setFixedWidth(100);

  footerLayout->addWidget(btnCancel_);
  footerLayout->addWidget(btnOk_);
  mainLayout->addWidget(footer);

  // Switching Connect
  connect(asrProviderCombo_, &QComboBox::currentTextChanged, this, [this]() {
    QString provider = asrProviderCombo_->currentData().toString();
    tencentContainer_->setVisible(provider == "tencent_asr");
    whisperContainer_->setVisible(provider == "local_whisper");
    updateWhisperStatus();
  });

  // Whisper model selection triggers verification
  connect(whisperModelCombo_, &QComboBox::currentTextChanged, this,
          [this]() { updateWhisperStatus(); });

  // Download Trigger
  connect(btnDownload_, &QPushButton::clicked, this,
          &AsrConfigDialog::onDownloadClicked);

  retranslateUi();
}

void AsrConfigDialog::loadDefaultConfig() {
  auto &cfg = ConfigManager::instance();

  // Load provider
  QString provider = cfg.asrProvider();
  int provIdx = asrProviderCombo_->findData(provider);
  if (provIdx >= 0) {
    asrProviderCombo_->setCurrentIndex(provIdx);
  }

  // Load Tencent settings
  sentenceMaxLengthSpin_->setValue(cfg.sentenceMaxLength());
  speakerDiarizationCheck_->setChecked(cfg.speakerDiarization());
  QString tModel = cfg.engineModelType();
  int tIdx = engineModelTypeCombo_->findData(tModel);
  if (tIdx >= 0) {
    engineModelTypeCombo_->setCurrentIndex(tIdx);
  }

  // Load Whisper settings
  QString wModel = cfg.whisperModel();
  int wIdx = whisperModelCombo_->findData(wModel);
  if (wIdx >= 0) {
    whisperModelCombo_->setCurrentIndex(wIdx);
  }

  QString wLang = cfg.whisperLanguage();
  if (wLang.isEmpty()) {
    wLang = "auto";
  }
  int lIdx = whisperLangCombo_->findData(wLang);
  if (lIdx >= 0) {
    whisperLangCombo_->setCurrentIndex(lIdx);
  } else {
    whisperLangCombo_->setCurrentIndex(0);
  }

  whisperThreadsSpin_->setValue(cfg.whisperThreads());
  whisperMaxLenSpin_->setValue(cfg.whisperMaxLen());

  // Setup initial visibility
  tencentContainer_->setVisible(provider == "tencent_asr");
  whisperContainer_->setVisible(provider == "local_whisper");
  updateWhisperStatus();
}

bool AsrConfigDialog::checkModelExists(const QString &modelName) {
  QString saveDir = ConfigManager::instance().whisperModelPath();
  QString fileName = QString("ggml-%1.bin").arg(modelName);
  return QFile::exists(saveDir + "/" + fileName);
}

void AsrConfigDialog::updateWhisperStatus() {
  if (isDownloading_) {
    return;
  }
  QString provider = asrProviderCombo_->currentData().toString();
  if (provider != "local_whisper") {
    btnOk_->setEnabled(true);
    return;
  }

  QString model = whisperModel();
  if (model != lastCheckedModel_) {
    lastCheckedModel_ = model;
    downloadError_.clear();
  }

  bool exists = checkModelExists(model);

  if (!downloadError_.isEmpty()) {
    whisperStatusLabel_->setText(
        tr("模型状态: 下载失败 (%1)").arg(downloadError_));
    whisperStatusLabel_->setStyleSheet("color: #ef4444; font-weight: bold;");
    whisperStatusLabel_->show();
    btnDownload_->show();
    btnOk_->setEnabled(false);
  } else if (exists) {
    whisperStatusLabel_->hide(); // 隐藏状态标签以腾出空间
    btnDownload_->hide();
    btnOk_->setEnabled(true);
  } else {
    whisperStatusLabel_->setText(tr("模型状态: 未下载"));
    whisperStatusLabel_->setStyleSheet("color: #ef4444; font-weight: bold;");
    whisperStatusLabel_->show();
    btnDownload_->show();
    btnOk_->setEnabled(false);
  }
}

void AsrConfigDialog::onDownloadClicked() {
  if (isDownloading_) {
    if (reply_) {
      reply_->abort();
    }
    return;
  }

  downloadError_.clear();
  lastReportedPercent_ = -1;

  QString modelName = whisperModel();
  QString fileName = QString("ggml-%1.bin").arg(modelName);

  QString saveDir = ConfigManager::instance().whisperModelPath();
  QDir().mkpath(saveDir);
  QString savePath = saveDir + "/" + fileName;

  downloadFile_ = new QFile(savePath, this);
  if (!downloadFile_->open(QIODevice::WriteOnly)) {
    downloadError_ = tr("无法创建模型文件: %1").arg(savePath);
#ifdef QT_DEBUG
    qDebug() << "[Debug] [AsrConfigDialog] " << downloadError_;
#endif
    downloadFile_->deleteLater();
    downloadFile_ = nullptr;
    updateWhisperStatus();
    return;
  }

  isDownloading_ = true;
  btnDownload_->setText(tr("取消"));
  // whisperProgressBar_->show(); // 移去进度条，仅保留文字描述
  whisperProgressBar_->setValue(0);
  whisperModelCombo_->setEnabled(false);
  whisperLangCombo_->setEnabled(false);
  whisperThreadsSpin_->setEnabled(false);
  whisperMaxLenSpin_->setEnabled(false);
  btnOk_->setEnabled(false);

  whisperStatusLabel_->show();
  whisperStatusLabel_->setText(tr("正在启动下载..."));
  whisperStatusLabel_->setStyleSheet("color: #3b82f6; font-weight: bold;");

  QString urlStr =
      QString("https://hf-mirror.com/ggerganov/whisper.cpp/resolve/main/%1")
          .arg(fileName);
  QUrl url(urlStr);

  if (!networkManager_) {
    networkManager_ = new QNetworkAccessManager(this);
  }

  startDownload(url, savePath);
}

void AsrConfigDialog::startDownload(const QUrl &url, const QString &savePath,
                                    int redirectCount) {
  if (redirectCount > 5) {
    downloadError_ = tr("重定向次数过多");
#ifdef QT_DEBUG
    qDebug() << "[Debug] [AsrConfigDialog] " << downloadError_;
#endif
    resetDownloadState();
    updateWhisperStatus();
    return;
  }

  // Ensure file is clean and truncated if we start a request (in case of
  // redirect leftovers)
  if (downloadFile_) {
    if (downloadFile_->isOpen()) {
      downloadFile_->close();
    }
    if (!downloadFile_->open(QIODevice::WriteOnly)) {
      downloadError_ = tr("无法创建模型文件: %1").arg(savePath);
#ifdef QT_DEBUG
      qDebug() << "[Debug] [AsrConfigDialog] " << downloadError_;
#endif
      resetDownloadState();
      updateWhisperStatus();
      return;
    }
  }

#ifdef QT_DEBUG
  qDebug() << "[Debug] [AsrConfigDialog] 开始下载模型, URL:" << url.toString();
#endif

  QNetworkRequest request(url);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::ManualRedirectPolicy);
  request.setHeader(QNetworkRequest::UserAgentHeader,
                    "Mozilla/5.0 (SubtitlesEditor)");

  reply_ = networkManager_->get(request);

  connect(reply_, &QNetworkReply::readyRead, this, [this]() {
    if (downloadFile_ && reply_) {
      int statusCode =
          reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (statusCode >= 300 && statusCode < 400) {
        return; // Skip redirect response body
      }
      downloadFile_->write(reply_->readAll());
    }
  });

  connect(
      reply_, &QNetworkReply::downloadProgress, this,
      [this](qint64 bytesRead, qint64 totalBytes) {
        if (!reply_)
          return;
        int statusCode =
            reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode >= 300 && statusCode < 400) {
          return; // Skip redirect progress
        }
        if (totalBytes > 0) {
          int percent = static_cast<int>((bytesRead * 100) / totalBytes);
          whisperProgressBar_->setValue(percent);
          double readMb = bytesRead / (1024.0 * 1024.0);
          double totalMb = totalBytes / (1024.0 * 1024.0);
          whisperStatusLabel_->show();
          whisperStatusLabel_->setText(tr("正在下载: %1% (%2 MB / %3 MB)")
                                           .arg(percent)
                                           .arg(readMb, 0, 'f', 1)
                                           .arg(totalMb, 0, 'f', 1));

          if (percent != lastReportedPercent_) {
            lastReportedPercent_ = percent;
#ifdef QT_DEBUG
            qDebug() << "[Debug] [AsrConfigDialog] 下载进度:" << percent
                     << "% (" << readMb << "MB /" << totalMb << "MB)";
#endif
          }
        }
      });

  connect(reply_, &QNetworkReply::redirected, this,
          [this, savePath, redirectCount](const QUrl &redirectUrl) {
            reply_->deleteLater();
            reply_ = nullptr;
            startDownload(redirectUrl, savePath, redirectCount + 1);
          });

  connect(reply_, &QNetworkReply::finished, this, [this, savePath]() {
    if (!reply_)
      return;

    // Check for HTTP redirect manually
    int statusCode =
        reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode >= 300 && statusCode < 400) {
      QUrl redirectUrl =
          reply_->attribute(QNetworkRequest::RedirectionTargetAttribute)
              .toUrl();
      if (redirectUrl.isValid()) {
        QUrl resolvedUrl = reply_->url().resolved(redirectUrl);
        reply_->deleteLater();
        reply_ = nullptr;
        startDownload(resolvedUrl, savePath);
        return;
      }
    }

    downloadFile_->close();

    if (reply_->error() == QNetworkReply::NoError) {
#ifdef QT_DEBUG
      qDebug() << "[Debug] [AsrConfigDialog] 模型下载成功";
#endif
    } else {
      downloadFile_->remove();
      if (reply_->error() != QNetworkReply::OperationCanceledError) {
        downloadError_ = tr("下载错误: %1").arg(reply_->errorString());
#ifdef QT_DEBUG
        qDebug() << "[Debug] [AsrConfigDialog] " << downloadError_;
#endif
      }
    }

    resetDownloadState();
    updateWhisperStatus();
  });
}

void AsrConfigDialog::resetDownloadState() {
  if (reply_) {
    reply_->deleteLater();
    reply_ = nullptr;
  }
  if (downloadFile_) {
    downloadFile_->deleteLater();
    downloadFile_ = nullptr;
  }
  isDownloading_ = false;
  btnDownload_->setText(tr("下载模型"));
  whisperProgressBar_->hide();
  whisperModelCombo_->setEnabled(true);
  whisperLangCombo_->setEnabled(true);
  whisperThreadsSpin_->setEnabled(true);
  whisperMaxLenSpin_->setEnabled(true);
}

QString AsrConfigDialog::asrProvider() const {
  return asrProviderCombo_->currentData().toString();
}

QString AsrConfigDialog::engineModelType() const {
  return engineModelTypeCombo_->currentData().toString();
}

int AsrConfigDialog::sentenceMaxLength() const {
  return sentenceMaxLengthSpin_->value();
}

bool AsrConfigDialog::speakerDiarization() const {
  return speakerDiarizationCheck_->isChecked();
}

QString AsrConfigDialog::whisperModel() const {
  return whisperModelCombo_->currentData().toString();
}

QString AsrConfigDialog::whisperLanguage() const {
  return whisperLangCombo_->currentData().toString();
}

int AsrConfigDialog::whisperThreads() const {
  return whisperThreadsSpin_->value();
}

int AsrConfigDialog::whisperMaxLen() const {
  return whisperMaxLenSpin_->value();
}

void AsrConfigDialog::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QDialog::changeEvent(event);
}

void AsrConfigDialog::retranslateUi() {
  setWindowTitle(tr("语音识别配置"));
  if (asrProvLabel_) {
    asrProvLabel_->setText(tr("识别提供商"));
  }
  if (asrProviderCombo_) {
    asrProviderCombo_->setItemText(0, tr("本地 ASR (Whisper)"));
    asrProviderCombo_->setItemText(1, tr("腾讯云 ASR"));
  }

  // Tencent
  if (engineLabel_) {
    engineLabel_->setText(tr("引擎模型类型"));
  }
  if (maxLenLabel_) {
    maxLenLabel_->setText(tr("单行字幕最大字数"));
  }
  if (speakerDiarizationLabel_) {
    speakerDiarizationLabel_->setText(tr("说话人识别"));
  }
  if (speakerDiarizationCheck_) {
    speakerDiarizationCheck_->setText(tr("开启说话人识别"));
  }

  // Whisper
  if (whisperModelLabel_) {
    whisperModelLabel_->setText(tr("语音识别模型"));
  }
  if (btnDownload_) {
    btnDownload_->setText(isDownloading_ ? tr("取消") : tr("下载模型"));
  }
  if (whisperLangLabel_) {
    whisperLangLabel_->setText(tr("识别语言"));
  }
  if (whisperLangCombo_) {
    whisperLangCombo_->setItemText(0, tr("自动检测"));
    whisperLangCombo_->setItemText(1, tr("中文"));
    whisperLangCombo_->setItemText(2, tr("英文"));
    whisperLangCombo_->setItemText(3, tr("日文"));
    whisperLangCombo_->setItemText(4, tr("韩文"));
  }
  if (whisperThreadsLabel_) {
    whisperThreadsLabel_->setText(tr("计算线程数"));
  }
  if (whisperMaxLenLabel_) {
    whisperMaxLenLabel_->setText(tr("单行字幕最大字数"));
  }

  if (engineModelTypeCombo_) {
    engineModelTypeCombo_->setItemText(
        0, QString("16k_zh_en(%1)").arg(tr("中英粤+9种方言大模型")));
    engineModelTypeCombo_->setItemText(
        1, QString("16k_zh_large(%1)").arg(tr("普方英大模型")));
    engineModelTypeCombo_->setItemText(
        2, QString("16k_multi_lang(%1)").arg(tr("多语种大模型")));
    engineModelTypeCombo_->setItemText(
        3, QString("16k_zh(%1)").arg(tr("中文普通话通用")));
    engineModelTypeCombo_->setItemText(4,
                                       QString("16k_en(%1)").arg(tr("英语")));
    engineModelTypeCombo_->setItemText(
        5, QString("16k_en_large(%1)").arg(tr("英语大模型")));
    engineModelTypeCombo_->setItemText(6,
                                       QString("16k_yue(%1)").arg(tr("粤语")));
    engineModelTypeCombo_->setItemText(
        7, QString("16k_zh-PY(%1)").arg(tr("中英粤混合")));
    engineModelTypeCombo_->setItemText(
        8, QString("16k_zh-TW(%1)").arg(tr("中文繁体")));
    engineModelTypeCombo_->setItemText(9,
                                       QString("16k_ja(%1)").arg(tr("日语")));
    engineModelTypeCombo_->setItemText(10,
                                       QString("16k_ko(%1)").arg(tr("韩语")));
    engineModelTypeCombo_->setItemText(11,
                                       QString("16k_vi(%1)").arg(tr("越南语")));
    engineModelTypeCombo_->setItemText(12,
                                       QString("16k_ms(%1)").arg(tr("马来语")));
    engineModelTypeCombo_->setItemText(
        13, QString("16k_id(%1)").arg(tr("印度尼西亚语")));
    engineModelTypeCombo_->setItemText(
        14, QString("16k_fil(%1)").arg(tr("菲律宾语")));
    engineModelTypeCombo_->setItemText(15,
                                       QString("16k_th(%1)").arg(tr("泰语")));
    engineModelTypeCombo_->setItemText(
        16, QString("16k_pt(%1)").arg(tr("葡萄牙语")));
    engineModelTypeCombo_->setItemText(
        17, QString("16k_tr(%1)").arg(tr("土耳其语")));
    engineModelTypeCombo_->setItemText(
        18, QString("16k_ar(%1)").arg(tr("阿拉伯语")));
    engineModelTypeCombo_->setItemText(
        19, QString("16k_es(%1)").arg(tr("西班牙语")));
    engineModelTypeCombo_->setItemText(20,
                                       QString("16k_hi(%1)").arg(tr("印地语")));
    engineModelTypeCombo_->setItemText(21,
                                       QString("16k_fr(%1)").arg(tr("法语")));
    engineModelTypeCombo_->setItemText(22,
                                       QString("16k_de(%1)").arg(tr("德语")));
    engineModelTypeCombo_->setItemText(
        23, QString("16k_zh_medical(%1)").arg(tr("中文医疗")));
  }

  if (btnCancel_) {
    btnCancel_->setText(tr("取消"));
  }
  if (btnOk_) {
    btnOk_->setText(tr("确定"));
  }
  updateWhisperStatus();
}
