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
    : BaseDialog(parent), m_sections(sections) {
  setObjectName("VideoPropertyDialog");
  setMinimumSize(480, 560);
  resize(520, 640);

  setupTitleBar();
  setupUi();

  setupWindowAgent(titleBar);
}

void VideoPropertyDialog::setupUi() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  mainLayout->addWidget(titleBar);

  auto *contentWidget = new QWidget(this);
  contentWidget->setObjectName("ConfigContentWidget");
  auto *contentLayout = new QVBoxLayout(contentWidget);
  contentLayout->setContentsMargins(30, 20, 30, 20);
  contentLayout->setSpacing(0);

  // Scroll area for sections
  auto *scrollArea = new QScrollArea(contentWidget);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setObjectName("PropertyScrollArea");

  auto *scrollContent = new QWidget(scrollArea);
  scrollContent->setObjectName("PropertyScrollContent");
  auto *scrollLayout = new QVBoxLayout(scrollContent);
  scrollLayout->setContentsMargins(0, 0, 10, 0);
  scrollLayout->setSpacing(20);
  scrollLayout->setAlignment(Qt::AlignTop);

  for (const auto &section : m_sections) {
    // Section title
    auto *sectionHeader = new QWidget(scrollContent);
    auto *headerLayout = new QVBoxLayout(sectionHeader);
    headerLayout->setContentsMargins(0, 10, 0, 5);
    headerLayout->setSpacing(8);

    auto *sectionTitle = new QLabel(section.first, sectionHeader);
    sectionTitle->setObjectName("ConfigFieldLabel");
    headerLayout->addWidget(sectionTitle);

    // Section divider
    auto *sectionDivider = new QFrame(sectionHeader);
    sectionDivider->setObjectName("SectionDivider");
    sectionDivider->setFixedHeight(1);
    sectionDivider->setStyleSheet(
        "background-color: rgba(255, 255, 255, 0.1);");
    headerLayout->addWidget(sectionDivider);

    scrollLayout->addWidget(sectionHeader);

    // Properties in this section
    auto *propertiesGrid = new QWidget(scrollContent);
    auto *gridLayout = new QVBoxLayout(propertiesGrid);
    gridLayout->setContentsMargins(0, 5, 0, 5);
    gridLayout->setSpacing(12);

    for (auto it = section.second.constBegin(); it != section.second.constEnd();
         ++it) {
      auto *row = new QWidget(propertiesGrid);
      auto *rowLayout = new QHBoxLayout(row);
      rowLayout->setContentsMargins(0, 0, 0, 0);
      rowLayout->setSpacing(16);

      auto *keyLabel = new QLabel(it.key(), row);
      keyLabel->setFixedWidth(120);
      keyLabel->setObjectName("PropertyKeyLabel");
      keyLabel->setStyleSheet("color: rgba(255, 255, 255, 0.5);");
      keyLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

      auto *valueLabel = new QLabel(it.value(), row);
      valueLabel->setObjectName("PropertyValueLabel");
      valueLabel->setWordWrap(true);
      valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
      valueLabel->setContextMenuPolicy(Qt::NoContextMenu);

      rowLayout->addWidget(keyLabel);
      rowLayout->addWidget(valueLabel, 1);
      gridLayout->addWidget(row);
    }
    scrollLayout->addWidget(propertiesGrid);
  }

  scrollLayout->addStretch();
  scrollArea->setWidget(scrollContent);
  contentLayout->addWidget(scrollArea);
  mainLayout->addWidget(contentWidget, 1);

  // Footer
  auto *footer = new QWidget(this);
  footer->setObjectName("ConfigFooter");
  footer->setFixedHeight(60);
  auto *footerLayout = new QHBoxLayout(footer);
  footerLayout->setContentsMargins(20, 0, 20, 0);

  footerLayout->addStretch();

  auto *btnOk = new QPushButton(tr("确定"), footer);
  btnOk->setObjectName("ConfigOkButton");
  btnOk->setFixedWidth(100);
  connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);

  footerLayout->addWidget(btnOk);
  mainLayout->addWidget(footer);
}
