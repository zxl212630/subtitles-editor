#include "AppMessageBox.h"

#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

// ── Static convenience methods ──

int AppMessageBox::warning(QWidget *parent, const QString &title,
                           const QString &text, StandardButtons buttons,
                           StandardButton defaultButton) {
  AppMessageBox box(Warning, title, text, buttons, defaultButton, parent);
  return box.exec();
}

int AppMessageBox::question(QWidget *parent, const QString &title,
                            const QString &text, StandardButtons buttons,
                            StandardButton defaultButton) {
  AppMessageBox box(Question, title, text, buttons, defaultButton, parent);
  return box.exec();
}

int AppMessageBox::critical(QWidget *parent, const QString &title,
                            const QString &text, StandardButtons buttons,
                            StandardButton defaultButton) {
  AppMessageBox box(Critical, title, text, buttons, defaultButton, parent);
  return box.exec();
}

int AppMessageBox::information(QWidget *parent, const QString &title,
                               const QString &text, StandardButtons buttons,
                               StandardButton defaultButton) {
  AppMessageBox box(Information, title, text, buttons, defaultButton, parent);
  return box.exec();
}

// ── Constructor ──

AppMessageBox::AppMessageBox(Icon icon, const QString &title,
                             const QString &text, StandardButtons buttons,
                             StandardButton defaultButton, QWidget *parent)
    : BaseDialog(parent), defaultButton_(defaultButton) {
  setObjectName("AppMessageBox");
  setWindowTitle(title);
  setMinimumWidth(380);

  setupTitleBar();
  setupContent();

  // Set icon
  switch (icon) {
  case Warning:
    iconLabel_->setText("⚠️");
    break;
  case Question:
    iconLabel_->setText("❓");
    break;
  case Critical:
    iconLabel_->setText("❌");
    break;
  case Information:
    iconLabel_->setText("ℹ️");
    break;
  case NoIcon:
    iconLabel_->hide();
    break;
  }

  // Set message text
  textLabel_->setText(text);

  // Setup buttons based on flags
  if (buttons & Ok) {
    okBtn_ = new QPushButton(tr("确定"), this);
    okBtn_->setObjectName("MessageBoxOkBtn");
    okBtn_->setMinimumWidth(70);
    connect(okBtn_, &QPushButton::clicked, this,
            [this]() { onButtonClicked(Ok); });
  }
  if (buttons & Yes) {
    yesBtn_ = new QPushButton(tr("是"), this);
    yesBtn_->setObjectName("MessageBoxYesBtn");
    yesBtn_->setMinimumWidth(70);
    connect(yesBtn_, &QPushButton::clicked, this,
            [this]() { onButtonClicked(Yes); });
  }
  if (buttons & No) {
    noBtn_ = new QPushButton(tr("否"), this);
    noBtn_->setObjectName("MessageBoxNoBtn");
    noBtn_->setMinimumWidth(70);
    connect(noBtn_, &QPushButton::clicked, this,
            [this]() { onButtonClicked(No); });
  }
  if (buttons & Cancel) {
    cancelBtn_ = new QPushButton(tr("取消"), this);
    cancelBtn_->setObjectName("MessageBoxCancelBtn");
    cancelBtn_->setMinimumWidth(70);
    connect(cancelBtn_, &QPushButton::clicked, this,
            [this]() { onButtonClicked(Cancel); });
  }
  if (buttons & OpenFolder) {
    openFolderBtn_ = new QPushButton(tr("打开所在文件夹"), this);
    openFolderBtn_->setObjectName("MessageBoxOpenFolderBtn");
    openFolderBtn_->setMinimumWidth(120);
    connect(openFolderBtn_, &QPushButton::clicked, this,
            [this]() { onButtonClicked(OpenFolder); });
  }

  // Set default button focus
  QPushButton *defaultBtn = nullptr;
  if (defaultButton == Ok && okBtn_)
    defaultBtn = okBtn_;
  else if (defaultButton == Yes && yesBtn_)
    defaultBtn = yesBtn_;
  else if (defaultButton == No && noBtn_)
    defaultBtn = noBtn_;
  else if (defaultButton == Cancel && cancelBtn_)
    defaultBtn = cancelBtn_;
  else if (defaultButton == OpenFolder && openFolderBtn_)
    defaultBtn = openFolderBtn_;

  if (defaultBtn) {
    defaultBtn->setDefault(true);
    defaultBtn->setFocus();
  }
  // Setup footer after buttons are created
  setupFooter();

  setupWindowAgent(titleBar);

  // Fix size to disable maximize button on macOS
  adjustSize();
  setFixedSize(size());
}

// ── Setup methods ──

void AppMessageBox::setupContent() {
  mainLayout_ = new QVBoxLayout(this);
  mainLayout_->setContentsMargins(0, 0, 0, 0);
  mainLayout_->setSpacing(0);

  mainLayout_->addWidget(titleBar);

  auto *content = new QWidget(this);
  content->setObjectName("MessageBoxContent");
  auto *contentLayout = new QHBoxLayout(content);
  contentLayout->setContentsMargins(24, 24, 24, 24);
  contentLayout->setSpacing(16);

  // Icon
  iconLabel_ = new QLabel(content);
  iconLabel_->setObjectName("MessageBoxIcon");
  iconLabel_->setFixedSize(48, 48);
  iconLabel_->setAlignment(Qt::AlignCenter);
  iconLabel_->setStyleSheet("font-size: 36px;");
  contentLayout->addWidget(iconLabel_, 0, Qt::AlignTop);

  // Message text
  textLabel_ = new QLabel(content);
  textLabel_->setObjectName("MessageBoxText");
  textLabel_->setWordWrap(true);
  textLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  contentLayout->addWidget(textLabel_, 1);

  mainLayout_->addWidget(content);
}

void AppMessageBox::setupFooter() {
  if (!mainLayout_)
    return;

  auto *footer = new QWidget(this);
  footer->setObjectName("MessageBoxFooter");
  footer->setFixedHeight(56);
  auto *footerLayout = new QHBoxLayout(footer);
  footerLayout->setContentsMargins(20, 0, 20, 0);
  footerLayout->setSpacing(8);

  footerLayout->addStretch();

  // Buttons in order: Cancel, No, Yes, Ok (right-aligned)
  if (cancelBtn_)
    footerLayout->addWidget(cancelBtn_);
  if (noBtn_)
    footerLayout->addWidget(noBtn_);
  if (yesBtn_)
    footerLayout->addWidget(yesBtn_);
  if (openFolderBtn_)
    footerLayout->addWidget(openFolderBtn_);
  if (okBtn_)
    footerLayout->addWidget(okBtn_);

  mainLayout_->addWidget(footer);
}

// ── Button handler ──

void AppMessageBox::onButtonClicked(StandardButton button) {
  clickedButton_ = button;
  done(button);
}

void AppMessageBox::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    if (okBtn_)
      okBtn_->setText(tr("确定"));
    if (yesBtn_)
      yesBtn_->setText(tr("是"));
    if (noBtn_)
      noBtn_->setText(tr("否"));
    if (cancelBtn_)
      cancelBtn_->setText(tr("取消"));
    if (openFolderBtn_)
      openFolderBtn_->setText(tr("打开所在文件夹"));
  }
  BaseDialog::changeEvent(event);
}
