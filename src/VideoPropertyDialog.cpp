#include "VideoPropertyDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

VideoPropertyDialog::VideoPropertyDialog(const QList<Section> &sections,
                                         QWidget *parent)
    : QDialog(parent), m_sections(sections) {
  setupUi();
}

void VideoPropertyDialog::setupUi() {
  setWindowTitle("属性");
  setMinimumSize(420, 400);
  resize(520, 600);

  // Apply dark background to dialog and all child widgets
  setStyleSheet(R"(
        QDialog {
            background-color: #1e1e1e;
        }
        QWidget {
            background-color: #1e1e1e;
        }
        QScrollArea {
            background-color: #1e1e1e;
            border: none;
        }
        QScrollBar:vertical {
            background: #2a2a2a;
            width: 8px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background: #4a4a4a;
            border-radius: 4px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: #5a5a5a;
        }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )");

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(24, 20, 24, 20);
  mainLayout->setSpacing(0);

  // Scroll area for sections
  auto *scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setStyleSheet(
      "QScrollArea { background: transparent; border: none; }");

  auto *scrollContent = new QWidget(scrollArea);
  auto *scrollLayout = new QVBoxLayout(scrollContent);
  scrollLayout->setContentsMargins(0, 0, 12, 0);
  scrollLayout->setSpacing(20);
  scrollLayout->setAlignment(Qt::AlignTop);

  for (const auto &section : m_sections) {
    // Section title
    auto *sectionTitle = new QLabel(section.first, scrollContent);
    sectionTitle->setStyleSheet(R"(
            QLabel {
                color: #9ca3af;
                font-size: 13px;
                font-weight: bold;
                background: transparent;
                padding-top: 8px;
            }
        )");
    scrollLayout->addWidget(sectionTitle);

    // Section divider
    auto *sectionDivider = new QWidget(scrollContent);
    sectionDivider->setFixedHeight(1);
    sectionDivider->setStyleSheet("background-color: #333333;");
    scrollLayout->addWidget(sectionDivider);

    // Properties in this section
    for (auto it = section.second.constBegin(); it != section.second.constEnd();
         ++it) {
      auto *row = new QWidget(scrollContent);
      auto *rowLayout = new QHBoxLayout(row);
      rowLayout->setContentsMargins(0, 4, 0, 4);
      rowLayout->setSpacing(16);

      auto *keyLabel = new QLabel(it.key() + ":", row);
      keyLabel->setFixedWidth(100);
      keyLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
      keyLabel->setStyleSheet(R"(
                QLabel {
                    color: #9ca3af;
                    font-size: 13px;
                    background: transparent;
                }
            )");

      auto *valueContainer = new QWidget(row);
      valueContainer->setStyleSheet(
          "background-color: #2a2a2a; border-radius: 4px;");
      auto *valueLayout = new QHBoxLayout(valueContainer);
      valueLayout->setContentsMargins(10, 6, 10, 6);
      valueLayout->setSpacing(0);

      auto *valueLabel = new QLabel(it.value(), valueContainer);
      valueLabel->setWordWrap(true);
      valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
      valueLabel->setContextMenuPolicy(Qt::NoContextMenu);
      valueLabel->setStyleSheet(R"(
                QLabel {
                    color: #d1d5db;
                    font-size: 13px;
                    background: transparent;
                }
            )");
      valueLayout->addWidget(valueLabel);

      rowLayout->addWidget(keyLabel);
      rowLayout->addWidget(valueContainer, 1);
      scrollLayout->addWidget(row);
    }
  }

  scrollLayout->addStretch();
  scrollArea->setWidget(scrollContent);
  mainLayout->addWidget(scrollArea, 1);

  // Spacer between content and button
  mainLayout->addSpacing(16);

  // Button box
  auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, this);
  buttonBox->setStyleSheet(R"(
        QDialogButtonBox {
            background: transparent;
        }
        QPushButton {
            background-color: #0284c7;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 8px 32px;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #0369a1;
        }
        QPushButton:pressed {
            background-color: #075985;
        }
    )");
  if (QPushButton *okBtn = buttonBox->button(QDialogButtonBox::Ok)) {
    okBtn->setText("确定");
  }
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  mainLayout->addWidget(buttonBox, 0, Qt::AlignCenter);
}
