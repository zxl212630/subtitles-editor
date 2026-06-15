#include "AsrConfigDialog.h"
#include "ConfigManager.h"
#include "AppMessageBox.h"
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QProgressDialog>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

AsrConfigDialog::AsrConfigDialog(QWidget *parent) : BaseDialog(parent) {
  setWindowTitle(tr("语音识别配置"));
  setMinimumSize(460, 440);
  resize(480, 480);

  setObjectName("AsrConfigDialog");

  setupTitleBar();
  setupUi();
  loadDefaultConfig();

  connect(btnCancel_, &QPushButton::clicked, this, &QDialog::reject);
  connect(btnOk_, &QPushButton::clicked, this, &QDialog::accept);

  setupWindowAgent(titleBar);
}

void AsrConfigDialog::setupTitleBar() {
  titleBar = new QFrame(this);
  titleBar->setFixedHeight(36);
  titleBar->setObjectName("TitleBar");

  auto *layout = new QHBoxLayout(titleBar);
  layout->setContentsMargins(12, 0, 12, 0);
  layout->setSpacing(0);

  layout->addStretch();
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
  engineModelTypeCombo_->addItem("16k_zh_en(中英粤+9种方言大模型)", "16k_zh_en");
  engineModelTypeCombo_->addItem("16k_zh_large(普方英大模型)", "16k_zh_large");
  engineModelTypeCombo_->addItem("16k_multi_lang(多语种大模型)", "16k_multi_lang");
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

  speakerDiarizationCheck_ = new QCheckBox(tr("开启说话人识别"), tencentContainer_);
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
  connect(whisperModelCombo_, &QComboBox::currentTextChanged, this, [this]() {
    updateWhisperStatus();
  });

  // Download Trigger
  connect(btnDownload_, &QPushButton::clicked, this, &AsrConfigDialog::onDownloadClicked);

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
  int lIdx = whisperLangCombo_->findData(wLang);
  if (lIdx >= 0) {
    whisperLangCombo_->setCurrentIndex(lIdx);
  }

  whisperThreadsSpin_->setValue(cfg.whisperThreads());

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
  QString provider = asrProviderCombo_->currentData().toString();
  if (provider != "local_whisper") {
    btnOk_->setEnabled(true);
    return;
  }

  QString model = whisperModel();
  bool exists = checkModelExists(model);
  if (exists) {
    whisperStatusLabel_->setText(tr("模型状态: 已下载"));
    whisperStatusLabel_->setStyleSheet("color: #10b981; font-weight: bold;");
    btnDownload_->hide();
    btnOk_->setEnabled(true);
  } else {
    whisperStatusLabel_->setText(tr("模型状态: 未下载"));
    whisperStatusLabel_->setStyleSheet("color: #ef4444; font-weight: bold;");
    btnDownload_->show();
    btnOk_->setEnabled(false);
  }
}

