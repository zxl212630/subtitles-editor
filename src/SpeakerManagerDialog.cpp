#include "SpeakerManagerDialog.h"
#include "ThemeManager.h"
#include "TranslationManager.h"
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QStyleOptionViewItem>
#include <QVBoxLayout>
#include <QWindowKit/QWKWidgets/widgetwindowagent.h>
#include <algorithm>

SpeakerManagerDialog::SpeakerManagerDialog(SubtitleTrack *track,
                                           QWidget *parent)
    : QDialog(parent), track_(track) {
  setMinimumSize(720, 520);
  setObjectName("SpeakerManagerDialog");

  windowAgent = new QWK::WidgetWindowAgent(this);
  windowAgent->setup(this);

  setupTitleBar();
  setupUi();
  retranslateUi();
  loadSettings();

  // Connect actions
  connect(browseFolderBtn_, &QPushButton::clicked, this,
          &SpeakerManagerDialog::onBrowseFolder);
  connect(addBtn_, &QPushButton::clicked, this,
          &SpeakerManagerDialog::onAddSpeaker);
  connect(removeBtn_, &QPushButton::clicked, this,
          &SpeakerManagerDialog::onRemoveSpeaker);
  connect(saveBtn_, &QPushButton::clicked, this,
          &SpeakerManagerDialog::onSaveAndApply);
  connect(cancelBtn_, &QPushButton::clicked, this,
          &SpeakerManagerDialog::onCancel);

  connect(speakerList_, &QListWidget::itemSelectionChanged, this,
          &SpeakerManagerDialog::onSpeakerSelectionChanged);
  connect(nameEdit_, &QLineEdit::textChanged, this,
          [this](const QString &text) { onNameChanged(text); });
  connect(imageCombo_, &QComboBox::currentTextChanged, this,
          [this](const QString &text) { onImageFileChanged(text); });
  connect(drawModeCombo_, &QComboBox::currentIndexChanged, this,
          [this](int index) { onDrawModeChanged(index); });

  auto marginSpins = {marginLeftSpin_, marginRightSpin_, marginTopSpin_,
                      marginBottomSpin_};
  for (auto *spin : marginSpins) {
    connect(spin, &QSpinBox::valueChanged, this,
            [this](int value) { onMarginChanged(value); });
  }

  // Connect theme and language change
  connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
          &SpeakerManagerDialog::updateTheme);
  connect(&TranslationManager::instance(), &TranslationManager::languageChanged,
          this, [this]() { retranslateUi(); });

  windowAgent->setTitleBar(titleBar);
}

SpeakerManagerDialog::~SpeakerManagerDialog() = default;

void SpeakerManagerDialog::setupTitleBar() {
  titleBar = new QFrame(this);
  titleBar->setFixedHeight(36);
  titleBar->setObjectName("TitleBar");

  auto *layout = new QHBoxLayout(titleBar);
  layout->setContentsMargins(80, 0, 12, 0); // 留出 macOS 三色球位置
  layout->setSpacing(0);

  titleLabel = new QLabel(titleBar);
  titleLabel->setObjectName("ConfigTitleLeftLabel");
  layout->addWidget(titleLabel);
  layout->addStretch();
}

