#include "BaseDialog.h"
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QWindowKit/QWKWidgets/widgetwindowagent.h>

BaseDialog::BaseDialog(QWidget *parent) : QDialog(parent) {}

BaseDialog::~BaseDialog() {}

void BaseDialog::setupWindowAgent(QFrame *customTitleBar) {
  titleBar = customTitleBar;
  windowAgent = new QWK::WidgetWindowAgent(this);
  windowAgent->setup(this);
  windowAgent->setTitleBar(titleBar);

  // 创建物理上绝对居中的标题 Label
  titleLabel = new QLabel(titleBar);
  titleLabel->setObjectName("ConfigTitleLeftLabel");
  titleLabel->setAlignment(Qt::AlignCenter);
  titleLabel->setText(windowTitle());
  titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  titleLabel->setGeometry(0, 0, titleBar->width(), titleBar->height());
  titleLabel->lower(); // 确保不阻挡底层按钮或菜单的交互

  titleBar->installEventFilter(this);

#ifdef Q_OS_WIN
  // Add a small 16x16 icon to the top-left of the dialog title bar
  if (titleBar->layout()) {
    auto *iconLabel = new QLabel(titleBar);
    iconLabel->setFixedSize(16, 16);
    iconLabel->setScaledContents(true);
    iconLabel->setPixmap(QIcon(":/icon.png").pixmap(16, 16));

    QWidget *leftContainer = titleBar->findChild<QWidget *>("TitleLeftContainer");
    if (leftContainer && leftContainer->layout()) {
      if (auto *leftBoxLayout = qobject_cast<QHBoxLayout *>(leftContainer->layout())) {
        leftBoxLayout->insertWidget(0, iconLabel);
        leftBoxLayout->setSpacing(8);
      }
    } else {
      if (auto *boxLayout = qobject_cast<QHBoxLayout *>(titleBar->layout())) {
        boxLayout->insertWidget(0, iconLabel);
      }
    }
  }

  // 创建系统按钮容器（使最小化、最大化、关闭按钮紧凑排列）
  auto *sysBtnContainer = new QWidget(titleBar);
  sysBtnContainer->setObjectName("TitleBarSystemButtonsContainer");
  auto *sysBtnLayout = new QHBoxLayout(sysBtnContainer);
  sysBtnLayout->setContentsMargins(0, 0, 0, 0);
  sysBtnLayout->setSpacing(0);

  minBtn = new QPushButton(sysBtnContainer);
  maxBtn = new QPushButton(sysBtnContainer);
  closeBtn = new QPushButton(sysBtnContainer);

  minBtn->setObjectName("TitleBarMinBtn");
  maxBtn->setObjectName("TitleBarMaxBtn");
  closeBtn->setObjectName("TitleBarCloseBtn");

  minBtn->setFixedSize(46, 36);
  maxBtn->setFixedSize(46, 36);
  closeBtn->setFixedSize(46, 36);

  minBtn->setIcon(QIcon(":/icons/minimize.svg"));
  maxBtn->setIcon(QIcon(":/icons/maximize.svg"));
  closeBtn->setIcon(QIcon(":/icons/close.svg"));

  minBtn->setIconSize(QSize(16, 16));
  maxBtn->setIconSize(QSize(16, 16));
  closeBtn->setIconSize(QSize(16, 16));

  minBtn->setToolTip(tr("最小化"));
  maxBtn->setToolTip(tr("最大化"));
  closeBtn->setToolTip(tr("关闭"));

  // 若对话框不支持最大化，则禁用最大化按钮
  if (!(windowFlags() & Qt::WindowMaximizeButtonHint)) {
    maxBtn->setEnabled(false);
  }

  sysBtnLayout->addWidget(minBtn);
  sysBtnLayout->addWidget(maxBtn);
  sysBtnLayout->addWidget(closeBtn);

  // 注入标题栏布局的最右端
  QWidget *rightContainer = titleBar->findChild<QWidget *>("TitleRightContainer");
  if (rightContainer && rightContainer->layout()) {
    if (auto *rightBoxLayout = qobject_cast<QHBoxLayout *>(rightContainer->layout())) {
      int left, top, right, bottom;
      rightBoxLayout->getContentsMargins(&left, &top, &right, &bottom);
      // 清空右边距使关闭按钮能贴紧右侧边界
      rightBoxLayout->setContentsMargins(left, top, 0, bottom);
      sysBtnContainer->setParent(rightContainer);
      rightBoxLayout->addWidget(sysBtnContainer);
    }
  } else if (titleBar->layout()) {
    if (auto *boxLayout = qobject_cast<QHBoxLayout *>(titleBar->layout())) {
      int left, top, right, bottom;
      boxLayout->getContentsMargins(&left, &top, &right, &bottom);
      // 清空右边距使关闭按钮能贴紧右侧边界
      boxLayout->setContentsMargins(left, top, 0, bottom);
    }
    titleBar->layout()->addWidget(sysBtnContainer);
  }

  // 注册系统按钮到 QWindowKit
  windowAgent->setSystemButton(QWK::WindowAgentBase::Minimize, minBtn);
  windowAgent->setSystemButton(QWK::WindowAgentBase::Maximize, maxBtn);
  windowAgent->setSystemButton(QWK::WindowAgentBase::Close, closeBtn);

  // 绑定点击事件
  connect(minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
  connect(maxBtn, &QPushButton::clicked, this, [this]() {
    if (isMaximized()) {
      showNormal();
    } else {
      showMaximized();
    }
  });
  connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
#endif
}

void BaseDialog::changeEvent(QEvent *event) {
  if (event->type() == QEvent::WindowTitleChange) {
    if (titleLabel) {
      titleLabel->setText(windowTitle());
    }
  }
#ifdef Q_OS_WIN
  if (event->type() == QEvent::WindowStateChange) {
    if (maxBtn) {
      if (isMaximized()) {
        maxBtn->setIcon(QIcon(":/icons/restore.svg"));
        maxBtn->setToolTip(tr("向下还原"));
      } else {
        maxBtn->setIcon(QIcon(":/icons/maximize.svg"));
        maxBtn->setToolTip(tr("最大化"));
      }
    }
  }
#endif
  QDialog::changeEvent(event);
}

bool BaseDialog::eventFilter(QObject *obj, QEvent *event) {
  if (obj == titleBar && event->type() == QEvent::Resize) {
    if (titleLabel) {
      titleLabel->setGeometry(0, 0, titleBar->width(), titleBar->height());
    }
  }
  return QDialog::eventFilter(obj, event);
}