void AsrConfigDialog::onDownloadClicked() {
  QString modelName = whisperModel();
  QString fileName = QString("ggml-%1.bin").arg(modelName);
  
  QString saveDir = ConfigManager::instance().whisperModelPath();
  QDir().mkpath(saveDir);
  QString savePath = saveDir + "/" + fileName;

  QString urlStr = QString("https://hf-mirror.com/ggerganov/whisper.cpp/resolve/main/%1").arg(fileName);
  QUrl url(urlStr);

  QNetworkAccessManager *manager = new QNetworkAccessManager(this);
  QNetworkRequest request(url);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

  QNetworkReply *reply = manager->get(request);

  QProgressDialog *progressDlg = new QProgressDialog(this);
  progressDlg->setWindowTitle(tr("下载模型"));
  progressDlg->setLabelText(tr("正在下载模型 %1...\n来源: hf-mirror.com").arg(fileName));
  progressDlg->setCancelButtonText(tr("取消"));
  progressDlg->setRange(0, 100);
  progressDlg->setModal(true);
  progressDlg->setMinimumDuration(0);
  progressDlg->show();

  QFile *file = new QFile(savePath, this);
  if (!file->open(QIODevice::WriteOnly)) {
    AppMessageBox::critical(this, tr("下载失败"), tr("无法创建模型文件: %1").arg(savePath));
    reply->abort();
    reply->deleteLater();
    progressDlg->deleteLater();
    file->deleteLater();
    manager->deleteLater();
    return;
  }

  connect(reply, &QNetworkReply::readyRead, this, [reply, file]() {
    file->write(reply->readAll());
  });

  connect(reply, &QNetworkReply::downloadProgress, this, [progressDlg](qint64 bytesRead, qint64 totalBytes) {
    if (totalBytes > 0) {
      int percent = static_cast<int>((bytesRead * 100) / totalBytes);
      progressDlg->setValue(percent);
    }
  });

  connect(progressDlg, &QProgressDialog::canceled, this, [reply]() {
    reply->abort();
  });

  connect(reply, &QNetworkReply::finished, this, [this, reply, file, progressDlg, manager, savePath]() {
    progressDlg->close();
    file->close();
    
    if (reply->error() == QNetworkReply::NoError) {
      AppMessageBox::information(this, tr("下载成功"), tr("模型已成功下载并保存。"));
      updateWhisperStatus();
    } else {
      file->remove();
      if (reply->error() != QNetworkReply::OperationCanceledError) {
        AppMessageBox::critical(this, tr("下载失败"), tr("下载错误: %1").arg(reply->errorString()));
      }
    }
    
    reply->deleteLater();
    file->deleteLater();
    progressDlg->deleteLater();
    manager->deleteLater();
  });
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
    btnDownload_->setText(tr("下载模型"));
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

  if (engineModelTypeCombo_) {
    engineModelTypeCombo_->setItemText(0, QString("16k_zh_en(%1)").arg(tr("中英粤+9种方言大模型")));
    engineModelTypeCombo_->setItemText(1, QString("16k_zh_large(%1)").arg(tr("普方英大模型")));
    engineModelTypeCombo_->setItemText(2, QString("16k_multi_lang(%1)").arg(tr("多语种大模型")));
    engineModelTypeCombo_->setItemText(3, QString("16k_zh(%1)").arg(tr("中文普通话通用")));
    engineModelTypeCombo_->setItemText(4, QString("16k_en(%1)").arg(tr("英语")));
    engineModelTypeCombo_->setItemText(5, QString("16k_en_large(%1)").arg(tr("英语大模型")));
    engineModelTypeCombo_->setItemText(6, QString("16k_yue(%1)").arg(tr("粤语")));
    engineModelTypeCombo_->setItemText(7, QString("16k_zh-PY(%1)").arg(tr("中英粤混合")));
    engineModelTypeCombo_->setItemText(8, QString("16k_zh-TW(%1)").arg(tr("中文繁体")));
    engineModelTypeCombo_->setItemText(9, QString("16k_ja(%1)").arg(tr("日语")));
    engineModelTypeCombo_->setItemText(10, QString("16k_ko(%1)").arg(tr("韩语")));
    engineModelTypeCombo_->setItemText(11, QString("16k_vi(%1)").arg(tr("越南语")));
    engineModelTypeCombo_->setItemText(12, QString("16k_ms(%1)").arg(tr("马来语")));
    engineModelTypeCombo_->setItemText(13, QString("16k_id(%1)").arg(tr("印度尼西亚语")));
    engineModelTypeCombo_->setItemText(14, QString("16k_fil(%1)").arg(tr("菲律宾语")));
    engineModelTypeCombo_->setItemText(15, QString("16k_th(%1)").arg(tr("泰语")));
    engineModelTypeCombo_->setItemText(16, QString("16k_pt(%1)").arg(tr("葡萄牙语")));
    engineModelTypeCombo_->setItemText(17, QString("16k_tr(%1)").arg(tr("土耳其语")));
    engineModelTypeCombo_->setItemText(18, QString("16k_ar(%1)").arg(tr("阿拉伯语")));
    engineModelTypeCombo_->setItemText(19, QString("16k_es(%1)").arg(tr("西班牙语")));
    engineModelTypeCombo_->setItemText(20, QString("16k_hi(%1)").arg(tr("印地语")));
    engineModelTypeCombo_->setItemText(21, QString("16k_fr(%1)").arg(tr("法语")));
    engineModelTypeCombo_->setItemText(22, QString("16k_de(%1)").arg(tr("德语")));
    engineModelTypeCombo_->setItemText(23, QString("16k_zh_medical(%1)").arg(tr("中文医疗")));
  }

  if (btnCancel_) {
    btnCancel_->setText(tr("取消"));
  }
  if (btnOk_) {
    btnOk_->setText(tr("确定"));
  }
  updateWhisperStatus();
}
