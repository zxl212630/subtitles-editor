#include "VideoPropertyDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

VideoPropertyDialog::VideoPropertyDialog(
    const QMap<QString, QString> &properties, QWidget *parent)
    : QDialog(parent), m_properties(properties) {
  setupUi();
}

void VideoPropertyDialog::setupUi() {
  setWindowTitle("视频属性");
  setMinimumSize(400, 300);
  resize(480, 360);
  setStyleSheet("QDialog { background-color: #1e1e1e; }");

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(20, 20, 20, 16);
  mainLayout->setSpacing(16);

  // Title
  auto *titleLabel = new QLabel("视频属性", this);
  titleLabel->setStyleSheet(R"(
        QLabel {
            color: #d1d5db;
            font-size: 14px;
            font-weight: bold;
            background: transparent;
        }
    )");
  mainLayout->addWidget(titleLabel);

  // Divider
  auto *divider = new QWidget(this);
  divider->setFixedHeight(1);
  divider->setStyleSheet("background-color: #333333;");
  mainLayout->addWidget(divider);

  // Scroll area for properties
  auto *scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");

  auto *scrollContent = new QWidget(scrollArea);
  auto *scrollLayout = new QVBoxLayout(scrollContent);
  scrollLayout->setContentsMargins(0, 0, 0, 0);
  scrollLayout->setSpacing(10);
  scrollLayout->setAlignment(Qt::AlignTop);

  for (auto it = m_properties.constBegin(); it != m_properties.constEnd();
       ++it) {
    auto *row = new QWidget(scrollContent);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(12);

    auto *keyLabel = new QLabel(it.key(), row);
    keyLabel->setFixedWidth(100);
    keyLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    keyLabel->setStyleSheet(R"(
            QLabel {
                color: #9ca3af;
                font-size: 13px;
                background: transparent;
            }
        )");

    auto *valueLabel = new QLabel(it.value(), row);
    valueLabel->setWordWrap(true);
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    valueLabel->setStyleSheet(R"(
            QLabel {
                color: #d1d5db;
                font-size: 13px;
                background: transparent;
            }
        )");

    rowLayout->addWidget(keyLabel);
    rowLayout->addWidget(valueLabel, 1);
    scrollLayout->addWidget(row);
  }

  scrollLayout->addStretch();
  scrollArea->setWidget(scrollContent);
  mainLayout->addWidget(scrollArea, 1);

  // Button box
  auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, this);
  buttonBox->setStyleSheet(R"(
        QDialogButtonBox {
            background: transparent;
        }
        QPushButton {
            background-color: #333333;
            color: #d1d5db;
            border: 1px solid #444444;
            border-radius: 4px;
            padding: 6px 18px;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #444444;
        }
        QPushButton:pressed {
            background-color: #555555;
        }
    )");
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  mainLayout->addWidget(buttonBox);
}
