#include "AsrConfigDialog.h"
#include "ConfigManager.h"
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QWindowKit/QWKWidgets/widgetwindowagent.h>

AsrConfigDialog::AsrConfigDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(tr("语音识别配置"));
  setMinimumSize(460, 420);
  resize(480, 440);

  setObjectName("AsrConfigDialog");

  windowAgent = new QWK::WidgetWindowAgent(this);
  windowAgent->setup(this);

  setupTitleBar();
  setupUi();
  loadDefaultConfig();

  connect(btnCancel_, &QPushButton::clicked, this, &QDialog::reject);
  connect(btnOk_, &QPushButton::clicked, this, &QDialog::accept);

  windowAgent->setTitleBar(titleBar);
}

void AsrConfigDialog::setupTitleBar() {
  titleBar = new QFrame(this);
  titleBar->setFixedHeight(36);
  titleBar->setObjectName("TitleBar");

  auto *layout = new QHBoxLayout(titleBar);
  layout->setContentsMargins(12, 0, 12, 0);
  layout->setSpacing(0);

  layout->addStretch();

  titleLabel = new QLabel(tr("语音识别配置"), titleBar);
  titleLabel->setObjectName("ConfigTitleLeftLabel");
  layout->addWidget(titleLabel);

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
  contentLayout->setContentsMargins(30, 20, 30, 20);
  contentLayout->setSpacing(15);

  // Engine Model Type
  engineLabel_ = new QLabel(tr("引擎模型类型"), contentWidget);
  engineLabel_->setObjectName("ConfigFieldLabel");
  contentLayout->addWidget(engineLabel_);

  engineModelTypeCombo_ = new QComboBox(contentWidget);
  engineModelTypeCombo_->setFixedHeight(32);
  engineModelTypeCombo_->setObjectName("ConfigComboBox");

  // Add models and their data mapping (exact matches to ConfigDialog)
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
  contentLayout->addWidget(engineModelTypeCombo_);

  // Max Sentence Length
  maxLenLabel_ = new QLabel(tr("单行字幕最大字数"), contentWidget);
  maxLenLabel_->setObjectName("ConfigFieldLabel");
  contentLayout->addWidget(maxLenLabel_);

  sentenceMaxLengthSpin_ = new QSpinBox(contentWidget);
  sentenceMaxLengthSpin_->setFixedHeight(32);
  sentenceMaxLengthSpin_->setRange(6, 40);
  sentenceMaxLengthSpin_->setObjectName("ConfigSpinBox");
  contentLayout->addWidget(sentenceMaxLengthSpin_);

  // Speaker Diarization
  speakerDiarizationLabel_ = new QLabel(tr("说话人识别"), contentWidget);
  speakerDiarizationLabel_->setObjectName("ConfigFieldLabel");
  contentLayout->addWidget(speakerDiarizationLabel_);

  speakerDiarizationCheck_ = new QCheckBox(tr("开启说话人识别"), contentWidget);
  speakerDiarizationCheck_->setFixedHeight(32);
  speakerDiarizationCheck_->setObjectName("ConfigCheckBox");
  contentLayout->addWidget(speakerDiarizationCheck_);

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

  retranslateUi();
}

void AsrConfigDialog::loadDefaultConfig() {
  auto &cfg = ConfigManager::instance();
  sentenceMaxLengthSpin_->setValue(cfg.sentenceMaxLength());
  speakerDiarizationCheck_->setChecked(cfg.speakerDiarization());

  QString model = cfg.engineModelType();
  int idx = engineModelTypeCombo_->findData(model);
  if (idx >= 0) {
    engineModelTypeCombo_->setCurrentIndex(idx);
  }
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

void AsrConfigDialog::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QDialog::changeEvent(event);
}

void AsrConfigDialog::retranslateUi() {
  setWindowTitle(tr("语音识别配置"));
  if (titleLabel) {
    titleLabel->setText(tr("语音识别配置"));
  }

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
}