void SpeakerManagerDialog::setupUi() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  mainLayout->addWidget(titleBar);

  // Content area widget
  auto *contentWidget = new QWidget(this);
  contentWidget->setObjectName("ConfigContentWidget");
  auto *contentLayout = new QVBoxLayout(contentWidget);
  contentLayout->setContentsMargins(20, 15, 20, 15);
  contentLayout->setSpacing(15);

  // 1. 统一设置区（顶部）
  auto *topSettingsFrame = new QFrame(contentWidget);
  topSettingsFrame->setObjectName("TopSettingsFrame");
  topSettingsFrame->setStyleSheet(
      "QFrame#TopSettingsFrame {"
      "  border: 1px solid rgba(255, 255, 255, 0.08);"
      "  border-radius: 8px;"
      "  background-color: rgba(255, 255, 255, 0.02);"
      "  padding: 10px;"
      "}");
  auto *topLayout = new QVBoxLayout(topSettingsFrame);
  topLayout->setContentsMargins(8, 8, 8, 8);
  topLayout->setSpacing(10);

  // 1.1 背景文件夹
  auto *folderRow = new QHBoxLayout();
  folderLabel_ = new QLabel(topSettingsFrame);
  folderLabel_->setObjectName("ConfigFieldLabel");
  folderLabel_->setFixedWidth(100);

  auto *editBtnRow = new QHBoxLayout();
  editBtnRow->setContentsMargins(0, 0, 0, 0);
  editBtnRow->setSpacing(0);

  bgFolderEdit_ = new QLineEdit(topSettingsFrame);
  bgFolderEdit_->setObjectName("SpeakerFolderEdit");
  bgFolderEdit_->setReadOnly(true);

  browseFolderBtn_ = new QPushButton(topSettingsFrame);
  browseFolderBtn_->setObjectName("SpeakerBrowseButton");
  browseFolderBtn_->setFixedWidth(80);

  editBtnRow->addWidget(bgFolderEdit_, 1);
  editBtnRow->addWidget(browseFolderBtn_);

  folderRow->addWidget(folderLabel_);
  folderRow->addLayout(editBtnRow, 1);
  topLayout->addLayout(folderRow);

  // 1.2 九宫格四向边距
  auto *marginRow = new QHBoxLayout();
  marginLabel_ = new QLabel(topSettingsFrame);
  marginLabel_->setObjectName("ConfigFieldLabel");
  marginLabel_->setFixedWidth(100);
  marginRow->addWidget(marginLabel_);

  auto createMarginSpin = [this, topSettingsFrame](QLabel *&labelField,
                                                   QSpinBox *&spinField) {
    auto *layout = new QHBoxLayout();
    labelField = new QLabel(topSettingsFrame);
    labelField->setStyleSheet("color: rgba(255, 255, 255, 0.6);");
    spinField = new QSpinBox(topSettingsFrame);
    spinField->setRange(0, 200);
    spinField->setFixedWidth(80);
    layout->addWidget(labelField);
    layout->addWidget(spinField);
    return layout;
  };

  auto *leftLayout = createMarginSpin(marginLeftLabel_, marginLeftSpin_);
  marginRow->addLayout(leftLayout);
  marginRow->addSpacing(10);

  auto *rightLayout = createMarginSpin(marginRightLabel_, marginRightSpin_);
  marginRow->addLayout(rightLayout);
  marginRow->addSpacing(10);

  auto *topLayout2 = createMarginSpin(marginTopLabel_, marginTopSpin_);
  marginRow->addLayout(topLayout2);
  marginRow->addSpacing(10);

  auto *bottomLayout = createMarginSpin(marginBottomLabel_, marginBottomSpin_);
  marginRow->addLayout(bottomLayout);

  marginRow->addStretch();
  topLayout->addLayout(marginRow);

  contentLayout->addWidget(topSettingsFrame);

  // 2. 中间分栏（左列表，右属性）
  auto *splitLayout = new QHBoxLayout();
  splitLayout->setSpacing(20);

  // 2.1 左侧列表区
  auto *leftWidget = new QWidget(contentWidget);
  leftWidget->setFixedWidth(240);
  auto *leftListLayout = new QVBoxLayout(leftWidget);
  leftListLayout->setContentsMargins(0, 0, 0, 0);
  leftListLayout->setSpacing(8);

  listLabel_ = new QLabel(leftWidget);
  listLabel_->setObjectName("ConfigFieldLabel");
  leftListLayout->addWidget(listLabel_);

  speakerList_ = new QListWidget(leftWidget);
  speakerList_->setObjectName("ConfigSidebar"); // 复用 ConfigSidebar 的好看样式
  leftListLayout->addWidget(speakerList_, 1);

  auto *btnRow = new QHBoxLayout();
  btnRow->setSpacing(8);
  addBtn_ = new QPushButton(leftWidget);
  addBtn_->setObjectName("SpeakerAddButton");
  removeBtn_ = new QPushButton(leftWidget);
  removeBtn_->setObjectName("SpeakerRemoveButton");
  btnRow->addWidget(addBtn_);
  btnRow->addWidget(removeBtn_);
  leftListLayout->addLayout(btnRow);

  splitLayout->addWidget(leftWidget);

  // 2.2 右侧属性区
  propertiesWidget_ = new QWidget(contentWidget);
  auto *rightLayoutProperties = new QVBoxLayout(propertiesWidget_);
  rightLayoutProperties->setContentsMargins(0, 0, 0, 0);
  rightLayoutProperties->setSpacing(10);

  // 说话人名称
  nameLabel_ = new QLabel(propertiesWidget_);
  nameLabel_->setObjectName("ConfigFieldLabel");
  rightLayoutProperties->addWidget(nameLabel_);
  nameEdit_ = new QLineEdit(propertiesWidget_);
  rightLayoutProperties->addWidget(nameEdit_);

  // 背景图片选择
  imgLabel_ = new QLabel(propertiesWidget_);
  imgLabel_->setObjectName("ConfigFieldLabel");
  rightLayoutProperties->addWidget(imgLabel_);
  imageCombo_ = new QComboBox(propertiesWidget_);
  rightLayoutProperties->addWidget(imageCombo_);

  // 拉伸模式
  modeLabel_ = new QLabel(propertiesWidget_);
  modeLabel_->setObjectName("ConfigFieldLabel");
  rightLayoutProperties->addWidget(modeLabel_);
  drawModeCombo_ = new QComboBox(propertiesWidget_);
  drawModeCombo_->addItem("", 0);
  drawModeCombo_->addItem("", 1);
  rightLayoutProperties->addWidget(drawModeCombo_);

  // 背景预览
  prevLabelTitle_ = new QLabel(propertiesWidget_);
  prevLabelTitle_->setObjectName("ConfigFieldLabel");
  rightLayoutProperties->addWidget(prevLabelTitle_);

  previewLabel_ = new QLabel(propertiesWidget_);
  previewLabel_->setFixedHeight(100);
  previewLabel_->setAlignment(Qt::AlignCenter);
  previewLabel_->setStyleSheet("QLabel {"
                               "  border: 1px solid rgba(255, 255, 255, 0.1);"
                               "  border-radius: 6px;"
                               "  background-color: rgba(0, 0, 0, 0.2);"
                               "}");
  rightLayoutProperties->addWidget(previewLabel_, 1);

  splitLayout->addWidget(propertiesWidget_, 1);
  contentLayout->addLayout(splitLayout, 1);
  mainLayout->addWidget(contentWidget, 1);

  // 3. 底部按钮区 (Footer)
  auto *footer = new QWidget(this);
  footer->setObjectName("ConfigFooter");
  footer->setFixedHeight(60);
  auto *footerLayout = new QHBoxLayout(footer);
  footerLayout->setContentsMargins(20, 0, 20, 0);
  footerLayout->addStretch();

  cancelBtn_ = new QPushButton(footer);
  cancelBtn_->setObjectName("ConfigCancelButton");
  saveBtn_ = new QPushButton(footer);
  saveBtn_->setObjectName("ConfigOkButton");

  footerLayout->addWidget(cancelBtn_);
  footerLayout->addWidget(saveBtn_);
  mainLayout->addWidget(footer);
}

