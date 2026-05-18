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
auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(24, 20, 24, 20);
  mainLayout->setSpacing(0);

  // Scroll area for sections
  auto *scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
auto *scrollContent = new QWidget(scrollArea);
  auto *scrollLayout = new QVBoxLayout(scrollContent);
  scrollLayout->setContentsMargins(0, 0, 12, 0);
  scrollLayout->setSpacing(20);
  scrollLayout->setAlignment(Qt::AlignTop);

  for (const auto &section : m_sections) {
    // Section title
    auto *sectionTitle = new QLabel(section.first, scrollContent);
scrollLayout->addWidget(sectionTitle);

    // Section divider
    auto *sectionDivider = new QWidget(scrollContent);
    sectionDivider->setFixedHeight(1);
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
auto *valueContainer = new QWidget(row);
auto *valueLayout = new QHBoxLayout(valueContainer);
      valueLayout->setContentsMargins(10, 6, 10, 6);
      valueLayout->setSpacing(0);

      auto *valueLabel = new QLabel(it.value(), valueContainer);
      valueLabel->setWordWrap(true);
      valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
      valueLabel->setContextMenuPolicy(Qt::NoContextMenu);
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
if (QPushButton *okBtn = buttonBox->button(QDialogButtonBox::Ok)) {
    okBtn->setText("确定");
  }
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  mainLayout->addWidget(buttonBox, 0, Qt::AlignCenter);
}