void SpeakerManagerDialog::retranslateUi() {
  setWindowTitle(tr("Speaker Management"));
  if (titleLabel)
    titleLabel->setText(tr("Speaker Management"));
  if (folderLabel_)
    folderLabel_->setText(tr("Background Folder:"));
  if (bgFolderEdit_)
    bgFolderEdit_->setPlaceholderText(
        tr("Select folder containing background images..."));
  if (browseFolderBtn_)
    browseFolderBtn_->setText(tr("Browse..."));
  if (marginLabel_)
    marginLabel_->setText(tr("9-Patch Margins:"));
  if (marginLeftLabel_)
    marginLeftLabel_->setText(tr("Left"));
  if (marginRightLabel_)
    marginRightLabel_->setText(tr("Right"));
  if (marginTopLabel_)
    marginTopLabel_->setText(tr("Top"));
  if (marginBottomLabel_)
    marginBottomLabel_->setText(tr("Bottom"));
  if (listLabel_)
    listLabel_->setText(tr("Speaker List"));
  if (addBtn_)
    addBtn_->setText(tr("Add"));
  if (removeBtn_)
    removeBtn_->setText(tr("Delete"));
  if (nameLabel_)
    nameLabel_->setText(tr("Speaker Name:"));
  if (imgLabel_)
    imgLabel_->setText(tr("Background Image:"));
  if (modeLabel_)
    modeLabel_->setText(tr("Draw Mode:"));
  if (prevLabelTitle_)
    prevLabelTitle_->setText(tr("Background Preview:"));
  if (drawModeCombo_) {
    drawModeCombo_->setItemText(0, tr("9-Patch Stretch"));
    drawModeCombo_->setItemText(1, tr("Fixed Size"));
  }
  if (cancelBtn_)
    cancelBtn_->setText(tr("Cancel"));
  if (saveBtn_)
    saveBtn_->setText(tr("Save & Apply"));

  // 刷新 ComboBox 的 "None" 选项
  if (imageCombo_ && imageCombo_->count() > 0) {
    imageCombo_->setItemText(0, tr("None"));
  }

  // 刷新右侧预览文本（在未选中状态下）
  if (speakerList_ && !speakerList_->currentItem() && previewLabel_) {
    previewLabel_->setText(tr("Select a speaker to edit properties"));
  } else {
    updatePreviewImage();
  }
}

void SpeakerManagerDialog::loadSettings() {
  loading_ = true;

  tempBgFolder_ = track_->globalBgFolder();
  tempMargins_ = track_->unifiedBorderMargins();

  bgFolderEdit_->setText(tempBgFolder_);
  marginLeftSpin_->setValue(tempMargins_.left());
  marginRightSpin_->setValue(tempMargins_.right());
  marginTopSpin_->setValue(tempMargins_.top());
  marginBottomSpin_->setValue(tempMargins_.bottom());

  tempSpeakers_.clear();
  for (const auto &spk : track_->allSpeakers()) {
    tempSpeakers_[spk.id] = spk;
  }

  scanImageFiles();
  populateSpeakerList();

  loading_ = false;

  if (speakerList_->count() > 0) {
    speakerList_->setCurrentRow(0);
  } else {
    propertiesWidget_->setEnabled(false);
  }
}

void SpeakerManagerDialog::scanImageFiles() {
  availableImages_.clear();
  QString folder = bgFolderEdit_->text();
  if (!folder.isEmpty() && QDir(folder).exists()) {
    QDir dir(folder);
    availableImages_ =
        dir.entryList({"*.png", "*.jpg", "*.jpeg", "*.bmp"}, QDir::Files);
  }
  populateImageCombo();
}

void SpeakerManagerDialog::populateImageCombo() {
  bool prevLoading = loading_;
  loading_ = true;
  imageCombo_->clear();
  imageCombo_->addItem(tr("None"), "");
  for (const auto &img : availableImages_) {
    imageCombo_->addItem(img, img);
  }
  loading_ = prevLoading;
}

void SpeakerManagerDialog::populateSpeakerList() {
  speakerList_->clear();
  // 排序插入
  QList<int> ids = tempSpeakers_.keys();
  std::sort(ids.begin(), ids.end());

  for (int id : ids) {
    const auto &spk = tempSpeakers_[id];
    QString text = spk.name.isEmpty()
                       ? QString("Speaker %1").arg(id)
                       : QString("%1 (%2)").arg(spk.name).arg(id);
    auto *item = new QListWidgetItem(text, speakerList_);
    item->setData(Qt::UserRole, id);
    speakerList_->addItem(item);
  }
}

void SpeakerManagerDialog::onSpeakerSelectionChanged() {
  QListWidgetItem *curItem = speakerList_->currentItem();
  if (!curItem) {
    propertiesWidget_->setEnabled(false);
    nameEdit_->clear();
    imageCombo_->setCurrentIndex(0);
    drawModeCombo_->setCurrentIndex(0);
    previewLabel_->setPixmap(QPixmap());
    previewLabel_->setText(tr("Select a speaker to edit properties"));
    return;
  }

  propertiesWidget_->setEnabled(true);
  int id = curItem->data(Qt::UserRole).toInt();
  const auto &info = tempSpeakers_[id];

  bool prevLoading = loading_;
  loading_ = true;

  nameEdit_->setText(info.name);
  int imgIdx = imageCombo_->findData(info.bgImageFile);
  if (imgIdx >= 0) {
    imageCombo_->setCurrentIndex(imgIdx);
  } else {
    imageCombo_->setCurrentIndex(0);
  }

  drawModeCombo_->setCurrentIndex(info.is9Patch ? 0 : 1);

  loading_ = prevLoading;
  updatePreviewImage();
}

void SpeakerManagerDialog::onAddSpeaker() {
  int nextId = 0;
  for (int id : tempSpeakers_.keys()) {
    if (id >= nextId) {
      nextId = id + 1;
    }
  }

  SpeakerInfo spk;
  spk.id = nextId;
  spk.name = QString("Speaker %1").arg(nextId);
  spk.is9Patch = true;
  tempSpeakers_[nextId] = spk;

  populateSpeakerList();

  // 选中新增的说话人
  for (int i = 0; i < speakerList_->count(); ++i) {
    if (speakerList_->item(i)->data(Qt::UserRole).toInt() == nextId) {
      speakerList_->setCurrentRow(i);
      break;
    }
  }
}

void SpeakerManagerDialog::onRemoveSpeaker() {
  QListWidgetItem *curItem = speakerList_->currentItem();
  if (!curItem)
    return;

  int id = curItem->data(Qt::UserRole).toInt();
  tempSpeakers_.remove(id);

  int nextRow = qMin(speakerList_->currentRow(), speakerList_->count() - 2);
  populateSpeakerList();

  if (speakerList_->count() > 0 && nextRow >= 0) {
    speakerList_->setCurrentRow(nextRow);
  } else {
    onSpeakerSelectionChanged();
  }
}

void SpeakerManagerDialog::onBrowseFolder() {
  QString dir = QFileDialog::getExistingDirectory(
      this, tr("Select Background Folder"), bgFolderEdit_->text(),
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
  if (!dir.isEmpty()) {
    bgFolderEdit_->setText(dir);
    scanImageFiles();
    updatePreviewImage();
  }
}

void SpeakerManagerDialog::onImageFileChanged(const QString &fileName) {
  if (loading_)
    return;
  QListWidgetItem *curItem = speakerList_->currentItem();
  if (!curItem)
    return;

  int id = curItem->data(Qt::UserRole).toInt();
  tempSpeakers_[id].bgImageFile = imageCombo_->currentData().toString();
  updatePreviewImage();
}

void SpeakerManagerDialog::onDrawModeChanged(int index) {
  if (loading_)
    return;
  QListWidgetItem *curItem = speakerList_->currentItem();
  if (!curItem)
    return;

  int id = curItem->data(Qt::UserRole).toInt();
  tempSpeakers_[id].is9Patch = (drawModeCombo_->currentData().toInt() == 0);
  updatePreviewImage();
}

void SpeakerManagerDialog::onNameChanged(const QString &name) {
  if (loading_)
    return;
  QListWidgetItem *curItem = speakerList_->currentItem();
  if (!curItem)
    return;

  int id = curItem->data(Qt::UserRole).toInt();
  tempSpeakers_[id].name = name;
  curItem->setText(QString("%1 (%2)").arg(name).arg(id));
}

void SpeakerManagerDialog::onMarginChanged(int /*value*/) {
  if (loading_)
    return;
  tempMargins_ =
      QMargins(marginLeftSpin_->value(), marginTopSpin_->value(),
               marginRightSpin_->value(), marginBottomSpin_->value());
  updatePreviewImage();
}

// 辅助方法：模拟九宫格拉伸绘制预览
static void drawNinePatchPreviewHelper(QPainter &painter, const QImage &src,
                                       const QRect &target, const QMargins &m) {
  int sw = src.width();
  int sh = src.height();
  int tw = target.width();
  int th = target.height();

  int ml = m.left(), mr = m.right(), mt = m.top(), mb = m.bottom();

  ml = qMin(ml, sw / 2);
  mr = qMin(mr, sw / 2);
  mt = qMin(mt, sh / 2);
  mb = qMin(mb, sh / 2);

  QRect sTL(0, 0, ml, mt);
  QRect sTC(ml, 0, sw - ml - mr, mt);
  QRect sTR(sw - mr, 0, mr, mt);
  QRect sML(0, mt, ml, sh - mt - mb);
  QRect sMC(ml, mt, sw - ml - mr, sh - mt - mb);
  QRect sMR(sw - mr, mt, mr, sh - mt - mb);
  QRect sBL(0, sh - mb, ml, mb);
  QRect sBC(ml, sh - mb, sw - ml - mr, mb);
  QRect sBR(sw - mr, sh - mb, mr, mb);

  int tx = target.x(), ty = target.y();

  QRect dTL(tx, ty, ml, mt);
  QRect dTC(tx + ml, ty, tw - ml - mr, mt);
  QRect dTR(tx + tw - mr, ty, mr, mt);
  QRect dML(tx, ty + mt, ml, th - mt - mb);
  QRect dMC(tx + ml, ty + mt, tw - ml - mr, th - mt - mb);
  QRect dMR(tx + tw - mr, ty + mt, mr, th - mt - mb);
  QRect dBL(tx, ty + th - mb, ml, mb);
  QRect dBC(tx + ml, ty + th - mb, tw - ml - mr, mb);
  QRect dBR(tx + tw - mr, ty + th - mb, mr, mb);

  painter.drawImage(dTL, src, sTL);
  painter.drawImage(dTC, src, sTC);
  painter.drawImage(dTR, src, sTR);
  painter.drawImage(dML, src, sML);
  painter.drawImage(dMC, src, sMC);
  painter.drawImage(dMR, src, sMR);
  painter.drawImage(dBL, src, sBL);
  painter.drawImage(dBC, src, sBC);
  painter.drawImage(dBR, src, sBR);
}

void SpeakerManagerDialog::updatePreviewImage() {
  if (loading_)
    return;

  QString imgName = imageCombo_->currentData().toString();
  if (imgName.isEmpty()) {
    previewLabel_->setPixmap(QPixmap());
    previewLabel_->setText(tr("No background image"));
    return;
  }

  QString folder = bgFolderEdit_->text();
  QString fullPath = QDir(folder).filePath(imgName);
  QImage img(fullPath);
  if (img.isNull()) {
    previewLabel_->setPixmap(QPixmap());
    previewLabel_->setText(tr("Failed to load background image"));
    return;
  }

  // 构造预览背景图大小
  int pw = previewLabel_->width() > 50 ? previewLabel_->width() : 400;
  int ph = previewLabel_->height() > 50 ? previewLabel_->height() : 100;

  QPixmap pixmap(pw, ph);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);

  // 模拟深色底座
  painter.fillRect(0, 0, pw, ph, QColor(0, 0, 0, 100));

  bool is9Patch = (drawModeCombo_->currentData().toInt() == 0);

  // 模拟的文字边框大小为 180 x 36
  QRect textRect((pw - 180) / 2, (ph - 36) / 2, 180, 36);
  // 向外扩展的背景大小
  QRect bgRect = textRect.adjusted(-15, -8, 15, 8);

  if (is9Patch) {
    drawNinePatchPreviewHelper(painter, img, bgRect, tempMargins_);
  } else {
    // 固定长度
    int imgX = bgRect.center().x() - img.width() / 2;
    int imgY = bgRect.center().y() - img.height() / 2;
    painter.drawImage(imgX, imgY, img);
  }

  // 绘制示例文字
  painter.setPen(Qt::white);
  QFont font = painter.font();
  font.setFamily("Inter");
  font.setPointSize(10);
  font.setBold(true);
  painter.setFont(font);
  painter.drawText(textRect, Qt::AlignCenter,
                   tr("Sample Subtitle Background Preview"));

  // 绘制九宫格边距虚线（浅红）
  if (is9Patch) {
    painter.setPen(QPen(QColor(239, 68, 68, 140), 1, Qt::DashLine));
    int x1 = bgRect.left() + tempMargins_.left();
    int x2 = bgRect.right() - tempMargins_.right();
    int y1 = bgRect.top() + tempMargins_.top();
    int y2 = bgRect.bottom() - tempMargins_.bottom();

    // 垂直线
    painter.drawLine(x1, bgRect.top(), x1, bgRect.bottom());
    painter.drawLine(x2, bgRect.top(), x2, bgRect.bottom());
    // 水平线
    painter.drawLine(bgRect.left(), y1, bgRect.right(), y1);
    painter.drawLine(bgRect.left(), y2, bgRect.right(), y2);
  }

  painter.end();
  previewLabel_->setPixmap(pixmap);
}

void SpeakerManagerDialog::onSaveAndApply() {
  track_->setGlobalBgFolder(bgFolderEdit_->text());
  track_->setUnifiedBorderMargins(tempMargins_);

  track_->clearSpeakers();
  for (const auto &spk : tempSpeakers_) {
    track_->setSpeakerInfo(spk.id, spk);
  }

  track_->saveGlobalSettings();
  accept();
}

void SpeakerManagerDialog::onCancel() { reject(); }

void SpeakerManagerDialog::updateTheme() {
  // 当主题变更时更新预览
  updatePreviewImage();
}
