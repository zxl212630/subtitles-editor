#include "SubtitleListPanel.h"
#include "AppMessageBox.h"
#include "ConfigManager.h"
#include "SpeakerManagerDialog.h"
#include "SubtitleListDelegate.h"
#include "SubtitleListModel.h"
#include "SubtitleTrack.h"
#include "ThemeManager.h"
#include "TranslationManager.h"
#include <QAbstractItemView>
#include <QBrush>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QSvgRenderer>

#include <QCoreApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QVBoxLayout>

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionSlider>

class ClickableSlider : public QSlider {
  Q_OBJECT
public:
  using QSlider::QSlider;

protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      QStyleOptionSlider opt;
      initStyleOption(&opt);
      QRect sr = style()->subControlRect(QStyle::CC_Slider, &opt,
                                         QStyle::SC_SliderHandle, this);

      if (!sr.contains(event->pos())) {
        int val;
        if (orientation() == Qt::Horizontal) {
          val = QStyle::sliderValueFromPosition(
              minimum(), maximum(), event->pos().x() - sr.width() / 2,
              width() - sr.width(), opt.upsideDown);
        } else {
          val = QStyle::sliderValueFromPosition(
              minimum(), maximum(),
              height() - event->pos().y() - sr.height() / 2,
              height() - sr.height(), opt.upsideDown);
        }
        setValue(val);
      }
    }
    QSlider::mousePressEvent(event);
  }
};

class SubtitleActionOverlay : public QWidget {
  Q_OBJECT
public:
  explicit SubtitleActionOverlay(QWidget *parent = nullptr) : QWidget(parent) {
    setFixedHeight(28);
    hide();

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->setAlignment(Qt::AlignCenter);

    addBtn_ = createBtn(":/icons/add.svg", tr("添加"));
    addBtn_->setObjectName("SubtitleActionBtn");
    mergeBtn_ = createBtn(":/icons/merge.svg", tr("合并"));
    mergeBtn_->setObjectName("SubtitleActionBtn");

    layout->addStretch();
    layout->addWidget(addBtn_);
    layout->addWidget(mergeBtn_);
    layout->addStretch();

    connect(addBtn_, &QPushButton::clicked, this,
            [this]() { emit addClicked(gapStart_, gapEnd_); });
    connect(mergeBtn_, &QPushButton::clicked, this,
            [this]() { emit mergeClicked(); });
  }

  void updateState(qint64 gapStart, qint64 gapEnd, bool canMerge, double fps) {
    gapStart_ = gapStart;
    gapEnd_ = gapEnd;

    double minGap = 1000.0 / fps;
    bool canAdd = (gapEnd - gapStart) >= minGap;

    addBtn_->setEnabled(canAdd);
    mergeBtn_->setEnabled(canMerge);
  }

  void retranslateUi() {
    addBtn_->setText(tr("添加"));
    mergeBtn_->setText(tr("合并"));
  }

signals:
  void addClicked(qint64 start, qint64 end);
  void mergeClicked();

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Dynamic horizontal line using primary color
    QColor primaryColor = ThemeManager::instance().getPrimaryColor();
    painter.setPen(QPen(primaryColor, 1));
    int midY = height() / 2;
    painter.drawLine(0, midY, width(), midY);
  }

private:
  QPushButton *createBtn(const QString &iconPath, const QString &text) {
    auto *btn = new QPushButton(this);
    btn->setText(text);
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(14, 14));
    return btn;
  }

  QPushButton *addBtn_ = nullptr;
  QPushButton *mergeBtn_ = nullptr;
  qint64 gapStart_ = 0;
  qint64 gapEnd_ = 0;
};

SubtitleListPanel::SubtitleListPanel(QWidget *parent) : QWidget(parent) {
  setupUi();

  connect(&TranslationManager::instance(), &TranslationManager::languageChanged,
          this, [this]() { retranslateUi(); });
}

void SubtitleListPanel::setTrack(SubtitleTrack *track) {
  if (track_) {
    disconnect(track_, &SubtitleTrack::itemSelected, this,
               &SubtitleListPanel::onTrackItemSelected);
  }
  track_ = track;
  model_->setTrack(track);
  delegate_->setTrack(track);
  if (track_) {
    connect(track_, &SubtitleTrack::itemSelected, this,
            &SubtitleListPanel::onTrackItemSelected);
  }
}

void SubtitleListPanel::setVideoFps(double fps) {
  if (fps > 0)
    videoFps_ = fps;
}

void SubtitleListPanel::setTotalDuration(qint64 ms) { totalDurationMs_ = ms; }

void SubtitleListPanel::updateSpeakerColumnVisibility() {
  bool enabled = ConfigManager::instance().speakerDiarization();
  if (headerSpeaker_) {
    headerSpeaker_->setVisible(enabled);
  }
  if (listView_ && listView_->viewport()) {
    listView_->viewport()->update();
  }
}

void SubtitleListPanel::retranslateUi() {
  if (actionOverlay_)
    actionOverlay_->retranslateUi();
  searchEdit_->setPlaceholderText(tr("Search..."));
  if (searchClearBtn_)
    searchClearBtn_->setToolTip(tr("Clear"));
  if (tabSubtitle_)
    tabSubtitle_->setText(tr("Subtitle"));
  if (tabPreset_)
    tabPreset_->setText(tr("Preset"));
  if (tabBubble_)
    tabBubble_->setText(tr("Bubble"));
  if (tabCustom_)
    tabCustom_->setText(tr("Custom"));
  if (tabAnimation_)
    tabAnimation_->setText(tr("Animation"));
  if (headerTime_)
    headerTime_->setText(tr("Timecode"));
  if (headerSpeaker_)
    headerSpeaker_->setText(tr("Speaker"));
  if (headerText_)
    headerText_->setText(tr("Subtitle"));
  if (headerAction_)
    headerAction_->setText(tr("Action"));

  if (presetTypeCombo_) {
    // 临时阻止信号，避免更新文本时触发 index 更改
    bool old = presetTypeCombo_->signalsBlocked();
    presetTypeCombo_->blockSignals(true);
    int idx = presetTypeCombo_->currentIndex();
    presetTypeCombo_->clear();
    presetTypeCombo_->addItem(tr("System Presets"), 0);
    presetTypeCombo_->addItem(tr("Custom Presets"), 1);
    presetTypeCombo_->setCurrentIndex(idx);
    presetTypeCombo_->blockSignals(old);
  }
  if (savePresetBtn_) {
    savePresetBtn_->setText(tr("+ Save Current Style"));
  }
  if (bubbleImageBrowse_) {
    bubbleImageBrowse_->setText(tr("Browse..."));
  }

  if (fillTypeCombo_) {
    bool old = fillTypeCombo_->signalsBlocked();
    fillTypeCombo_->blockSignals(true);
    int idx = fillTypeCombo_->currentIndex();
    fillTypeCombo_->clear();
    fillTypeCombo_->addItems({tr("Color Fill"), tr("Gradient Fill")});
    fillTypeCombo_->setCurrentIndex(idx);
    fillTypeCombo_->blockSignals(old);
  }

  if (auto *lbl = findChild<QLabel *>("FillHeaderLabel"))
    lbl->setText(tr("Fill"));
  if (auto *lbl = findChild<QLabel *>("OutlineHeaderLabel"))
    lbl->setText(tr("Outline"));
  if (auto *lbl = findChild<QLabel *>("ShadowHeaderLabel"))
    lbl->setText(tr("Shadow"));
  if (auto *lbl = findChild<QLabel *>("BackgroundHeaderLabel"))
    lbl->setText(tr("Background"));
  if (auto *lbl = findChild<QLabel *>("BubbleHeaderLabel"))
    lbl->setText(tr("Bubble"));

  if (auto *lbl = findChild<QLabel *>("lblFillType"))
    lbl->setText(tr("Type"));
  if (auto *lbl = findChild<QLabel *>("lblFillColor"))
    lbl->setText(tr("Color 1"));
  if (auto *lbl = findChild<QLabel *>("lblFillColor2"))
    lbl->setText(tr("Color 2"));
  if (auto *lbl = findChild<QLabel *>("lblFillAngle"))
    lbl->setText(tr("Angle"));
  if (auto *lbl = findChild<QLabel *>("lblFillOpacity"))
    lbl->setText(tr("Opacity"));

  if (auto *lbl = findChild<QLabel *>("lblStrokeColor"))
    lbl->setText(tr("Color"));
  if (auto *lbl = findChild<QLabel *>("lblStrokeThickness"))
    lbl->setText(tr("Thickness"));
  if (auto *lbl = findChild<QLabel *>("lblStrokeOpacity"))
    lbl->setText(tr("Opacity"));

  if (auto *lbl = findChild<QLabel *>("lblShadowColor"))
    lbl->setText(tr("Color"));
  if (auto *lbl = findChild<QLabel *>("lblShadowOffsetX"))
    lbl->setText(tr("L/R Offset"));
  if (auto *lbl = findChild<QLabel *>("lblShadowOffsetY"))
    lbl->setText(tr("T/B Offset"));
  if (auto *lbl = findChild<QLabel *>("lblShadowBlur"))
    lbl->setText(tr("Blur"));
  if (auto *lbl = findChild<QLabel *>("lblShadowOpacity"))
    lbl->setText(tr("Opacity"));

  if (auto *lbl = findChild<QLabel *>("lblBgColor"))
    lbl->setText(tr("Color"));
  if (auto *lbl = findChild<QLabel *>("lblBgOpacity"))
    lbl->setText(tr("Opacity"));
  if (auto *lbl = findChild<QLabel *>("lblBgRoundness"))
    lbl->setText(tr("Roundness"));
  if (auto *lbl = findChild<QLabel *>("lblBgPaddingX"))
    lbl->setText(tr("L/R Padding"));
  if (auto *lbl = findChild<QLabel *>("lblBgPaddingY"))
    lbl->setText(tr("T/B Padding"));
  if (auto *lbl = findChild<QLabel *>("lblBgOffsetX"))
    lbl->setText(tr("L/R Offset"));
  if (auto *lbl = findChild<QLabel *>("lblBgOffsetY"))
    lbl->setText(tr("T/B Offset"));

  if (auto *lbl = findChild<QLabel *>("lblBubbleImage"))
    lbl->setText(tr("Image"));
  if (auto *lbl = findChild<QLabel *>("lblBubblePaddingLeft"))
    lbl->setText(tr("Left Padding"));
  if (auto *lbl = findChild<QLabel *>("lblBubblePaddingRight"))
    lbl->setText(tr("Right Padding"));
  if (auto *lbl = findChild<QLabel *>("lblBubblePaddingTop"))
    lbl->setText(tr("Top Padding"));
  if (auto *lbl = findChild<QLabel *>("lblBubblePaddingBottom"))
    lbl->setText(tr("Bottom Padding"));
  if (auto *lbl = findChild<QLabel *>("lblBubbleSliceLeft"))
    lbl->setText(tr("Left Slice"));
  if (auto *lbl = findChild<QLabel *>("lblBubbleSliceRight"))
    lbl->setText(tr("Right Slice"));
  if (auto *lbl = findChild<QLabel *>("lblBubbleSliceTop"))
    lbl->setText(tr("Top Slice"));
  if (auto *lbl = findChild<QLabel *>("lblBubbleSliceBottom"))
    lbl->setText(tr("Bottom Slice"));

  if (auto *lbl = findChild<QLabel *>("lblBgPaddingUniformTitle"))
    lbl->setText(tr("Background Padding"));
  if (auto *lbl = findChild<QLabel *>("lblBgPaddingGroupHeaderTitle")) {
    lbl->setText(tr("Background Padding"));
    if (auto *btn = findChild<QPushButton *>("BgPaddingGroupHeaderBtn")) {
      int w = lbl->fontMetrics().horizontalAdvance(lbl->text()) + 6 + 12 + 8;
      btn->setGeometry(12, 6, w, 20);
    }
  }

  if (auto *lbl = findChild<QLabel *>("lblBubblePaddingUniformTitle"))
    lbl->setText(tr("Text Padding"));
  if (auto *lbl = findChild<QLabel *>("lblBubblePaddingGroupHeaderTitle")) {
    lbl->setText(tr("Text Padding"));
    if (auto *btn = findChild<QPushButton *>("BubblePaddingGroupHeaderBtn")) {
      int w = lbl->fontMetrics().horizontalAdvance(lbl->text()) + 6 + 12 + 8;
      btn->setGeometry(12, 6, w, 20);
    }
  }

  if (auto *lbl = findChild<QLabel *>("lblBubbleSliceUniformTitle"))
    lbl->setText(tr("9-Patch Stretch"));
  if (auto *lbl = findChild<QLabel *>("lblBubbleSliceGroupHeaderTitle")) {
    lbl->setText(tr("9-Patch Stretch"));
    if (auto *btn = findChild<QPushButton *>("BubbleSliceGroupHeaderBtn")) {
      int w = lbl->fontMetrics().horizontalAdvance(lbl->text()) + 6 + 12 + 8;
      btn->setGeometry(12, 6, w, 20);
    }
  }

  populatePresets();
  populateBubbles();
}

void SubtitleListPanel::setupUi() {
  setObjectName("SubtitleListPanel");
  setAttribute(Qt::WA_StyledBackground);
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // --- Panel header (tabs) ---
  auto *panelHeader = new QFrame(this);
  panelHeader->setObjectName("SubtitlePanelHeader");
  panelHeader->setFixedHeight(40);
  auto *phLayout = new QHBoxLayout(panelHeader);
  phLayout->setContentsMargins(12, 6, 0, 6);
  phLayout->setSpacing(4);
  phLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  tabSubtitle_ = new QPushButton(tr("Subtitle"), panelHeader);
  tabSubtitle_->setObjectName("SubtitleTabBtn");
  tabSubtitle_->setProperty("active", true);
  tabSubtitle_->setFixedSize(60, 28);
  phLayout->addWidget(tabSubtitle_);

  tabPreset_ = new QPushButton(tr("Preset"), panelHeader);
  tabPreset_->setObjectName("SubtitleTabBtn");
  tabPreset_->setProperty("active", false);
  tabPreset_->setFixedSize(60, 28);
  phLayout->addWidget(tabPreset_);

  tabBubble_ = new QPushButton(tr("Bubble"), panelHeader);
  tabBubble_->setObjectName("SubtitleTabBtn");
  tabBubble_->setProperty("active", false);
  tabBubble_->setFixedSize(60, 28);
  phLayout->addWidget(tabBubble_);

  tabCustom_ = new QPushButton(tr("Custom"), panelHeader);
  tabCustom_->setObjectName("SubtitleTabBtn");
  tabCustom_->setProperty("active", false);
  tabCustom_->setFixedSize(60, 28);
  phLayout->addWidget(tabCustom_);

  tabAnimation_ = new QPushButton(tr("Animation"), panelHeader);
  tabAnimation_->setObjectName("SubtitleTabBtn");
  tabAnimation_->setProperty("active", false);
  tabAnimation_->setFixedSize(60, 28);
  phLayout->addWidget(tabAnimation_);

  tabSubtitle_->setAttribute(Qt::WA_TransparentForMouseEvents, false);
  tabPreset_->setAttribute(Qt::WA_TransparentForMouseEvents, false);
  tabBubble_->setAttribute(Qt::WA_TransparentForMouseEvents, false);
  tabCustom_->setAttribute(Qt::WA_TransparentForMouseEvents, false);
  tabAnimation_->setAttribute(Qt::WA_TransparentForMouseEvents, true);

  tabPreset_->show();
  tabBubble_->show();
  tabCustom_->show();
  tabAnimation_->hide();

  connect(tabSubtitle_, &QPushButton::clicked, this, [this]() {
    if (stackedWidget_)
      stackedWidget_->setCurrentIndex(0);
    tabSubtitle_->setProperty("active", true);
    tabPreset_->setProperty("active", false);
    tabBubble_->setProperty("active", false);
    tabCustom_->setProperty("active", false);
    tabSubtitle_->style()->unpolish(tabSubtitle_);
    tabSubtitle_->style()->polish(tabSubtitle_);
    tabPreset_->style()->unpolish(tabPreset_);
    tabPreset_->style()->polish(tabPreset_);
    tabBubble_->style()->unpolish(tabBubble_);
    tabBubble_->style()->polish(tabBubble_);
    tabCustom_->style()->unpolish(tabCustom_);
    tabCustom_->style()->polish(tabCustom_);
  });

  connect(tabPreset_, &QPushButton::clicked, this, [this]() {
    if (stackedWidget_)
      stackedWidget_->setCurrentIndex(1);
    tabSubtitle_->setProperty("active", false);
    tabPreset_->setProperty("active", true);
    tabBubble_->setProperty("active", false);
    tabCustom_->setProperty("active", false);
    tabSubtitle_->style()->unpolish(tabSubtitle_);
    tabSubtitle_->style()->polish(tabSubtitle_);
    tabPreset_->style()->unpolish(tabPreset_);
    tabPreset_->style()->polish(tabPreset_);
    tabBubble_->style()->unpolish(tabBubble_);
    tabBubble_->style()->polish(tabBubble_);
    tabCustom_->style()->unpolish(tabCustom_);
    tabCustom_->style()->polish(tabCustom_);
  });

  connect(tabBubble_, &QPushButton::clicked, this, [this]() {
    if (stackedWidget_) {
      stackedWidget_->setCurrentIndex(2);
      populateBubbles();
    }
    tabSubtitle_->setProperty("active", false);
    tabPreset_->setProperty("active", false);
    tabBubble_->setProperty("active", true);
    tabCustom_->setProperty("active", false);
    tabSubtitle_->style()->unpolish(tabSubtitle_);
    tabSubtitle_->style()->polish(tabSubtitle_);
    tabPreset_->style()->unpolish(tabPreset_);
    tabPreset_->style()->polish(tabPreset_);
    tabBubble_->style()->unpolish(tabBubble_);
    tabBubble_->style()->polish(tabBubble_);
    tabCustom_->style()->unpolish(tabCustom_);
    tabCustom_->style()->polish(tabCustom_);
  });

  connect(tabCustom_, &QPushButton::clicked, this, [this]() {
    if (stackedWidget_) {
      stackedWidget_->setCurrentIndex(3);
      // Automatically load the active subtitle's style
      if (track_) {
        bool found = false;
        for (const auto &item : track_->items()) {
          if (item.selected) {
            loadStyleFromItem(item);
            found = true;
            break;
          }
        }
        if (!found) {
          loadStyleFromItem(track_->defaultStyleItem());
        }
      }
    }
    tabSubtitle_->setProperty("active", false);
    tabPreset_->setProperty("active", false);
    tabBubble_->setProperty("active", false);
    tabCustom_->setProperty("active", true);
    tabSubtitle_->style()->unpolish(tabSubtitle_);
    tabSubtitle_->style()->polish(tabSubtitle_);
    tabPreset_->style()->unpolish(tabPreset_);
    tabPreset_->style()->polish(tabPreset_);
    tabBubble_->style()->unpolish(tabBubble_);
    tabBubble_->style()->polish(tabBubble_);
    tabCustom_->style()->unpolish(tabCustom_);
    tabCustom_->style()->polish(tabCustom_);
  });

  phLayout->addStretch();
  layout->addWidget(panelHeader);

  // --- Panel content ---
  auto *panelContent = new QFrame(this);
  panelContent->setObjectName("SubtitlePanelContent");
  panelContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto *pcLayout = new QVBoxLayout(panelContent);
  pcLayout->setContentsMargins(12, 12, 12, 12);
  pcLayout->setSpacing(0);

  // Search bar
  auto *searchBar = new QFrame(panelContent);
  searchBar->setObjectName("SubtitleSearchBar");
  searchBar->setFixedHeight(40);
  auto *sbLayout = new QHBoxLayout(searchBar);
  sbLayout->setContentsMargins(0, 0, 0, 0);
  sbLayout->setAlignment(Qt::AlignVCenter);

  // Search input container (icon + text inside a single frame)
  auto *searchInput = new QFrame(searchBar);
  searchInput->setObjectName("SubtitleSearchInputContainer");
  searchInput->setFixedHeight(28);
  auto *siLayout = new QHBoxLayout(searchInput);
  siLayout->setContentsMargins(10, 0, 10, 0);
  siLayout->setSpacing(4);

  auto *searchIcon = new QLabel(searchInput);
  searchIcon->setObjectName("SubtitleSearchIcon");
  searchIcon->setFixedSize(14, 14);
  searchIcon->setAlignment(Qt::AlignCenter);
  siLayout->addWidget(searchIcon, 0, Qt::AlignVCenter);

  searchEdit_ = new QLineEdit(searchInput);
  searchEdit_->setObjectName("SubtitleSearchEdit");
  searchEdit_->setPlaceholderText(tr("Search..."));
  siLayout->addWidget(searchEdit_, 1);

  searchClearBtn_ = new QPushButton(searchInput);
  searchClearBtn_->setObjectName("SubtitleSearchClearButton");
  searchClearBtn_->setFixedSize(18, 18);
  searchClearBtn_->setIcon(QIcon(":/icons/close.svg"));
  searchClearBtn_->setIconSize(QSize(10, 10));
  searchClearBtn_->setToolTip(tr("Clear"));
  searchClearBtn_->hide();
  siLayout->addWidget(searchClearBtn_, 0, Qt::AlignVCenter);

  sbLayout->addWidget(searchInput);
  pcLayout->addWidget(searchBar);

  connect(searchEdit_, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            model_->setFilterText(text);
            searchClearBtn_->setVisible(!text.isEmpty());
          });

  connect(searchClearBtn_, &QPushButton::clicked, this,
          [this]() { searchEdit_->clear(); });

  // List container
  auto *listContainer = new QFrame(panelContent);
  listContainer->setObjectName("SubtitleListContainer");
  listContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto *lcLayout = new QVBoxLayout(listContainer);
  lcLayout->setContentsMargins(0, 0, 0, 0);
  lcLayout->setSpacing(0);

  // Table header
  auto *tableHeader = new QFrame(listContainer);
  tableHeader->setObjectName("SubtitleTableHeader");
  tableHeader->setFixedHeight(32);
  auto *thLayout = new QHBoxLayout(tableHeader);
  thLayout->setContentsMargins(12, 0, 12, 0);
  thLayout->setSpacing(12);
  thLayout->setAlignment(Qt::AlignVCenter);

  auto *headerLeft = new QFrame(tableHeader);
  headerLeft->setObjectName("SubtitleHeaderLeft");
  auto *hlLayout = new QHBoxLayout(headerLeft);
  hlLayout->setContentsMargins(0, 0, 0, 0);
  hlLayout->setSpacing(12);
  hlLayout->setAlignment(Qt::AlignVCenter);

  headerTime_ = new QLabel(tr("Timecode"), headerLeft);
  headerTime_->setObjectName("SubtitleHeaderLabel");
  headerTime_->setFixedWidth(115);
  hlLayout->addWidget(headerTime_);

  headerSpeaker_ = new QLabel(tr("Speaker"), headerLeft);
  headerSpeaker_->setObjectName("SubtitleHeaderLabel");
  headerSpeaker_->setFixedWidth(80);
  hlLayout->addWidget(headerSpeaker_);

  headerText_ = new QLabel(tr("Subtitle"), headerLeft);
  headerText_->setObjectName("SubtitleHeaderLabel");
  hlLayout->addWidget(headerText_);

  thLayout->addWidget(headerLeft);
  thLayout->addStretch();

  headerAction_ = new QLabel(tr("Action"), tableHeader);
  headerAction_->setObjectName("SubtitleHeaderLabel");
  thLayout->addWidget(headerAction_);

  lcLayout->addWidget(tableHeader);

  // Separator line
  auto *headerSeparator = new QFrame(listContainer);
  headerSeparator->setObjectName("SubtitleHeaderSeparator");
  headerSeparator->setFixedHeight(1);
  lcLayout->addWidget(headerSeparator);

  // Subtitle list
  listView_ = new QListView(listContainer);
  listView_->setObjectName("SubtitleListView");
  listView_->setSelectionMode(QAbstractItemView::SingleSelection);
  listView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  model_ = new SubtitleListModel(this);
  listView_->setModel(model_);

  delegate_ = new SubtitleListDelegate(this);
  listView_->setItemDelegate(delegate_);

  actionOverlay_ = new SubtitleActionOverlay(listContainer);
  connect(actionOverlay_, &SubtitleActionOverlay::addClicked, this,
          [this](qint64 start, qint64 end) {
            if (track_)
              track_->addGapItem(start, end);
          });

  // Use properties to store IDs on the overlay for merge
  connect(actionOverlay_, &SubtitleActionOverlay::mergeClicked, this, [this]() {
    QString id1 = actionOverlay_->property("id1").toString();
    QString id2 = actionOverlay_->property("id2").toString();
    if (track_ && !id1.isEmpty() && !id2.isEmpty()) {
      track_->mergeItems(id1, id2);
    }
  });

  connect(delegate_, &SubtitleListDelegate::deleteClicked, this,
          [this](const QString &id) {
            if (track_) {
              track_->removeItem(id);
            }
          });

  connect(delegate_, &SubtitleListDelegate::splitClicked, this,
          [this](const QString &id, int pos) {
            if (track_) {
              track_->splitItem(id, pos);
            }
          });

  connect(delegate_, &SubtitleListDelegate::splitClickedWithData, this,
          [this](const QString &id, int pos, const QString &text) {
            if (track_) {
              track_->splitItem(id, pos, text);
            }
          });

  listView_->setMouseTracking(true);
  listView_->viewport()->installEventFilter(this);
  actionOverlay_->installEventFilter(this);
  searchEdit_->installEventFilter(this);

  connect(listView_, &QListView::clicked, this,
          &SubtitleListPanel::onItemClicked);
  connect(listView_, &QListView::doubleClicked, this,
          &SubtitleListPanel::onItemDoubleClicked);

  lcLayout->addWidget(listView_);
  lcLayout->addSpacing(12); // Extra space at bottom
  pcLayout->addWidget(listContainer);
  stackedWidget_ = new QStackedWidget(this);
  stackedWidget_->setObjectName("SubtitlePanelStackedWidget");
  stackedWidget_->addWidget(panelContent);

  auto *presetWrapper = new QFrame(this);
  presetWrapper->setObjectName("SubtitlePanelContent");
  presetWrapper->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto *presetWrapperLayout = new QVBoxLayout(presetWrapper);
  presetWrapperLayout->setContentsMargins(12, 12, 12, 12);
  presetWrapperLayout->setSpacing(0);
  QWidget *presetPanel = createPresetStylePanel();
  presetWrapperLayout->addWidget(presetPanel);
  stackedWidget_->addWidget(presetWrapper);

  auto *bubbleWrapper = new QFrame(this);
  bubbleWrapper->setObjectName("SubtitlePanelContent");
  bubbleWrapper->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto *bubbleWrapperLayout = new QVBoxLayout(bubbleWrapper);
  bubbleWrapperLayout->setContentsMargins(12, 12, 12, 12);
  bubbleWrapperLayout->setSpacing(0);
  QWidget *bubblePanel = createBubbleStylePanel();
  bubbleWrapperLayout->addWidget(bubblePanel);
  stackedWidget_->addWidget(bubbleWrapper);

  auto *customWrapper = new QFrame(this);
  customWrapper->setObjectName("SubtitlePanelContent");
  customWrapper->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto *customWrapperLayout = new QVBoxLayout(customWrapper);
  customWrapperLayout->setContentsMargins(12, 12, 12, 12);
  customWrapperLayout->setSpacing(0);
  QWidget *customPanel = createCustomStylePanel();
  customWrapperLayout->addWidget(customPanel);
  stackedWidget_->addWidget(customWrapper);

  layout->addWidget(stackedWidget_);

  updateSpeakerColumnVisibility();
}

void SubtitleListPanel::onItemClicked(const QModelIndex &index) {
  if (!index.isValid())
    return;
  QString id = model_->data(index, SubtitleListModel::IdRole).toString();
  qint64 startMs =
      model_->data(index, SubtitleListModel::StartMsRole).toLongLong();
  if (track_) {
    track_->selectItem(id);
  }
  emit itemSelected(id);
  emit itemSeekRequested(id, startMs);
}

void SubtitleListPanel::onItemDoubleClicked(const QModelIndex &index) {
  if (!index.isValid())
    return;
  QString id = model_->data(index, SubtitleListModel::IdRole).toString();
  qint64 startMs =
      model_->data(index, SubtitleListModel::StartMsRole).toLongLong();
  if (track_) {
    track_->selectItem(id);
  }
  emit itemDoubleClicked(id, startMs);
}

void SubtitleListPanel::onTrackItemSelected(const QString &id) {
  if (!model_ || !listView_)
    return;
  currentSelectedId_ = id;
  QModelIndex index = model_->indexForId(id);
  if (index.isValid()) {
    if (listView_->currentIndex() != index) {
      listView_->setCurrentIndex(index);
    }
    listView_->scrollTo(index, QAbstractItemView::EnsureVisible);
    if (track_) {
      for (const auto &item : track_->items()) {
        if (item.id == id) {
          loadStyleFromItem(item);
          break;
        }
      }
    }
  } else {
    if (track_) {
      loadStyleFromItem(track_->defaultStyleItem());
    }
  }
}

bool SubtitleListPanel::eventFilter(QObject *watched, QEvent *event) {
  if (watched == searchEdit_) {
    if (event->type() == QEvent::FocusIn) {
      if (auto *parent = qobject_cast<QFrame *>(searchEdit_->parentWidget())) {
        parent->setProperty("focused", true);
        parent->style()->unpolish(parent);
        parent->style()->polish(parent);
      }
    } else if (event->type() == QEvent::FocusOut) {
      if (auto *parent = qobject_cast<QFrame *>(searchEdit_->parentWidget())) {
        parent->setProperty("focused", false);
        parent->style()->unpolish(parent);
        parent->style()->polish(parent);
      }
    }
    return false;
  }

  if (watched == actionOverlay_) {
    if (event->type() == QEvent::Leave) {
      if (actionOverlay_ && !actionOverlay_->underMouse()) {
        actionOverlay_->hide();
      }
    }
    return false;
  }

  if (watched == listView_->viewport()) {
    if (event->type() == QEvent::MouseButtonDblClick) {
      auto *me = static_cast<QMouseEvent *>(event);
      QModelIndex index = listView_->indexAt(me->pos());
      if (index.isValid()) {
        QStyleOptionViewItem option;
        option.initFrom(listView_);
        option.rect = listView_->visualRect(index);

        // Trigger the standard double clicked seek behavior
        onItemDoubleClicked(index);

        // Determine Edit Zone
        int timecodeWidth = 115;
        QRect timeRect(option.rect.left() + 12, option.rect.top() + 6,
                       timecodeWidth, option.rect.height() - 12);
        if (timeRect.contains(me->pos())) {
          int midY = option.rect.top() + option.rect.height() / 2;
          if (me->pos().y() < midY) {
            delegate_->setEditZone(SubtitleListDelegate::EditZone::StartTime);
          } else {
            delegate_->setEditZone(SubtitleListDelegate::EditZone::EndTime);
          }
        } else {
          delegate_->setEditZone(SubtitleListDelegate::EditZone::Text);
        }
        listView_->edit(index);
        return true; // Handled to override QListView default editing triggers
      }
    }

    if (event->type() == QEvent::MouseButtonPress) {
      auto *me = static_cast<QMouseEvent *>(event);
      QModelIndex index = listView_->indexAt(me->pos());
      if (index.isValid()) {
        QStyleOptionViewItem option;
        option.initFrom(listView_);
        option.rect = listView_->visualRect(index);

        // Check Speaker Pill
        if (delegate_->speakerRect(option).contains(me->pos())) {
          showSpeakerMenu(index, me->globalPosition().toPoint());
          return true; // Handled
        }

        // Check Split Button
        if (delegate_->splitButtonRect(option).contains(me->pos())) {
          QString id =
              model_->data(index, SubtitleListModel::IdRole).toString();
          int pos = -1;
          QString text;
          if (delegate_->getActiveEditorInfo(index, pos, text)) {
            if (track_)
              track_->splitItem(id, pos, text);
          } else {
            if (track_)
              track_->splitItem(id, -1);
          }
          return true; // Handled
        }

        // Check Delete Button
        if (delegate_->deleteButtonRect(option).contains(me->pos())) {
          QString id =
              model_->data(index, SubtitleListModel::IdRole).toString();
          if (track_)
            track_->removeItem(id);
          return true; // Handled
        }
      }
    }

    if (event->type() == QEvent::MouseMove) {
      auto *me = static_cast<QMouseEvent *>(event);
      QModelIndex index = listView_->indexAt(me->pos());
      int button = 0;
      if (index.isValid()) {
        QStyleOptionViewItem option;
        option.initFrom(listView_);
        option.rect = listView_->visualRect(index);
        QRect splitRect = delegate_->splitButtonRect(option);
        QRect deleteRect = delegate_->deleteButtonRect(option);
        if (splitRect.contains(me->pos())) {
          button = 1;
        } else if (deleteRect.contains(me->pos())) {
          button = 2;
        }
      }
      delegate_->setHoveredIndex(index, button);

      // --- Add/Merge Overlay Logic ---
      if (track_) {
        const auto &items = track_->items();
        bool foundZone = false;
        const int threshold = 12; // px sensitivity

        // Check if mouse is near any item's top/bottom
        for (int i = 0; i <= items.size(); ++i) {
          int y = -1;
          qint64 gapStart = 0, gapEnd = 0;
          QString id1, id2;

          if (i < items.size()) {
            // Check top of item i
            QModelIndex idx = model_->index(i);
            QRect r = listView_->visualRect(idx);
            if (qAbs(me->pos().y() - r.top()) <= threshold) {
              y = r.top();
              gapEnd = items[i].startMs;
              id2 = items[i].id;
              if (i > 0) {
                gapStart = items[i - 1].endMs;
                id1 = items[i - 1].id;
              }
            }
          }

          if (y == -1 && i > 0) {
            // Check bottom of item i-1
            QModelIndex idx = model_->index(i - 1);
            QRect r = listView_->visualRect(idx);
            if (qAbs(me->pos().y() - r.bottom()) <= threshold) {
              y = r.bottom();
              gapStart = items[i - 1].endMs;
              id1 = items[i - 1].id;
              if (i < items.size()) {
                gapEnd = items[i].startMs;
                id2 = items[i].id;
              } else {
                gapEnd = totalDurationMs_;
              }
            }
          }

          if (y != -1) {
            // Map viewport Y to listContainer coordinates
            QPoint posInContainer = listView_->viewport()->mapTo(
                listView_->parentWidget(), QPoint(0, y));

            actionOverlay_->move(0, posInContainer.y() -
                                        actionOverlay_->height() / 2);
            actionOverlay_->resize(listView_->parentWidget()->width(),
                                   actionOverlay_->height());

            bool canMerge = !id1.isEmpty() && !id2.isEmpty();
            actionOverlay_->updateState(gapStart, gapEnd, canMerge, videoFps_);
            actionOverlay_->setProperty("id1", id1);
            actionOverlay_->setProperty("id2", id2);

            actionOverlay_->show();
            actionOverlay_->raise();
            foundZone = true;
            break;
          }
        }

        if (!foundZone) {
          // Only hide if the mouse isn't currently over the overlay itself
          if (actionOverlay_ && !actionOverlay_->underMouse()) {
            actionOverlay_->hide();
          }
        }
      }

      return false;
    } else if (event->type() == QEvent::Leave) {
      delegate_->setHoveredIndex(QModelIndex(), 0);
      if (actionOverlay_ && !actionOverlay_->underMouse()) {
        actionOverlay_->hide();
      }
      return false;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void SubtitleListPanel::leaveEvent(QEvent *event) {
  if (actionOverlay_ && !actionOverlay_->underMouse()) {
    actionOverlay_->hide();
  }
  QWidget::leaveEvent(event);
}

void SubtitleListPanel::showSpeakerMenu(const QModelIndex &index,
                                        const QPoint &globalPos) {
  if (!track_)
    return;

  QMenu menu(this);
  QAction *unassignAction = menu.addAction(tr("Unassigned"));
  unassignAction->setData(-1);

  menu.addSeparator();

  for (const auto &spk : track_->allSpeakers()) {
    QString label = spk.name.isEmpty()
                        ? QString("Speaker %1").arg(spk.id)
                        : QString("%1 (%2)").arg(spk.name).arg(spk.id);
    QAction *action = menu.addAction(label);
    action->setData(spk.id);
  }

  menu.addSeparator();
  QAction *newAction = menu.addAction(tr("+ New Speaker..."));
  QAction *manageAction = menu.addAction(tr("⚙️ Manage Speakers..."));

  QAction *chosen = menu.exec(globalPos);
  if (!chosen)
    return;

  if (chosen == manageAction) {
    SpeakerManagerDialog dlg(track_, this);
    dlg.exec();
    return;
  }
  if (chosen == newAction) {
    int nextId = 0;
    for (const auto &spk : track_->allSpeakers()) {
      if (spk.id >= nextId)
        nextId = spk.id + 1;
    }
    track_->autoRegisterSpeaker(nextId);
    model_->setData(index, nextId, SubtitleListModel::SpeakerIdRole);
    return;
  }

  int speakerId = chosen->data().toInt();
  model_->setData(index, speakerId, SubtitleListModel::SpeakerIdRole);
}

QWidget *SubtitleListPanel::createCustomStylePanel() {
  auto *mainContainer = new QWidget(this);
  mainContainer->setObjectName("CustomStyleMainContainer");
  auto *mainLayout = new QVBoxLayout(mainContainer);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  auto *scrollArea = new QScrollArea(mainContainer);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  auto *container = new QWidget(scrollArea);
  container->setObjectName("CustomStyleContainer");
  customStyleContainer_ = container;

  // 美化水平滑块为播放器类似的小圆形指针并适配高亮主题色
  container->setStyleSheet("QSlider::groove:horizontal {"
                           "  height: 4px;"
                           "  background: #3c3c3c;"
                           "  border-radius: 2px;"
                           "}"
                           "QSlider::groove:horizontal:disabled {"
                           "  background: #252525;"
                           "}"
                           "QSlider::sub-page:horizontal {"
                           "  background: palette(highlight);"
                           "  border-radius: 2px;"
                           "}"
                           "QSlider::sub-page:horizontal:disabled {"
                           "  background: #555555;"
                           "  border-radius: 2px;"
                           "}"
                           "QSlider::add-page:horizontal {"
                           "  background: #3c3c3c;"
                           "  border-radius: 2px;"
                           "}"
                           "QSlider::add-page:horizontal:disabled {"
                           "  background: #252525;"
                           "  border-radius: 2px;"
                           "}"
                           "QSlider::handle:horizontal {"
                           "  background: #d1d5db;"
                           "  width: 12px;"
                           "  height: 12px;"
                           "  margin-top: -4px;"
                           "  margin-bottom: -4px;"
                           "  border-radius: 6px;"
                           "}"
                           "QSlider::handle:horizontal:hover {"
                           "  background: #ffffff;"
                           "}"
                           "QSlider::handle:horizontal:disabled {"
                           "  background: #555555;"
                           "}");

  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(16);

  // 初始化折叠图标
  QPixmap downArrowPixmap(":/icons/down-arrow.svg");
  QTransform trans;
  trans.rotate(-90);
  QPixmap rightArrowPixmap =
      downArrowPixmap.transformed(trans, Qt::SmoothTransformation);

  // Helper for collapsible section frame
  auto createCollapsibleGroup =
      [&](const QString &title, const QString &objName, QFormLayout *formLayout,
          QCheckBox *toggleCheck, bool defaultExpanded) {
        auto *contentFrame = new QFrame(container);
        // 去除额外白框与背景颜色，直接平铺融入面板中
        contentFrame->setStyleSheet(
            "QFrame { background: transparent; border: none; }");
        contentFrame->setLayout(formLayout);
        formLayout->setContentsMargins(12, 12, 12, 12);
        formLayout->setSpacing(8);

        auto *headerButton = new QPushButton(container);
        headerButton->setFlat(true);
        headerButton->setFixedHeight(28);
        headerButton->setStyleSheet(
            "QPushButton { border: none; background: transparent; }");

        auto *headerInnerLayout = new QHBoxLayout(headerButton);
        headerInnerLayout->setContentsMargins(4, 0, 8, 0);
        headerInnerLayout->setSpacing(6);

        auto *headerLabel = new QLabel(title, headerButton);
        headerLabel->setObjectName(objName + "HeaderLabel");
        headerLabel->setStyleSheet(
            "font-weight: bold; font-size: 13px; color: palette(text);");

        auto *headerIcon = new QLabel(headerButton);
        headerIcon->setFixedSize(12, 12);
        headerIcon->setScaledContents(true);

        headerInnerLayout->addWidget(headerLabel, 0, Qt::AlignVCenter);
        headerInnerLayout->addWidget(headerIcon, 0, Qt::AlignVCenter);
        headerInnerLayout->addStretch();

        headerButton->setProperty("expanded", defaultExpanded);

        auto updateState = [=]() {
          bool checked = toggleCheck->isChecked();
          bool expanded = headerButton->property("expanded").toBool();

          // 即使不启用，也可以展开/折叠
          contentFrame->setVisible(expanded);

          headerIcon->setPixmap(expanded ? downArrowPixmap : rightArrowPixmap);

          // 置灰不可编辑
          contentFrame->setEnabled(checked);
          headerButton->setEnabled(true);
        };

        connect(headerButton, &QPushButton::clicked, this, [=]() {
          bool expanded = headerButton->property("expanded").toBool();
          headerButton->setProperty("expanded", !expanded);
          updateState();
        });

        connect(toggleCheck, &QCheckBox::stateChanged, this,
                [=](int /*state*/) { updateState(); });

        auto *headerLayout = new QHBoxLayout();
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(4);
        headerLayout->addWidget(toggleCheck);
        headerLayout->addWidget(headerButton, 1);

        layout->addLayout(headerLayout);
        layout->addWidget(contentFrame);

        updateState();
      };

  auto addFormRow = [&](QFormLayout *form, const QString &text,
                        const QString &objName, QWidget *field) {
    auto *label = new QLabel(text, container);
    label->setObjectName(objName);
    form->addRow(label, field);
  };

  auto setupCollapsibleGroupBoxHeader = [&](QGroupBox *groupBox, const QString &titleText,
                                            const QPixmap &iconPixmap, std::function<void()> collapseCallback) {
    groupBox->setTitle("");
    groupBox->setStyleSheet("QGroupBox::title { background: transparent; padding: 0px; margin: 0px; }");

    auto *headerBtn = new QPushButton(groupBox);
    headerBtn->setObjectName(groupBox->objectName() + "HeaderBtn");
    headerBtn->setFlat(true);
    headerBtn->setFixedHeight(20);
    headerBtn->setCursor(Qt::PointingHandCursor);
    headerBtn->setStyleSheet(
        "QPushButton { border: none; background-color: palette(window); padding: 0px; }");

    auto *headerLayout = new QHBoxLayout(headerBtn);
    headerLayout->setContentsMargins(4, 0, 4, 0);
    headerLayout->setSpacing(6);

    auto *lblTitle = new QLabel(titleText, headerBtn);
    lblTitle->setObjectName("lbl" + groupBox->objectName() + "HeaderTitle");
    lblTitle->setStyleSheet("font-weight: bold; font-size: 13px; color: palette(text); background: transparent;");

    auto *lblIcon = new QLabel(headerBtn);
    lblIcon->setFixedSize(12, 12);
    lblIcon->setScaledContents(true);
    lblIcon->setPixmap(iconPixmap);
    lblIcon->setStyleSheet("background: transparent;");

    headerLayout->addWidget(lblTitle, 0, Qt::AlignVCenter);
    headerLayout->addWidget(lblIcon, 0, Qt::AlignVCenter);
    headerLayout->addStretch();

    int textW = lblTitle->fontMetrics().horizontalAdvance(titleText);
    int totalW = textW + 6 + 12 + 8; // 6 spacing, 12 icon width, 8 padding (4px each side)
    headerBtn->setGeometry(12, 6, totalW, 20);

    connect(headerBtn, &QPushButton::clicked, this, collapseCallback);
  };

  // --- TEXT FILL GROUP ---
  fillForm_ = new QFormLayout();
  fillForm_->setContentsMargins(0, 0, 0, 0);
  fillForm_->setSpacing(8);

  fillTypeCombo_ = new QComboBox(container);
  fillTypeCombo_->addItems({tr("Color Fill"), tr("Gradient Fill")});
  if (auto *view = fillTypeCombo_->view()) {
    if (QWidget *w = view->window()) {
      w->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint |
                        Qt::NoDropShadowWindowHint);
      w->setAttribute(Qt::WA_TranslucentBackground);
    }
  }
  addFormRow(fillForm_, tr("Type"), "lblFillType", fillTypeCombo_);

  fillColorBtn_ = new ColorButton(container);
  addFormRow(fillForm_, tr("Color 1"), "lblFillColor", fillColorBtn_);

  fillColor2Btn_ = new ColorButton(container);
  addFormRow(fillForm_, tr("Color 2"), "lblFillColor2", fillColor2Btn_);

  auto *angleContainer = new QWidget(container);
  auto *angleLayout = new QHBoxLayout(angleContainer);
  angleLayout->setContentsMargins(0, 0, 0, 0);
  angleLayout->setSpacing(8);
  fillAngleSlider_ = new ClickableSlider(Qt::Horizontal, angleContainer);
  fillAngleSlider_->setRange(0, 360);
  fillAngleSpin_ = new QSpinBox(angleContainer);
  fillAngleSpin_->setRange(0, 360);
  angleLayout->addWidget(fillAngleSlider_);
  angleLayout->addWidget(fillAngleSpin_);
  addFormRow(fillForm_, tr("Angle"), "lblFillAngle", angleContainer);

  textOpacitySlider_ = new ClickableSlider(Qt::Horizontal, container);
  textOpacitySlider_->setRange(0, 100);
  addFormRow(fillForm_, tr("Opacity"), "lblFillOpacity", textOpacitySlider_);

  fillEnableCheck_ = new QCheckBox(container);
  createCollapsibleGroup(tr("Fill"), "Fill", fillForm_, fillEnableCheck_, true);

  // --- OUTLINE GROUP ---
  auto *strokeForm = new QFormLayout();
  strokeForm->setContentsMargins(0, 0, 0, 0);
  strokeForm->setSpacing(8);

  strokeColorBtn_ = new ColorButton(container);
  addFormRow(strokeForm, tr("Color"), "lblStrokeColor", strokeColorBtn_);

  strokeWidthSpin_ = new QSpinBox(container);
  strokeWidthSpin_->setRange(1, 20);
  addFormRow(strokeForm, tr("Thickness"), "lblStrokeThickness",
             strokeWidthSpin_);

  strokeOpacitySlider_ = new ClickableSlider(Qt::Horizontal, container);
  strokeOpacitySlider_->setRange(0, 100);
  addFormRow(strokeForm, tr("Opacity"), "lblStrokeOpacity",
             strokeOpacitySlider_);

  strokeEnableCheck_ = new QCheckBox(container);
  createCollapsibleGroup(tr("Outline"), "Outline", strokeForm,
                         strokeEnableCheck_, false);

  // --- SHADOW GROUP ---
  auto *shadowForm = new QFormLayout();
  shadowForm->setContentsMargins(0, 0, 0, 0);
  shadowForm->setSpacing(8);

  shadowColorBtn_ = new ColorButton(container);
  addFormRow(shadowForm, tr("Color"), "lblShadowColor", shadowColorBtn_);

  shadowOffsetXSpin_ = new QSpinBox(container);
  shadowOffsetXSpin_->setRange(-30, 30);
  addFormRow(shadowForm, tr("L/R Offset"), "lblShadowOffsetX",
             shadowOffsetXSpin_);

  shadowOffsetYSpin_ = new QSpinBox(container);
  shadowOffsetYSpin_->setRange(-30, 30);
  addFormRow(shadowForm, tr("T/B Offset"), "lblShadowOffsetY",
             shadowOffsetYSpin_);

  shadowBlurSlider_ = new ClickableSlider(Qt::Horizontal, container);
  shadowBlurSlider_->setRange(0, 20);
  addFormRow(shadowForm, tr("Blur"), "lblShadowBlur", shadowBlurSlider_);

  shadowOpacitySlider_ = new ClickableSlider(Qt::Horizontal, container);
  shadowOpacitySlider_->setRange(0, 100);
  addFormRow(shadowForm, tr("Opacity"), "lblShadowOpacity",
             shadowOpacitySlider_);

  shadowEnableCheck_ = new QCheckBox(container);
  createCollapsibleGroup(tr("Shadow"), "Shadow", shadowForm, shadowEnableCheck_,
                         false);

  // --- BACKGROUND GROUP ---
  bgForm_ = new QFormLayout();
  bgForm_->setContentsMargins(0, 0, 0, 0);
  bgForm_->setSpacing(8);

  bgColorBtn_ = new ColorButton(container);
  addFormRow(bgForm_, tr("Color"), "lblBgColor", bgColorBtn_);

  bgOpacitySlider_ = new ClickableSlider(Qt::Horizontal, container);
  bgOpacitySlider_->setRange(0, 100);
  addFormRow(bgForm_, tr("Opacity"), "lblBgOpacity", bgOpacitySlider_);

  bgRoundnessSlider_ = new ClickableSlider(Qt::Horizontal, container);
  bgRoundnessSlider_->setRange(0, 50);
  addFormRow(bgForm_, tr("Roundness"), "lblBgRoundness", bgRoundnessSlider_);

  auto *bgOffsetContainer = new QWidget(container);
  auto *bgOffsetLayout = new QHBoxLayout(bgOffsetContainer);
  bgOffsetLayout->setContentsMargins(0, 0, 0, 0);
  bgOffsetLayout->setSpacing(8);

  bgOffsetXSpin_ = new QSpinBox(bgOffsetContainer);
  bgOffsetXSpin_->setRange(-200, 200);

  bgOffsetYSpin_ = new QSpinBox(bgOffsetContainer);
  bgOffsetYSpin_->setRange(-200, 200);

  auto *lblBgOffsetX = new QLabel(tr("L/R Offset"), bgOffsetContainer);
  lblBgOffsetX->setObjectName("lblBgOffsetX");

  auto *lblBgOffsetY = new QLabel(tr("T/B Offset"), bgOffsetContainer);
  lblBgOffsetY->setObjectName("lblBgOffsetY");

  bgOffsetLayout->addWidget(lblBgOffsetX);
  bgOffsetLayout->addWidget(bgOffsetXSpin_, 1);
  bgOffsetLayout->addSpacing(16);
  bgOffsetLayout->addWidget(lblBgOffsetY);
  bgOffsetLayout->addWidget(bgOffsetYSpin_, 1);

  bgForm_->addRow(bgOffsetContainer);

  bgPaddingUniformContainer_ = new QWidget(container);
  auto *bgPaddingUniformLayout = new QHBoxLayout(bgPaddingUniformContainer_);
  bgPaddingUniformLayout->setContentsMargins(0, 0, 0, 0);
  bgPaddingUniformLayout->setSpacing(8);

  bgPaddingUniformSpin_ = new QSpinBox(bgPaddingUniformContainer_);
  bgPaddingUniformSpin_->setRange(0, 200);

  auto *btnExpandBgPadding = new QPushButton(bgPaddingUniformContainer_);
  btnExpandBgPadding->setFixedWidth(20);
  btnExpandBgPadding->setFlat(true);
  btnExpandBgPadding->setIcon(QIcon(rightArrowPixmap));
  btnExpandBgPadding->setIconSize(QSize(12, 12));
  btnExpandBgPadding->setStyleSheet("QPushButton { border: none; background: transparent; }");

  bgPaddingUniformLayout->addWidget(bgPaddingUniformSpin_, 1);
  bgPaddingUniformLayout->addWidget(btnExpandBgPadding);

  addFormRow(bgForm_, tr("Background Padding"), "lblBgPaddingUniformTitle", bgPaddingUniformContainer_);

  bgPaddingGroup_ = new QGroupBox(tr("Background Padding"), container);
  bgPaddingGroup_->setObjectName("BgPaddingGroup");
  auto *bgPgLayout = new QVBoxLayout(bgPaddingGroup_);
  bgPgLayout->setContentsMargins(6, 12, 6, 6);
  bgPgLayout->setSpacing(6);

  auto *bgLrContainer = new QWidget(bgPaddingGroup_);
  auto *bgLrLayout = new QHBoxLayout(bgLrContainer);
  bgLrLayout->setContentsMargins(0, 0, 0, 0);
  bgLrLayout->setSpacing(8);
  bgPaddingLeftSpin_ = new QSpinBox(bgLrContainer);
  bgPaddingLeftSpin_->setRange(0, 200);
  bgPaddingRightSpin_ = new QSpinBox(bgLrContainer);
  bgPaddingRightSpin_->setRange(0, 200);
  auto *lblBgPaddingLeft = new QLabel(tr("Left Padding"), bgLrContainer);
  lblBgPaddingLeft->setObjectName("lblBgPaddingLeft");
  auto *lblBgPaddingRight = new QLabel(tr("Right Padding"), bgLrContainer);
  lblBgPaddingRight->setObjectName("lblBgPaddingRight");
  bgLrLayout->addWidget(lblBgPaddingLeft);
  bgLrLayout->addWidget(bgPaddingLeftSpin_, 1);
  bgLrLayout->addSpacing(16);
  bgLrLayout->addWidget(lblBgPaddingRight);
  bgLrLayout->addWidget(bgPaddingRightSpin_, 1);
  bgPgLayout->addWidget(bgLrContainer);

  auto *bgTbContainer = new QWidget(bgPaddingGroup_);
  auto *bgTbLayout = new QHBoxLayout(bgTbContainer);
  bgTbLayout->setContentsMargins(0, 0, 0, 0);
  bgTbLayout->setSpacing(8);
  bgPaddingTopSpin_ = new QSpinBox(bgTbContainer);
  bgPaddingTopSpin_->setRange(0, 200);
  bgPaddingBottomSpin_ = new QSpinBox(bgTbContainer);
  bgPaddingBottomSpin_->setRange(0, 200);
  auto *lblBgPaddingTop = new QLabel(tr("Top Padding"), bgTbContainer);
  lblBgPaddingTop->setObjectName("lblBgPaddingTop");
  auto *lblBgPaddingBottom = new QLabel(tr("Bottom Padding"), bgTbContainer);
  lblBgPaddingBottom->setObjectName("lblBgPaddingBottom");
  bgTbLayout->addWidget(lblBgPaddingTop);
  bgTbLayout->addWidget(bgPaddingTopSpin_, 1);
  bgTbLayout->addSpacing(16);
  bgTbLayout->addWidget(lblBgPaddingBottom);
  bgTbLayout->addWidget(bgPaddingBottomSpin_, 1);
  bgPgLayout->addWidget(bgTbContainer);

  bgForm_->addRow(bgPaddingGroup_);

  auto toggleBgPaddingMode = [this](bool expand) {
    bgPaddingUniformContainer_->setVisible(!expand);
    if (auto *lbl = bgForm_->labelForField(bgPaddingUniformContainer_))
      lbl->setVisible(!expand);
    bgPaddingGroup_->setVisible(expand);
  };

  connect(btnExpandBgPadding, &QPushButton::clicked, this, [toggleBgPaddingMode]() {
    toggleBgPaddingMode(true);
  });

  setupCollapsibleGroupBoxHeader(bgPaddingGroup_, tr("Background Padding"), downArrowPixmap, [this, toggleBgPaddingMode]() {
    toggleBgPaddingMode(false);
  });

  connect(bgPaddingUniformSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int val) {
            bgPaddingLeftSpin_->setValue(val);
            bgPaddingRightSpin_->setValue(val);
            bgPaddingTopSpin_->setValue(val);
            bgPaddingBottomSpin_->setValue(val);
          });
  toggleBgPaddingMode(false);

  bgEnableCheck_ = new QCheckBox(container);
  createCollapsibleGroup(tr("Background"), "Background", bgForm_,
                         bgEnableCheck_, false);

  // --- BUBBLE GROUP ---
  bubbleForm_ = new QFormLayout();
  bubbleForm_->setContentsMargins(0, 0, 0, 0);
  bubbleForm_->setSpacing(8);

  auto *bubbleImageContainer = new QWidget(container);
  auto *bubbleImageLayout = new QHBoxLayout(bubbleImageContainer);
  bubbleImageLayout->setContentsMargins(0, 0, 0, 0);
  bubbleImageLayout->setSpacing(4);
  bubbleImagePathEdit_ = new QLineEdit(bubbleImageContainer);
  bubbleImagePathEdit_->setObjectName("BubbleImagePathEdit");
  bubbleImagePathEdit_->setReadOnly(true);
  bubbleImageBrowse_ = new QPushButton(tr("Browse..."), bubbleImageContainer);
  bubbleImageBrowse_->setObjectName("BubbleImageBrowseBtn");
  bubbleImageBrowse_->setFixedWidth(80);
  bubbleImageLayout->addWidget(bubbleImagePathEdit_);
  bubbleImageLayout->addWidget(bubbleImageBrowse_);
  addFormRow(bubbleForm_, tr("Image"), "lblBubbleImage", bubbleImageContainer);

  bubblePaddingUniformContainer_ = new QWidget(container);
  auto *bubblePaddingUniformLayout = new QHBoxLayout(bubblePaddingUniformContainer_);
  bubblePaddingUniformLayout->setContentsMargins(0, 0, 0, 0);
  bubblePaddingUniformLayout->setSpacing(8);

  bubblePaddingUniformSpin_ = new QSpinBox(bubblePaddingUniformContainer_);
  bubblePaddingUniformSpin_->setRange(0, 200);

  auto *btnExpandBubblePadding = new QPushButton(bubblePaddingUniformContainer_);
  btnExpandBubblePadding->setFixedWidth(20);
  btnExpandBubblePadding->setFlat(true);
  btnExpandBubblePadding->setIcon(QIcon(rightArrowPixmap));
  btnExpandBubblePadding->setIconSize(QSize(12, 12));
  btnExpandBubblePadding->setStyleSheet("QPushButton { border: none; background: transparent; }");

  bubblePaddingUniformLayout->addWidget(bubblePaddingUniformSpin_, 1);
  bubblePaddingUniformLayout->addWidget(btnExpandBubblePadding);

  addFormRow(bubbleForm_, tr("Text Padding"), "lblBubblePaddingUniformTitle", bubblePaddingUniformContainer_);

  bubblePaddingGroup_ = new QGroupBox(tr("Text Padding"), container);
  bubblePaddingGroup_->setObjectName("BubblePaddingGroup");
  auto *pgLayout = new QVBoxLayout(bubblePaddingGroup_);
  pgLayout->setContentsMargins(6, 12, 6, 6);
  pgLayout->setSpacing(6);

  auto *lrContainer = new QWidget(bubblePaddingGroup_);
  auto *lrLayout = new QHBoxLayout(lrContainer);
  lrLayout->setContentsMargins(0, 0, 0, 0);
  lrLayout->setSpacing(8);
  bubblePaddingLeftSpin_ = new QSpinBox(lrContainer);
  bubblePaddingLeftSpin_->setRange(0, 200);
  bubblePaddingRightSpin_ = new QSpinBox(lrContainer);
  bubblePaddingRightSpin_->setRange(0, 200);
  auto *lblBubblePaddingLeft = new QLabel(tr("Left Padding"), lrContainer);
  lblBubblePaddingLeft->setObjectName("lblBubblePaddingLeft");
  auto *lblBubblePaddingRight = new QLabel(tr("Right Padding"), lrContainer);
  lblBubblePaddingRight->setObjectName("lblBubblePaddingRight");
  lrLayout->addWidget(lblBubblePaddingLeft);
  lrLayout->addWidget(bubblePaddingLeftSpin_, 1);
  lrLayout->addSpacing(16);
  lrLayout->addWidget(lblBubblePaddingRight);
  lrLayout->addWidget(bubblePaddingRightSpin_, 1);
  pgLayout->addWidget(lrContainer);

  auto *tbContainer = new QWidget(bubblePaddingGroup_);
  auto *tbLayout = new QHBoxLayout(tbContainer);
  tbLayout->setContentsMargins(0, 0, 0, 0);
  tbLayout->setSpacing(8);
  bubblePaddingTopSpin_ = new QSpinBox(tbContainer);
  bubblePaddingTopSpin_->setRange(0, 200);
  bubblePaddingBottomSpin_ = new QSpinBox(tbContainer);
  bubblePaddingBottomSpin_->setRange(0, 200);
  auto *lblBubblePaddingTop = new QLabel(tr("Top Padding"), tbContainer);
  lblBubblePaddingTop->setObjectName("lblBubblePaddingTop");
  auto *lblBubblePaddingBottom = new QLabel(tr("Bottom Padding"), tbContainer);
  lblBubblePaddingBottom->setObjectName("lblBubblePaddingBottom");
  tbLayout->addWidget(lblBubblePaddingTop);
  tbLayout->addWidget(bubblePaddingTopSpin_, 1);
  tbLayout->addSpacing(16);
  tbLayout->addWidget(lblBubblePaddingBottom);
  tbLayout->addWidget(bubblePaddingBottomSpin_, 1);
  pgLayout->addWidget(tbContainer);

  bubbleForm_->addRow(bubblePaddingGroup_);

  auto toggleBubblePaddingMode = [this](bool expand) {
    bubblePaddingUniformContainer_->setVisible(!expand);
    if (auto *lbl = bubbleForm_->labelForField(bubblePaddingUniformContainer_))
      lbl->setVisible(!expand);
    bubblePaddingGroup_->setVisible(expand);
  };

  connect(btnExpandBubblePadding, &QPushButton::clicked, this, [toggleBubblePaddingMode]() {
    toggleBubblePaddingMode(true);
  });

  setupCollapsibleGroupBoxHeader(bubblePaddingGroup_, tr("Text Padding"), downArrowPixmap, [this, toggleBubblePaddingMode]() {
    toggleBubblePaddingMode(false);
  });

  connect(bubblePaddingUniformSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int val) {
            bubblePaddingLeftSpin_->setValue(val);
            bubblePaddingRightSpin_->setValue(val);
            bubblePaddingTopSpin_->setValue(val);
            bubblePaddingBottomSpin_->setValue(val);
          });
  toggleBubblePaddingMode(false);

  bubbleSliceUniformContainer_ = new QWidget(container);
  auto *bubbleSliceUniformLayout = new QHBoxLayout(bubbleSliceUniformContainer_);
  bubbleSliceUniformLayout->setContentsMargins(0, 0, 0, 0);
  bubbleSliceUniformLayout->setSpacing(8);

  bubbleSliceUniformSpin_ = new QSpinBox(bubbleSliceUniformContainer_);
  bubbleSliceUniformSpin_->setRange(0, 200);

  auto *btnExpandBubbleSlice = new QPushButton(bubbleSliceUniformContainer_);
  btnExpandBubbleSlice->setFixedWidth(20);
  btnExpandBubbleSlice->setFlat(true);
  btnExpandBubbleSlice->setIcon(QIcon(rightArrowPixmap));
  btnExpandBubbleSlice->setIconSize(QSize(12, 12));
  btnExpandBubbleSlice->setStyleSheet("QPushButton { border: none; background: transparent; }");

  bubbleSliceUniformLayout->addWidget(bubbleSliceUniformSpin_, 1);
  bubbleSliceUniformLayout->addWidget(btnExpandBubbleSlice);

  addFormRow(bubbleForm_, tr("9-Patch Stretch"), "lblBubbleSliceUniformTitle", bubbleSliceUniformContainer_);

  bubbleSliceGroup_ = new QGroupBox(tr("9-Patch Stretch"), container);
  bubbleSliceGroup_->setObjectName("BubbleSliceGroup");
  auto *sgLayout = new QVBoxLayout(bubbleSliceGroup_);
  sgLayout->setContentsMargins(6, 12, 6, 6);
  sgLayout->setSpacing(6);

  auto *sliceLrContainer = new QWidget(bubbleSliceGroup_);
  auto *sliceLrLayout = new QHBoxLayout(sliceLrContainer);
  sliceLrLayout->setContentsMargins(0, 0, 0, 0);
  sliceLrLayout->setSpacing(8);
  bubbleSliceLeftSpin_ = new QSpinBox(sliceLrContainer);
  bubbleSliceLeftSpin_->setRange(0, 200);
  bubbleSliceRightSpin_ = new QSpinBox(sliceLrContainer);
  bubbleSliceRightSpin_->setRange(0, 200);
  auto *lblBubbleSliceLeft = new QLabel(tr("Left Slice"), sliceLrContainer);
  lblBubbleSliceLeft->setObjectName("lblBubbleSliceLeft");
  auto *lblBubbleSliceRight = new QLabel(tr("Right Slice"), sliceLrContainer);
  lblBubbleSliceRight->setObjectName("lblBubbleSliceRight");
  sliceLrLayout->addWidget(lblBubbleSliceLeft);
  sliceLrLayout->addWidget(bubbleSliceLeftSpin_, 1);
  sliceLrLayout->addSpacing(16);
  sliceLrLayout->addWidget(lblBubbleSliceRight);
  sliceLrLayout->addWidget(bubbleSliceRightSpin_, 1);
  sgLayout->addWidget(sliceLrContainer);

  auto *sliceTbContainer = new QWidget(bubbleSliceGroup_);
  auto *sliceTbLayout = new QHBoxLayout(sliceTbContainer);
  sliceTbLayout->setContentsMargins(0, 0, 0, 0);
  sliceTbLayout->setSpacing(8);
  bubbleSliceTopSpin_ = new QSpinBox(sliceTbContainer);
  bubbleSliceTopSpin_->setRange(0, 200);
  bubbleSliceBottomSpin_ = new QSpinBox(sliceTbContainer);
  bubbleSliceBottomSpin_->setRange(0, 200);
  auto *lblBubbleSliceTop = new QLabel(tr("Top Slice"), sliceTbContainer);
  lblBubbleSliceTop->setObjectName("lblBubbleSliceTop");
  auto *lblBubbleSliceBottom = new QLabel(tr("Bottom Slice"), sliceTbContainer);
  lblBubbleSliceBottom->setObjectName("lblBubbleSliceBottom");
  sliceTbLayout->addWidget(lblBubbleSliceTop);
  sliceTbLayout->addWidget(bubbleSliceTopSpin_, 1);
  sliceTbLayout->addSpacing(16);
  sliceTbLayout->addWidget(lblBubbleSliceBottom);
  sliceTbLayout->addWidget(bubbleSliceBottomSpin_, 1);
  sgLayout->addWidget(sliceTbContainer);

  bubbleForm_->addRow(bubbleSliceGroup_);

  auto toggleBubbleSliceMode = [this](bool expand) {
    bubbleSliceUniformContainer_->setVisible(!expand);
    if (auto *lbl = bubbleForm_->labelForField(bubbleSliceUniformContainer_))
      lbl->setVisible(!expand);
    bubbleSliceGroup_->setVisible(expand);
  };

  connect(btnExpandBubbleSlice, &QPushButton::clicked, this, [toggleBubbleSliceMode]() {
    toggleBubbleSliceMode(true);
  });

  setupCollapsibleGroupBoxHeader(bubbleSliceGroup_, tr("9-Patch Stretch"), downArrowPixmap, [this, toggleBubbleSliceMode]() {
    toggleBubbleSliceMode(false);
  });

  connect(bubbleSliceUniformSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int val) {
            bubbleSliceLeftSpin_->setValue(val);
            bubbleSliceRightSpin_->setValue(val);
            bubbleSliceTopSpin_->setValue(val);
            bubbleSliceBottomSpin_->setValue(val);
          });
  toggleBubbleSliceMode(false);

  bubbleEnableCheck_ = new QCheckBox(container);
  createCollapsibleGroup(tr("Bubble"), "Bubble", bubbleForm_, bubbleEnableCheck_,
                         false);

  // Connect Slider and Spinbox for Angle
  connect(fillAngleSlider_, &QSlider::valueChanged, fillAngleSpin_,
          &QSpinBox::setValue);
  connect(fillAngleSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
          fillAngleSlider_, &QSlider::setValue);

  connect(bubbleImageBrowse_, &QPushButton::clicked, this, [this]() {
    QString path =
        QFileDialog::getOpenFileName(this, tr("Select Bubble Image"), QString(),
                                     tr("Images (*.png *.jpg *.jpeg *.svg)"));
    if (!path.isEmpty()) {
      bubbleImagePathEdit_->setText(path);
      applyCustomStyleToActiveItem();
    }
  });

  // Setup live update slots
  auto triggerUpdate = [this]() { applyCustomStyleToActiveItem(); };

  // Connect interactive controls
  connect(fillEnableCheck_, &QCheckBox::stateChanged, this, triggerUpdate);
  connect(fillTypeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this, triggerUpdate]() {
            updateFillTypeFields();
            triggerUpdate();
          });
  connect(fillColorBtn_, &ColorButton::colorChanged, this, triggerUpdate);
  connect(fillColor2Btn_, &ColorButton::colorChanged, this, triggerUpdate);
  connect(fillAngleSlider_, &QSlider::valueChanged, this, triggerUpdate);
  connect(textOpacitySlider_, &QSlider::valueChanged, this, triggerUpdate);

  connect(strokeEnableCheck_, &QCheckBox::stateChanged, this, triggerUpdate);
  connect(strokeColorBtn_, &ColorButton::colorChanged, this, triggerUpdate);
  connect(strokeWidthSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          triggerUpdate);
  connect(strokeOpacitySlider_, &QSlider::valueChanged, this, triggerUpdate);

  connect(shadowEnableCheck_, &QCheckBox::stateChanged, this, triggerUpdate);
  connect(shadowColorBtn_, &ColorButton::colorChanged, this, triggerUpdate);
  connect(shadowOffsetXSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          triggerUpdate);
  connect(shadowOffsetYSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          triggerUpdate);
  connect(shadowBlurSlider_, &QSlider::valueChanged, this, triggerUpdate);
  connect(shadowOpacitySlider_, &QSlider::sliderReleased, this, triggerUpdate);

  connect(bgEnableCheck_, &QCheckBox::stateChanged, this, triggerUpdate);
  connect(bgColorBtn_, &ColorButton::colorChanged, this, triggerUpdate);
  connect(bgOpacitySlider_, &QSlider::valueChanged, this, triggerUpdate);
  connect(bgRoundnessSlider_, &QSlider::valueChanged, this, triggerUpdate);
  connect(bgPaddingLeftSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          triggerUpdate);
  connect(bgPaddingRightSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          triggerUpdate);
  connect(bgPaddingTopSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          triggerUpdate);
  connect(bgPaddingBottomSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          triggerUpdate);
  connect(bgOffsetXSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          triggerUpdate);
  connect(bgOffsetYSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          triggerUpdate);

  connect(bubbleEnableCheck_, &QCheckBox::stateChanged, this, triggerUpdate);
  connect(bubblePaddingLeftSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
          this, triggerUpdate);
  connect(bubblePaddingRightSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
          this, triggerUpdate);
  connect(bubblePaddingTopSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
          this, triggerUpdate);
  connect(bubblePaddingBottomSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
          this, triggerUpdate);
  connect(bubbleSliceLeftSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
          this, triggerUpdate);
  connect(bubbleSliceRightSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
          this, triggerUpdate);
  connect(bubbleSliceTopSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
          this, triggerUpdate);
  connect(bubbleSliceBottomSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
          this, triggerUpdate);

  connect(bgPaddingUniformSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, triggerUpdate);
  connect(bubblePaddingUniformSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, triggerUpdate);
  connect(bubbleSliceUniformSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, triggerUpdate);

  // Undo support on slider release / spinbox edit finished
  auto recordUndoState = [this]() {
    if (track_ && !currentSelectedId_.isEmpty()) {
      SubtitleItem item;
      for (const auto &it : track_->items()) {
        if (it.id == currentSelectedId_) {
          item = it;
          break;
        }
      }

      item.fillType =
          fillEnableCheck_->isChecked() ? fillTypeCombo_->currentIndex() : -1;
      item.fillColor = fillColorBtn_->color().name();
      item.fillColor2 = fillColor2Btn_->color().name();
      item.fillAngle = fillAngleSlider_->value();
      item.fillTexturePath = "";
      item.fillTextureTile = true;
      item.textOpacity = textOpacitySlider_->value() / 100.0;

      item.strokeEnabled = strokeEnableCheck_->isChecked();
      item.strokeColor = strokeColorBtn_->color().name();
      item.strokeWidth = strokeWidthSpin_->value();
      item.strokeOpacity = strokeOpacitySlider_->value() / 100.0;

      item.shadowEnabled = shadowEnableCheck_->isChecked();
      item.shadowColor = shadowColorBtn_->color().name();
      item.shadowOffsetX = shadowOffsetXSpin_->value();
      item.shadowOffsetY = shadowOffsetYSpin_->value();
      item.shadowBlur = shadowBlurSlider_->value();
      item.shadowOpacity = shadowOpacitySlider_->value() / 100.0;

      item.bgType = bgEnableCheck_->isChecked() ? 1 : 0;
      item.bgColor = bgColorBtn_->color().name();
      item.bgOpacity = bgOpacitySlider_->value() / 100.0;
      item.bgRoundness = bgRoundnessSlider_->value();
      item.bgPaddingLeft = bgPaddingLeftSpin_->value();
      item.bgPaddingRight = bgPaddingRightSpin_->value();
      item.bgPaddingTop = bgPaddingTopSpin_->value();
      item.bgPaddingBottom = bgPaddingBottomSpin_->value();
      item.bgOffsetX = bgOffsetXSpin_->value();
      item.bgOffsetY = bgOffsetYSpin_->value();

      item.bubbleEnabled = bubbleEnableCheck_->isChecked();
      item.bubbleImagePath = bubbleImagePathEdit_->text();
      item.bubblePaddingLeft = bubblePaddingLeftSpin_->value();
      item.bubblePaddingRight = bubblePaddingRightSpin_->value();
      item.bubblePaddingTop = bubblePaddingTopSpin_->value();
      item.bubblePaddingBottom = bubblePaddingBottomSpin_->value();
      item.bubbleSliceLeft = bubbleSliceLeftSpin_->value();
      item.bubbleSliceRight = bubbleSliceRightSpin_->value();
      item.bubbleSliceTop = bubbleSliceTopSpin_->value();
      item.bubbleSliceBottom = bubbleSliceBottomSpin_->value();

      track_->updateItem(currentSelectedId_, item);
    }
  };

  connect(fillAngleSlider_, &QSlider::sliderReleased, this, recordUndoState);
  connect(textOpacitySlider_, &QSlider::sliderReleased, this, recordUndoState);
  connect(strokeOpacitySlider_, &QSlider::sliderReleased, this,
          recordUndoState);
  connect(shadowBlurSlider_, &QSlider::sliderReleased, this, recordUndoState);
  connect(shadowOpacitySlider_, &QSlider::sliderReleased, this,
          recordUndoState);
  connect(bgOpacitySlider_, &QSlider::sliderReleased, this, recordUndoState);
  connect(bgRoundnessSlider_, &QSlider::sliderReleased, this, recordUndoState);

  connect(bgPaddingLeftSpin_, &QSpinBox::editingFinished, this, recordUndoState);
  connect(bgPaddingRightSpin_, &QSpinBox::editingFinished, this, recordUndoState);
  connect(bgPaddingTopSpin_, &QSpinBox::editingFinished, this, recordUndoState);
  connect(bgPaddingBottomSpin_, &QSpinBox::editingFinished, this, recordUndoState);
  connect(bgOffsetXSpin_, &QSpinBox::editingFinished, this, recordUndoState);
  connect(bgOffsetYSpin_, &QSpinBox::editingFinished, this, recordUndoState);
  connect(bubblePaddingLeftSpin_, &QSpinBox::editingFinished, this,
          recordUndoState);
  connect(bubblePaddingRightSpin_, &QSpinBox::editingFinished, this,
          recordUndoState);
  connect(bubblePaddingTopSpin_, &QSpinBox::editingFinished, this,
          recordUndoState);
  connect(bubblePaddingBottomSpin_, &QSpinBox::editingFinished, this,
          recordUndoState);
  connect(bubbleSliceLeftSpin_, &QSpinBox::editingFinished, this,
          recordUndoState);
  connect(bubbleSliceRightSpin_, &QSpinBox::editingFinished, this,
          recordUndoState);
  connect(bubbleSliceTopSpin_, &QSpinBox::editingFinished, this,
          recordUndoState);
  connect(bubbleSliceBottomSpin_, &QSpinBox::editingFinished, this,
          recordUndoState);

  connect(bgPaddingUniformSpin_, &QSpinBox::editingFinished, this, recordUndoState);
  connect(bubblePaddingUniformSpin_, &QSpinBox::editingFinished, this, recordUndoState);
  connect(bubbleSliceUniformSpin_, &QSpinBox::editingFinished, this, recordUndoState);

  // 添加拉伸弹簧，确保多余空间由底部弹簧吸收，从而使各展开组高度固定，不被强制拉伸
  layout->addStretch();

  scrollArea->setWidget(container);
  mainLayout->addWidget(scrollArea);

  // 初始化显隐状态
  updateFillTypeFields();

  // 保存预设按钮水平居中放置
  auto *btnContainer = new QWidget(mainContainer);
  auto *btnLayout = new QHBoxLayout(btnContainer);
  btnLayout->setContentsMargins(12, 8, 12, 12);
  btnLayout->setSpacing(8);
  btnLayout->setAlignment(Qt::AlignCenter);

  savePresetBtn_ = new QPushButton(tr("+ Save Current Style"), btnContainer);
  savePresetBtn_->setObjectName("SavePresetBtn");
  savePresetBtn_->setFixedSize(76, 28);
  savePresetBtn_->setStyleSheet(
      "QPushButton { background-color: #2c2c2c; border: 1px solid #444; "
      "border-radius: 4px; color: #eee; font-size: 11px; }"
      "QPushButton:hover { background-color: #3c3c3c; border-color: #555; }");

  btnLayout->addWidget(savePresetBtn_);

  mainLayout->addWidget(btnContainer);

  // 保存为预设点击逻辑
  connect(savePresetBtn_, &QPushButton::clicked, this, [this]() {
    if (!track_)
      return;

    SubtitleItem item;
    bool found = false;
    if (!currentSelectedId_.isEmpty()) {
      for (const auto &it : track_->items()) {
        if (it.id == currentSelectedId_) {
          item = it;
          found = true;
          break;
        }
      }
    }
    if (!found) {
      item = track_->defaultStyleItem();
    }

    QString key = "Custom Presets";
    QJsonArray array;
    QString existingRaw = ConfigManager::instance().getString(key, "data");
    if (!existingRaw.isEmpty()) {
      array = QJsonDocument::fromJson(existingRaw.toUtf8()).array();
    }

    int nextIdx = array.size() + 1;
    QString name = QString("Preset %1").arg(nextIdx);

    QJsonObject presetObj;
    presetObj["name"] = name;

    QJsonObject styleObj;
    styleObj["fillType"] = item.fillType;
    styleObj["fillColor"] = item.fillColor;
    styleObj["fillColor2"] = item.fillColor2;
    styleObj["fillAngle"] = item.fillAngle;
    styleObj["fillTexturePath"] = item.fillTexturePath;
    styleObj["fillTextureTile"] = item.fillTextureTile;
    styleObj["textOpacity"] = item.textOpacity;

    styleObj["strokeEnabled"] = item.strokeEnabled;
    styleObj["strokeWidth"] = item.strokeWidth;
    styleObj["strokeColor"] = item.strokeColor;
    styleObj["strokeOpacity"] = item.strokeOpacity;

    styleObj["shadowEnabled"] = item.shadowEnabled;
    styleObj["shadowOffsetX"] = item.shadowOffsetX;
    styleObj["shadowOffsetY"] = item.shadowOffsetY;
    styleObj["shadowBlur"] = item.shadowBlur;
    styleObj["shadowColor"] = item.shadowColor;
    styleObj["shadowOpacity"] = item.shadowOpacity;

    styleObj["bgType"] = item.bgType;
    styleObj["bgColor"] = item.bgColor;
    styleObj["bgOpacity"] = item.bgOpacity;
    styleObj["bgRoundness"] = item.bgRoundness;
    styleObj["bgPaddingLeft"] = item.bgPaddingLeft;
    styleObj["bgPaddingRight"] = item.bgPaddingRight;
    styleObj["bgPaddingTop"] = item.bgPaddingTop;
    styleObj["bgPaddingBottom"] = item.bgPaddingBottom;
    styleObj["bgOffsetX"] = item.bgOffsetX;
    styleObj["bgOffsetY"] = item.bgOffsetY;

    styleObj["bubbleEnabled"] = item.bubbleEnabled;
    styleObj["bubbleImagePath"] = item.bubbleImagePath;
    styleObj["bubblePaddingLeft"] = item.bubblePaddingLeft;
    styleObj["bubblePaddingRight"] = item.bubblePaddingRight;
    styleObj["bubblePaddingTop"] = item.bubblePaddingTop;
    styleObj["bubblePaddingBottom"] = item.bubblePaddingBottom;
    styleObj["bubbleSliceLeft"] = item.bubbleSliceLeft;
    styleObj["bubbleSliceRight"] = item.bubbleSliceRight;
    styleObj["bubbleSliceTop"] = item.bubbleSliceTop;
    styleObj["bubbleSliceBottom"] = item.bubbleSliceBottom;

    presetObj["style"] = styleObj;
    array.append(presetObj);

    ConfigManager::instance().setValue(
        key, "data", QJsonDocument(array).toJson(QJsonDocument::Compact));
    ConfigManager::instance().sync();

    // 自动切换到自定义预设页并更新显示
    if (presetTypeCombo_) {
      presetTypeCombo_->setCurrentIndex(1);
    }
    populatePresets();

    // 切换到预设 tab 页，让用户直观地看到新保存的预设
    if (tabPreset_) {
      tabPreset_->click();
    }
  });

  return mainContainer;
}

QIcon SubtitleListPanel::createPresetIcon(const SubtitleItem &style,
                                          const QSize &size) {
  QString svgContent = generateSvgForPreset(style);
  QByteArray svgData = svgContent.toUtf8();
  QSvgRenderer renderer(svgData);

  qreal dpr = devicePixelRatio();
  QImage image(size * dpr, QImage::Format_ARGB32_Premultiplied);
  image.setDevicePixelRatio(dpr);
  image.fill(Qt::transparent);

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  renderer.render(&painter, QRectF(0, 0, size.width(), size.height()));

  return QIcon(QPixmap::fromImage(image));
}

QString SubtitleListPanel::generateSvgForPreset(const SubtitleItem &style) {
  QString defs;
  QString fillAttr;
  QString strokeAttr;
  QString bgRect;
  QString paths;

  // 1. 填充渐变或纯色
  if (style.fillType == 1) {
    // 线性渐变角度处理
    double rad = (90 - style.fillAngle) * 3.14159265358979323846 / 180.0;
    double x1 = 50 - 50 * cos(rad);
    double y1 = 50 - 50 * sin(rad);
    double x2 = 50 + 50 * cos(rad);
    double y2 = 50 + 50 * sin(rad);

    defs += QString("    <linearGradient id=\"grad\" x1=\"%1%\" y1=\"%2%\" "
                    "x2=\"%3%\" y2=\"%4%\">\n"
                    "      <stop offset=\"0%\" stop-color=\"%5\" "
                    "stop-opacity=\"%6\" />\n"
                    "      <stop offset=\"100%\" stop-color=\"%7\" "
                    "stop-opacity=\"%8\" />\n"
                    "    </linearGradient>\n")
                .arg(x1)
                .arg(y1)
                .arg(x2)
                .arg(y2)
                .arg(style.fillColor)
                .arg(style.textOpacity)
                .arg(style.fillColor2)
                .arg(style.textOpacity);

    fillAttr = "url(#grad)";
  } else {
    fillAttr = style.fillColor;
  }

  // 2. 描边属性
  if (style.strokeEnabled && style.strokeWidth > 0) {
    // 为 80x80 画布缩放描边粗细
    double sw = qMax(1.0, style.strokeWidth * 0.5);
    strokeAttr =
        QString(" stroke=\"%1\" stroke-width=\"%2\" stroke-opacity=\"%3\" "
                "stroke-linejoin=\"round\" stroke-linecap=\"round\"")
            .arg(style.strokeColor)
            .arg(sw)
            .arg(style.strokeOpacity);
  } else {
    strokeAttr = " stroke=\"none\"";
  }

  // 3. 背景底框与气泡
  if (style.bubbleEnabled && !style.bubbleImagePath.isEmpty()) {
    bgRect += QString("  <rect x=\"4\" y=\"12\" width=\"72\" height=\"56\" "
                      "rx=\"6\" ry=\"6\" fill=\"#555555\" fill-opacity=\"0.4\" "
                      "stroke=\"#888888\" stroke-width=\"1.5\" "
                      "stroke-dasharray=\"3,3\" />\n");
  }

  if (style.bgType == 1) {
    double rx = qMin(15.0, style.bgRoundness * 0.5);
    double bx = 8.0 + style.bgOffsetX * 0.2;
    double by = 16.0 + style.bgOffsetY * 0.2;
    bgRect +=
        QString("  <rect x=\"%1\" y=\"%2\" width=\"64\" height=\"48\" "
                "rx=\"%3\" ry=\"%3\" fill=\"%4\" fill-opacity=\"%5\" />\n")
            .arg(bx)
            .arg(by)
            .arg(rx)
            .arg(style.bgColor)
            .arg(style.bgOpacity);
  }

  // 4. 手动阴影路径 (规避 Qt SVG 对 filter 阴影支持不佳的问题)
  // 字母 "A" 的基本矢量路径
  QString pathD = "M 25,60 L 37,20 L 43,20 L 55,60 L 47,60 L 44,48 L 36,48 L "
                  "33,60 Z M 37,43 L 43,43 L 40,32 Z";

  if (style.shadowEnabled && style.shadowOpacity > 0.0) {
    // 缩放阴影位移，在 80x80 图标上稍微放大一点以获得更明显的视觉反馈
    double dx = style.shadowOffsetX * 1.5;
    double dy = style.shadowOffsetY * 1.5;
    QString shColor = style.shadowColor;
    double opacity = style.shadowOpacity;
    double blur = qMax(1.0, style.shadowBlur * 0.8);

    // 三层叠加来模拟软模糊边缘效果
    // 外层极虚化投影
    paths += QString("  <path d=\"%1\" fill=\"%2\" fill-opacity=\"%3\" "
                     "stroke=\"%2\" stroke-width=\"%4\" stroke-opacity=\"%3\" "
                     "stroke-linejoin=\"round\" stroke-linecap=\"round\" "
                     "transform=\"translate(%5, %6)\" />\n")
                 .arg(pathD)
                 .arg(shColor)
                 .arg(opacity * 0.25)
                 .arg(blur * 1.5)
                 .arg(dx)
                 .arg(dy);

    // 中间虚化投影
    paths += QString("  <path d=\"%1\" fill=\"%2\" fill-opacity=\"%3\" "
                     "stroke=\"%2\" stroke-width=\"%4\" stroke-opacity=\"%3\" "
                     "stroke-linejoin=\"round\" stroke-linecap=\"round\" "
                     "transform=\"translate(%5, %6)\" />\n")
                 .arg(pathD)
                 .arg(shColor)
                 .arg(opacity * 0.5)
                 .arg(blur * 0.8)
                 .arg(dx)
                 .arg(dy);

    // 内层实体投影
    paths += QString("  <path d=\"%1\" fill=\"%2\" fill-opacity=\"%3\" "
                     "stroke=\"%2\" stroke-width=\"%4\" stroke-opacity=\"%3\" "
                     "stroke-linejoin=\"round\" stroke-linecap=\"round\" "
                     "transform=\"translate(%5, %6)\" />\n")
                 .arg(pathD)
                 .arg(shColor)
                 .arg(opacity * 0.8)
                 .arg(blur * 0.3)
                 .arg(dx)
                 .arg(dy);
  }

  // 5. 主体路径 "A"
  paths += QString("  <path d=\"%1\" fill=\"%2\" fill-opacity=\"%3\"%4 />\n")
               .arg(pathD)
               .arg(fillAttr)
               .arg(style.fillType == 1 ? 1.0 : style.textOpacity)
               .arg(strokeAttr);

  QString svg =
      QString("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"80\" "
              "height=\"80\" viewBox=\"0 0 80 80\">\n"
              "  <defs>\n"
              "%1"
              "  </defs>\n"
              "%2"
              "%3"
              "</svg>")
          .arg(defs)
          .arg(bgRect)
          .arg(paths);

  return svg;
}

QString SubtitleListPanel::writeSvgPresetFile(const QString &name,
                                              const SubtitleItem &style) {
  QString svg = generateSvgForPreset(style);
  QString filename = QString("preset_%1.svg").arg(name);
  filename.replace(" ", "_");
  QString path = QDir::tempPath() + "/" + filename;
  QFile file(path);
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    file.write(svg.toUtf8());
    file.close();
  }
  return path;
}

QWidget *SubtitleListPanel::createPresetStylePanel() {
  auto *container = new QWidget(this);
  container->setObjectName("PresetStyleContainer");
  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(12);

  // 下拉选择框：切换系统预设与自定义预设
  presetTypeCombo_ = new QComboBox(container);
  presetTypeCombo_->addItem(tr("System Presets"), 0);
  presetTypeCombo_->addItem(tr("Custom Presets"), 1);
  presetTypeCombo_->setFixedHeight(32);
  if (auto *view = presetTypeCombo_->view()) {
    if (QWidget *w = view->window()) {
      w->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint |
                        Qt::NoDropShadowWindowHint);
      w->setAttribute(Qt::WA_TranslucentBackground);
    }
  }
  layout->addWidget(presetTypeCombo_);

  // 列表容器：提供与字幕列表一致的背景色和边框
  auto *listContainer = new QFrame(container);
  listContainer->setObjectName("SubtitleListContainer");
  listContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto *lcLayout = new QVBoxLayout(listContainer);
  lcLayout->setContentsMargins(6, 6, 6, 6);
  lcLayout->setSpacing(0);

  // 流式布局的 QListWidget
  presetListWidget_ = new QListWidget(listContainer);
  presetListWidget_->setObjectName("PresetListWidget");
  presetListWidget_->setViewMode(QListView::IconMode);
  presetListWidget_->setResizeMode(QListView::Adjust);
  presetListWidget_->setMovement(QListView::Static);
  presetListWidget_->setFlow(QListView::LeftToRight);
  presetListWidget_->setWrapping(true);
  presetListWidget_->setSpacing(8);
  presetListWidget_->setUniformItemSizes(true);
  presetListWidget_->setGridSize(QSize(76, 76));
  presetListWidget_->setIconSize(QSize(60, 60));
  presetListWidget_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  presetListWidget_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  presetListWidget_->setContextMenuPolicy(Qt::CustomContextMenu);

  presetListWidget_->setStyleSheet(
      "QListWidget#PresetListWidget {"
      "  background-color: transparent;"
      "  border: none;"
      "}"
      "QListWidget#PresetListWidget::item {"
      "  background-color: #242424;"
      "  border: 1px solid #3c3c3c;"
      "  border-radius: 6px;"
      "}"
      "QListWidget#PresetListWidget::item:hover {"
      "  background-color: #2e2e2e;"
      "  border-color: #0088cc;"
      "}"
      "QListWidget#PresetListWidget::item:selected {"
      "  background-color: #2e2e2e;"
      "  border-color: #0088cc;"
      "}");

  lcLayout->addWidget(presetListWidget_);
  layout->addWidget(listContainer);

  // 切换类型连接
  connect(presetTypeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &SubtitleListPanel::populatePresets);

  // 点击应用预设
  connect(presetListWidget_, &QListWidget::itemClicked, this,
          [this](QListWidgetItem *item) {
            if (!item || !track_)
              return;
            QString jsonStr = item->data(Qt::UserRole).toString();
            QJsonObject styleObj =
                QJsonDocument::fromJson(jsonStr.toUtf8()).object();

            SubtitleItem style;
            style.fillType = styleObj["fillType"].toInt(0);
            style.fillColor = styleObj["fillColor"].toString("#FFFFFF");
            style.fillColor2 = styleObj["fillColor2"].toString("#FFFFFF");
            style.fillAngle = styleObj["fillAngle"].toInt(90);
            style.fillTexturePath = styleObj["fillTexturePath"].toString();
            style.fillTextureTile = styleObj["fillTextureTile"].toBool(true);
            style.textOpacity = styleObj["textOpacity"].toDouble(1.0);

            style.strokeEnabled = styleObj["strokeEnabled"].toBool(false);
            style.strokeWidth = styleObj["strokeWidth"].toInt(2);
            style.strokeColor = styleObj["strokeColor"].toString("#000000");
            style.strokeOpacity = styleObj["strokeOpacity"].toDouble(1.0);

            style.shadowEnabled = styleObj["shadowEnabled"].toBool(false);
            style.shadowOffsetX = styleObj["shadowOffsetX"].toInt(3);
            style.shadowOffsetY = styleObj["shadowOffsetY"].toInt(3);
            style.shadowBlur = styleObj["shadowBlur"].toInt(5);
            style.shadowColor = styleObj["shadowColor"].toString("#000000");
            style.shadowOpacity = styleObj["shadowOpacity"].toDouble(0.5);

            style.bgType = styleObj["bgType"].toInt(0);
            style.bgColor = styleObj["bgColor"].toString("#000000");
            style.bgOpacity = styleObj["bgOpacity"].toDouble(0.6);
            style.bgRoundness = styleObj["bgRoundness"].toInt(4);
            style.bgPaddingLeft = styleObj["bgPaddingLeft"].toInt(styleObj["bgPaddingX"].toInt(15));
            style.bgPaddingRight = styleObj["bgPaddingRight"].toInt(styleObj["bgPaddingX"].toInt(15));
            style.bgPaddingTop = styleObj["bgPaddingTop"].toInt(styleObj["bgPaddingY"].toInt(10));
            style.bgPaddingBottom = styleObj["bgPaddingBottom"].toInt(styleObj["bgPaddingY"].toInt(10));
            style.bgOffsetX = styleObj["bgOffsetX"].toInt(0);
            style.bgOffsetY = styleObj["bgOffsetY"].toInt(0);

            style.bubbleEnabled = styleObj["bubbleEnabled"].toBool(false);
            style.bubbleImagePath = styleObj["bubbleImagePath"].toString();
            style.bubblePaddingLeft = styleObj["bubblePaddingLeft"].toInt(15);
            style.bubblePaddingRight = styleObj["bubblePaddingRight"].toInt(15);
            style.bubblePaddingTop = styleObj["bubblePaddingTop"].toInt(10);
            style.bubblePaddingBottom =
                styleObj["bubblePaddingBottom"].toInt(10);
            style.bubbleSliceLeft =
                styleObj["bubbleSliceLeft"].toInt(style.bubblePaddingLeft);
            style.bubbleSliceRight =
                styleObj["bubbleSliceRight"].toInt(style.bubblePaddingRight);
            style.bubbleSliceTop =
                styleObj["bubbleSliceTop"].toInt(style.bubblePaddingTop);
            style.bubbleSliceBottom =
                styleObj["bubbleSliceBottom"].toInt(style.bubblePaddingBottom);

            if (!currentSelectedId_.isEmpty()) {
              SubtitleItem item;
              for (const auto &it : track_->items()) {
                if (it.id == currentSelectedId_) {
                  item = it;
                  break;
                }
              }
              item.fillType = style.fillType;
              item.fillColor = style.fillColor;
              item.fillColor2 = style.fillColor2;
              item.fillAngle = style.fillAngle;
              item.fillTexturePath = style.fillTexturePath;
              item.fillTextureTile = style.fillTextureTile;
              item.textOpacity = style.textOpacity;

              item.strokeEnabled = style.strokeEnabled;
              item.strokeWidth = style.strokeWidth;
              item.strokeColor = style.strokeColor;
              item.strokeOpacity = style.strokeOpacity;

              item.shadowEnabled = style.shadowEnabled;
              item.shadowOffsetX = style.shadowOffsetX;
              item.shadowOffsetY = style.shadowOffsetY;
              item.shadowBlur = style.shadowBlur;
              item.shadowColor = style.shadowColor;
              item.shadowOpacity = style.shadowOpacity;

              item.bgType = style.bgType;
              item.bgColor = style.bgColor;
              item.bgOpacity = style.bgOpacity;
              item.bgRoundness = style.bgRoundness;
              item.bgPaddingLeft = style.bgPaddingLeft;
              item.bgPaddingRight = style.bgPaddingRight;
              item.bgPaddingTop = style.bgPaddingTop;
              item.bgPaddingBottom = style.bgPaddingBottom;
              item.bgOffsetX = style.bgOffsetX;
              item.bgOffsetY = style.bgOffsetY;
              item.bubbleEnabled = style.bubbleEnabled;
              item.bubbleImagePath = style.bubbleImagePath;
              item.bubblePaddingLeft = style.bubblePaddingLeft;
              item.bubblePaddingRight = style.bubblePaddingRight;
              item.bubblePaddingTop = style.bubblePaddingTop;
              item.bubblePaddingBottom = style.bubblePaddingBottom;
              item.bubbleSliceLeft = style.bubbleSliceLeft;
              item.bubbleSliceRight = style.bubbleSliceRight;
              item.bubbleSliceTop = style.bubbleSliceTop;
              item.bubbleSliceBottom = style.bubbleSliceBottom;

              track_->updateItem(currentSelectedId_, item);
              loadStyleFromItem(item);
            } else {
              SubtitleItem item = track_->defaultStyleItem();
              item.fillType = style.fillType;
              item.fillColor = style.fillColor;
              item.fillColor2 = style.fillColor2;
              item.fillAngle = style.fillAngle;
              item.fillTexturePath = style.fillTexturePath;
              item.fillTextureTile = style.fillTextureTile;
              item.textOpacity = style.textOpacity;

              item.strokeEnabled = style.strokeEnabled;
              item.strokeWidth = style.strokeWidth;
              item.strokeColor = style.strokeColor;
              item.strokeOpacity = style.strokeOpacity;

              item.shadowEnabled = style.shadowEnabled;
              item.shadowOffsetX = style.shadowOffsetX;
              item.shadowOffsetY = style.shadowOffsetY;
              item.shadowBlur = style.shadowBlur;
              item.shadowColor = style.shadowColor;
              item.shadowOpacity = style.shadowOpacity;

              item.bgType = style.bgType;
              item.bgColor = style.bgColor;
              item.bgOpacity = style.bgOpacity;
              item.bgRoundness = style.bgRoundness;
              item.bgPaddingLeft = style.bgPaddingLeft;
              item.bgPaddingRight = style.bgPaddingRight;
              item.bgPaddingTop = style.bgPaddingTop;
              item.bgPaddingBottom = style.bgPaddingBottom;
              item.bgOffsetX = style.bgOffsetX;
              item.bgOffsetY = style.bgOffsetY;
              item.bubbleEnabled = style.bubbleEnabled;
              item.bubbleImagePath = style.bubbleImagePath;
              item.bubblePaddingLeft = style.bubblePaddingLeft;
              item.bubblePaddingRight = style.bubblePaddingRight;
              item.bubblePaddingTop = style.bubblePaddingTop;
              item.bubblePaddingBottom = style.bubblePaddingBottom;
              item.bubbleSliceLeft = style.bubbleSliceLeft;
              item.bubbleSliceRight = style.bubbleSliceRight;
              item.bubbleSliceTop = style.bubbleSliceTop;
              item.bubbleSliceBottom = style.bubbleSliceBottom;

              track_->setDefaultStyleItem(item);
              loadStyleFromItem(item);
            }
          });

  // 右键菜单删除自定义预设
  connect(presetListWidget_, &QListWidget::customContextMenuRequested, this,
          [this](const QPoint &pos) {
            QListWidgetItem *item = presetListWidget_->itemAt(pos);
            if (!item)
              return;
            bool isCustom = item->data(Qt::UserRole + 1).toBool();
            int customIndex = item->data(Qt::UserRole + 2).toInt();
            if (isCustom) {
              showPresetContextMenu(customIndex,
                                    presetListWidget_->mapToGlobal(pos));
            }
          });

  // 初始化填充预设数据
  populatePresets();

  return container;
}

void SubtitleListPanel::loadStyleFromItem(const SubtitleItem &item) {
  isUpdatingControls_ = true;

  if (fillEnableCheck_)
    fillEnableCheck_->setChecked(item.fillType >= 0);
  if (fillTypeCombo_) {
    int idx = (item.fillType >= 0) ? item.fillType : 0;
    if (idx > 1)
      idx = 0;
    fillTypeCombo_->setCurrentIndex(idx);
  }
  if (fillColorBtn_)
    fillColorBtn_->setColor(QColor(item.fillColor));
  if (fillColor2Btn_)
    fillColor2Btn_->setColor(QColor(item.fillColor2));
  if (fillAngleSlider_)
    fillAngleSlider_->setValue(item.fillAngle);
  if (fillAngleSpin_)
    fillAngleSpin_->setValue(item.fillAngle);
  if (textOpacitySlider_)
    textOpacitySlider_->setValue(qRound(item.textOpacity * 100.0));

  if (strokeEnableCheck_)
    strokeEnableCheck_->setChecked(item.strokeEnabled);
  if (strokeColorBtn_)
    strokeColorBtn_->setColor(QColor(item.strokeColor));
  if (strokeWidthSpin_)
    strokeWidthSpin_->setValue(item.strokeWidth);
  if (strokeOpacitySlider_)
    strokeOpacitySlider_->setValue(qRound(item.strokeOpacity * 100.0));

  if (shadowEnableCheck_)
    shadowEnableCheck_->setChecked(item.shadowEnabled);
  if (shadowColorBtn_)
    shadowColorBtn_->setColor(QColor(item.shadowColor));
  if (shadowOffsetXSpin_)
    shadowOffsetXSpin_->setValue(item.shadowOffsetX);
  if (shadowOffsetYSpin_)
    shadowOffsetYSpin_->setValue(item.shadowOffsetY);
  if (shadowBlurSlider_)
    shadowBlurSlider_->setValue(item.shadowBlur);
  if (shadowOpacitySlider_)
    shadowOpacitySlider_->setValue(qRound(item.shadowOpacity * 100.0));

  if (bgEnableCheck_)
    bgEnableCheck_->setChecked(item.bgType > 0);
  if (bgColorBtn_)
    bgColorBtn_->setColor(QColor(item.bgColor));
  if (bgOpacitySlider_)
    bgOpacitySlider_->setValue(qRound(item.bgOpacity * 100.0));
  if (bgRoundnessSlider_)
    bgRoundnessSlider_->setValue(item.bgRoundness);
  if (bgPaddingLeftSpin_)
    bgPaddingLeftSpin_->setValue(item.bgPaddingLeft);
  if (bgPaddingRightSpin_)
    bgPaddingRightSpin_->setValue(item.bgPaddingRight);
  if (bgPaddingTopSpin_)
    bgPaddingTopSpin_->setValue(item.bgPaddingTop);
  if (bgPaddingBottomSpin_)
    bgPaddingBottomSpin_->setValue(item.bgPaddingBottom);
  if (bgPaddingUniformSpin_) {
    bool bgUniform = (item.bgPaddingLeft == item.bgPaddingRight &&
                      item.bgPaddingLeft == item.bgPaddingTop &&
                      item.bgPaddingLeft == item.bgPaddingBottom);
    bool expand = !bgUniform;
    bgPaddingUniformSpin_->setValue(item.bgPaddingLeft);
    bgPaddingUniformContainer_->setVisible(!expand);
    if (auto *lbl = bgForm_->labelForField(bgPaddingUniformContainer_))
      lbl->setVisible(!expand);
    bgPaddingGroup_->setVisible(expand);
  }
  if (bgOffsetXSpin_)
    bgOffsetXSpin_->setValue(item.bgOffsetX);
  if (bgOffsetYSpin_)
    bgOffsetYSpin_->setValue(item.bgOffsetY);

  if (bubbleEnableCheck_)
    bubbleEnableCheck_->setChecked(item.bubbleEnabled);
  if (bubbleImagePathEdit_)
    bubbleImagePathEdit_->setText(item.bubbleImagePath);
  if (bubblePaddingLeftSpin_)
    bubblePaddingLeftSpin_->setValue(item.bubblePaddingLeft);
  if (bubblePaddingRightSpin_)
    bubblePaddingRightSpin_->setValue(item.bubblePaddingRight);
  if (bubblePaddingTopSpin_)
    bubblePaddingTopSpin_->setValue(item.bubblePaddingTop);
  if (bubblePaddingBottomSpin_)
    bubblePaddingBottomSpin_->setValue(item.bubblePaddingBottom);

  if (bubblePaddingUniformSpin_) {
    bool bubblePadUniform = (item.bubblePaddingLeft == item.bubblePaddingRight &&
                             item.bubblePaddingLeft == item.bubblePaddingTop &&
                             item.bubblePaddingLeft == item.bubblePaddingBottom);
    bool expand = !bubblePadUniform;
    bubblePaddingUniformSpin_->setValue(item.bubblePaddingLeft);
    bubblePaddingUniformContainer_->setVisible(!expand);
    if (auto *lbl = bubbleForm_->labelForField(bubblePaddingUniformContainer_))
      lbl->setVisible(!expand);
    bubblePaddingGroup_->setVisible(expand);
  }

  if (bubbleSliceLeftSpin_)
    bubbleSliceLeftSpin_->setValue(item.bubbleSliceLeft);
  if (bubbleSliceRightSpin_)
    bubbleSliceRightSpin_->setValue(item.bubbleSliceRight);
  if (bubbleSliceTopSpin_)
    bubbleSliceTopSpin_->setValue(item.bubbleSliceTop);
  if (bubbleSliceBottomSpin_)
    bubbleSliceBottomSpin_->setValue(item.bubbleSliceBottom);

  if (bubbleSliceUniformSpin_) {
    bool bubbleSliceUniform = (item.bubbleSliceLeft == item.bubbleSliceRight &&
                               item.bubbleSliceLeft == item.bubbleSliceTop &&
                               item.bubbleSliceLeft == item.bubbleSliceBottom);
    bool expand = !bubbleSliceUniform;
    bubbleSliceUniformSpin_->setValue(item.bubbleSliceLeft);
    bubbleSliceUniformContainer_->setVisible(!expand);
    if (auto *lbl = bubbleForm_->labelForField(bubbleSliceUniformContainer_))
      lbl->setVisible(!expand);
    bubbleSliceGroup_->setVisible(expand);
  }

  isUpdatingControls_ = false;

  if (customStyleContainer_) {
    customStyleContainer_->setEnabled(!currentSelectedId_.isEmpty());
  }
  if (savePresetBtn_) {
    savePresetBtn_->setEnabled(!currentSelectedId_.isEmpty());
  }

  updateFillTypeFields();
}

void SubtitleListPanel::applyCustomStyleToActiveItem() {
  if (isUpdatingControls_)
    return;
  if (!track_)
    return;

  QString activeId = currentSelectedId_;
  SubtitleItem item;
  bool found = false;
  if (!activeId.isEmpty()) {
    for (const auto &it : track_->items()) {
      if (it.id == activeId) {
        item = it;
        found = true;
        break;
      }
    }
  }

  if (!found) {
    item = track_->defaultStyleItem();
  }

  item.fillType =
      fillEnableCheck_->isChecked() ? fillTypeCombo_->currentIndex() : -1;
  item.fillColor = fillColorBtn_->color().name();
  item.fillColor2 = fillColor2Btn_->color().name();
  item.fillAngle = fillAngleSlider_->value();
  item.fillTexturePath = "";
  item.fillTextureTile = true;
  item.textOpacity = textOpacitySlider_->value() / 100.0;

  item.strokeEnabled = strokeEnableCheck_->isChecked();
  item.strokeColor = strokeColorBtn_->color().name();
  item.strokeWidth = strokeWidthSpin_->value();
  item.strokeOpacity = strokeOpacitySlider_->value() / 100.0;

  item.shadowEnabled = shadowEnableCheck_->isChecked();
  item.shadowColor = shadowColorBtn_->color().name();
  item.shadowOffsetX = shadowOffsetXSpin_->value();
  item.shadowOffsetY = shadowOffsetYSpin_->value();
  item.shadowBlur = shadowBlurSlider_->value();
  item.shadowOpacity = shadowOpacitySlider_->value() / 100.0;

  item.bgType = bgEnableCheck_->isChecked() ? 1 : 0;
  item.bgColor = bgColorBtn_->color().name();
  item.bgOpacity = bgOpacitySlider_->value() / 100.0;
  item.bgRoundness = bgRoundnessSlider_->value();
  item.bgPaddingLeft = bgPaddingLeftSpin_->value();
  item.bgPaddingRight = bgPaddingRightSpin_->value();
  item.bgPaddingTop = bgPaddingTopSpin_->value();
  item.bgPaddingBottom = bgPaddingBottomSpin_->value();
  item.bgOffsetX = bgOffsetXSpin_->value();
  item.bgOffsetY = bgOffsetYSpin_->value();

  item.bubbleEnabled = bubbleEnableCheck_->isChecked();
  item.bubbleImagePath = bubbleImagePathEdit_->text();
  item.bubblePaddingLeft = bubblePaddingLeftSpin_->value();
  item.bubblePaddingRight = bubblePaddingRightSpin_->value();
  item.bubblePaddingTop = bubblePaddingTopSpin_->value();
  item.bubblePaddingBottom = bubblePaddingBottomSpin_->value();
  item.bubbleSliceLeft = bubbleSliceLeftSpin_->value();
  item.bubbleSliceRight = bubbleSliceRightSpin_->value();
  item.bubbleSliceTop = bubbleSliceTopSpin_->value();
  item.bubbleSliceBottom = bubbleSliceBottomSpin_->value();

  if (found) {
    track_->updateItemDirect(activeId, item);
  } else {
    track_->setDefaultStyleItem(item);
  }
}

void SubtitleListPanel::loadCustomPresets() {
  if (!presetListWidget_)
    return;

  presetListWidget_->clear();

  QString key = "Custom Presets";
  QString existingRaw = ConfigManager::instance().getString(key, "data");
  if (existingRaw.isEmpty())
    return;

  QJsonArray array = QJsonDocument::fromJson(existingRaw.toUtf8()).array();
  for (int i = 0; i < array.size(); ++i) {
    QJsonObject presetObj = array[i].toObject();
    QString name = presetObj["name"].toString();
    QJsonObject styleObj = presetObj["style"].toObject();

    SubtitleItem item;
    item.fillType = styleObj["fillType"].toInt(0);
    item.fillColor = styleObj["fillColor"].toString("#FFFFFF");
    item.fillColor2 = styleObj["fillColor2"].toString("#FFFFFF");
    item.fillAngle = styleObj["fillAngle"].toInt(90);
    item.fillTexturePath = styleObj["fillTexturePath"].toString();
    item.fillTextureTile = styleObj["fillTextureTile"].toBool(true);
    item.textOpacity = styleObj["textOpacity"].toDouble(1.0);

    item.strokeEnabled = styleObj["strokeEnabled"].toBool(false);
    item.strokeWidth = styleObj["strokeWidth"].toInt(2);
    item.strokeColor = styleObj["strokeColor"].toString("#000000");
    item.strokeOpacity = styleObj["strokeOpacity"].toDouble(1.0);

    item.shadowEnabled = styleObj["shadowEnabled"].toBool(false);
    item.shadowOffsetX = styleObj["shadowOffsetX"].toInt(3);
    item.shadowOffsetY = styleObj["shadowOffsetY"].toInt(3);
    item.shadowBlur = styleObj["shadowBlur"].toInt(5);
    item.shadowColor = styleObj["shadowColor"].toString("#000000");
    item.shadowOpacity = styleObj["shadowOpacity"].toDouble(0.5);

    item.bgType = styleObj["bgType"].toInt(0);
    item.bgColor = styleObj["bgColor"].toString("#000000");
    item.bgOpacity = styleObj["bgOpacity"].toDouble(0.6);
    item.bgRoundness = styleObj["bgRoundness"].toInt(4);
    if (styleObj.contains("bgPaddingLeft")) {
      item.bgPaddingLeft = styleObj["bgPaddingLeft"].toInt(15);
      item.bgPaddingRight = styleObj["bgPaddingRight"].toInt(15);
    } else {
      int padX = styleObj["bgPaddingX"].toInt(15);
      item.bgPaddingLeft = padX;
      item.bgPaddingRight = padX;
    }
    if (styleObj.contains("bgPaddingTop")) {
      item.bgPaddingTop = styleObj["bgPaddingTop"].toInt(10);
      item.bgPaddingBottom = styleObj["bgPaddingBottom"].toInt(10);
    } else {
      int padY = styleObj["bgPaddingY"].toInt(10);
      item.bgPaddingTop = padY;
      item.bgPaddingBottom = padY;
    }
    item.bgOffsetX = styleObj["bgOffsetX"].toInt(0);
    item.bgOffsetY = styleObj["bgOffsetY"].toInt(0);

    item.bubbleEnabled = styleObj["bubbleEnabled"].toBool(false);
    item.bubbleImagePath = styleObj["bubbleImagePath"].toString();
    item.bubblePaddingLeft = styleObj["bubblePaddingLeft"].toInt(15);
    item.bubblePaddingRight = styleObj["bubblePaddingRight"].toInt(15);
    item.bubblePaddingTop = styleObj["bubblePaddingTop"].toInt(10);
    item.bubblePaddingBottom = styleObj["bubblePaddingBottom"].toInt(10);
    item.bubbleSliceLeft = styleObj["bubbleSliceLeft"].toInt(item.bubblePaddingLeft);
    item.bubbleSliceRight = styleObj["bubbleSliceRight"].toInt(item.bubblePaddingRight);
    item.bubbleSliceTop = styleObj["bubbleSliceTop"].toInt(item.bubblePaddingTop);
    item.bubbleSliceBottom = styleObj["bubbleSliceBottom"].toInt(item.bubblePaddingBottom);

    addPresetCard(name, item, true, i);
  }
}

void SubtitleListPanel::showPresetContextMenu(int idx, const QPoint &pos) {
  QMenu menu(this);
  QAction *deleteAct = menu.addAction(tr("Delete"));
  QAction *chosen = menu.exec(pos);
  if (chosen == deleteAct) {
    int result = AppMessageBox::question(
        this, tr("Delete Preset"),
        tr("Are you sure you want to delete this preset?"),
        AppMessageBox::Yes | AppMessageBox::No);
    if (result == AppMessageBox::Yes) {
      QString key = "Custom Presets";
      QJsonArray array;
      QString existingRaw = ConfigManager::instance().getString(key, "data");
      if (!existingRaw.isEmpty()) {
        array = QJsonDocument::fromJson(existingRaw.toUtf8()).array();
      }
      if (idx >= 0 && idx < array.size()) {
        array.removeAt(idx);
        ConfigManager::instance().setValue(
            key, "data", QJsonDocument(array).toJson(QJsonDocument::Compact));
        ConfigManager::instance().sync();
        loadCustomPresets();
      }
    }
  }
}

void SubtitleListPanel::addPresetCard(const QString &name,
                                      const SubtitleItem &style, bool isCustom,
                                      int customIndex) {
  if (!presetListWidget_)
    return;

  auto *item = new QListWidgetItem(presetListWidget_);
  item->setIcon(createPresetIcon(style, QSize(60, 60)));
  item->setToolTip(name);

  // 序列化样式为 JSON 并存储到 UserRole 中
  QJsonObject styleObj;
  styleObj["fillType"] = style.fillType;
  styleObj["fillColor"] = style.fillColor;
  styleObj["fillColor2"] = style.fillColor2;
  styleObj["fillAngle"] = style.fillAngle;
  styleObj["fillTexturePath"] = style.fillTexturePath;
  styleObj["fillTextureTile"] = style.fillTextureTile;
  styleObj["textOpacity"] = style.textOpacity;

  styleObj["strokeEnabled"] = style.strokeEnabled;
  styleObj["strokeWidth"] = style.strokeWidth;
  styleObj["strokeColor"] = style.strokeColor;
  styleObj["strokeOpacity"] = style.strokeOpacity;

  styleObj["shadowEnabled"] = style.shadowEnabled;
  styleObj["shadowOffsetX"] = style.shadowOffsetX;
  styleObj["shadowOffsetY"] = style.shadowOffsetY;
  styleObj["shadowBlur"] = style.shadowBlur;
  styleObj["shadowColor"] = style.shadowColor;
  styleObj["shadowOpacity"] = style.shadowOpacity;

  styleObj["bgType"] = style.bgType;
  styleObj["bgColor"] = style.bgColor;
  styleObj["bgOpacity"] = style.bgOpacity;
  styleObj["bgRoundness"] = style.bgRoundness;
  styleObj["bgPaddingLeft"] = style.bgPaddingLeft;
  styleObj["bgPaddingRight"] = style.bgPaddingRight;
  styleObj["bgPaddingTop"] = style.bgPaddingTop;
  styleObj["bgPaddingBottom"] = style.bgPaddingBottom;
  styleObj["bgOffsetX"] = style.bgOffsetX;
  styleObj["bgOffsetY"] = style.bgOffsetY;

  styleObj["bubbleEnabled"] = style.bubbleEnabled;
  styleObj["bubbleImagePath"] = style.bubbleImagePath;
  styleObj["bubblePaddingLeft"] = style.bubblePaddingLeft;
  styleObj["bubblePaddingRight"] = style.bubblePaddingRight;
  styleObj["bubblePaddingTop"] = style.bubblePaddingTop;
  styleObj["bubblePaddingBottom"] = style.bubblePaddingBottom;
  styleObj["bubbleSliceLeft"] = style.bubbleSliceLeft;
  styleObj["bubbleSliceRight"] = style.bubbleSliceRight;
  styleObj["bubbleSliceTop"] = style.bubbleSliceTop;
  styleObj["bubbleSliceBottom"] = style.bubbleSliceBottom;

  QJsonDocument doc(styleObj);
  item->setData(Qt::UserRole, doc.toJson(QJsonDocument::Compact));
  item->setData(Qt::UserRole + 1, isCustom);
  item->setData(Qt::UserRole + 2, customIndex);
}

void SubtitleListPanel::populatePresets() {
  if (!presetListWidget_)
    return;

  presetListWidget_->clear();

  int type = presetTypeCombo_ ? presetTypeCombo_->currentIndex() : 0;
  if (type == 0) {
    // 1. Default White (默认白)
    SubtitleItem p1;
    p1.fillType = 0;
    p1.fillColor = "#FFFFFF";
    p1.textOpacity = 1.0;
    p1.strokeEnabled = true;
    p1.strokeWidth = 2;
    p1.strokeColor = "#000000";
    p1.strokeOpacity = 1.0;
    p1.shadowEnabled = false;
    p1.bgType = 0;

    // 2. Classic Yellow (经典黄)
    SubtitleItem p2;
    p2.fillType = 0;
    p2.fillColor = "#FFCC00";
    p2.textOpacity = 1.0;
    p2.strokeEnabled = true;
    p2.strokeWidth = 2;
    p2.strokeColor = "#000000";
    p2.strokeOpacity = 1.0;
    p2.shadowEnabled = false;
    p2.bgType = 0;

    // 3. Soft Shadow (柔和阴影)
    SubtitleItem p3;
    p3.fillType = 0;
    p3.fillColor = "#FFFFFF";
    p3.textOpacity = 1.0;
    p3.strokeEnabled = false;
    p3.shadowEnabled = true;
    p3.shadowColor = "#000000";
    p3.shadowOffsetX = 2;
    p3.shadowOffsetY = 2;
    p3.shadowBlur = 5;
    p3.shadowOpacity = 0.6;
    p3.bgType = 0;

    // 4. Neon Glow (霓虹光)
    SubtitleItem p4;
    p4.fillType = 0;
    p4.fillColor = "#00FFFF";
    p4.textOpacity = 1.0;
    p4.strokeEnabled = true;
    p4.strokeColor = "#FF00FF";
    p4.strokeWidth = 4;
    p4.strokeOpacity = 0.8;
    p4.shadowEnabled = true;
    p4.shadowColor = "#FF00FF";
    p4.shadowOffsetX = 0;
    p4.shadowOffsetY = 0;
    p4.shadowBlur = 10;
    p4.shadowOpacity = 0.9;
    p4.bgType = 0;

    // 5. Translucent Box (半透背景)
    SubtitleItem p5;
    p5.fillType = 0;
    p5.fillColor = "#FFFFFF";
    p5.textOpacity = 1.0;
    p5.strokeEnabled = false;
    p5.shadowEnabled = false;
    p5.bgType = 1;
    p5.bgColor = "#000000";
    p5.bgOpacity = 0.6;
    p5.bgRoundness = 4;
    p5.bgPaddingLeft = 15;
    p5.bgPaddingRight = 15;
    p5.bgPaddingTop = 10;
    p5.bgPaddingBottom = 10;

    // 6. Silver Gradient (银渐变)
    SubtitleItem p6;
    p6.fillType = 1;
    p6.fillColor = "#FFFFFF";
    p6.fillColor2 = "#AAAAAA";
    p6.fillAngle = 90;
    p6.textOpacity = 1.0;
    p6.strokeEnabled = true;
    p6.strokeColor = "#111111";
    p6.strokeWidth = 2;
    p6.strokeOpacity = 1.0;
    p6.shadowEnabled = false;
    p6.bgType = 0;

    addPresetCard(tr("Default White"), p1, false);
    addPresetCard(tr("Classic Yellow"), p2, false);
    addPresetCard(tr("Soft Shadow"), p3, false);
    addPresetCard(tr("Neon Glow"), p4, false);
    addPresetCard(tr("Translucent Box"), p5, false);
    addPresetCard(tr("Silver Gradient"), p6, false);
  } else {
    loadCustomPresets();
  }
}

void SubtitleListPanel::updateFillTypeFields() {
  if (!fillTypeCombo_ || !fillForm_)
    return;

  auto setRowVisible = [](QFormLayout *form, int row, bool visible) {
    auto *item = form->itemAt(row, QFormLayout::FieldRole);
    if (!item)
      return;
    auto *widget = item->widget();
    if (widget) {
      widget->setVisible(visible);
      auto *label = form->labelForField(widget);
      if (label)
        label->setVisible(visible);
    }
  };

  int idx = fillTypeCombo_->currentIndex(); // 0 = Solid, 1 = Gradient

  setRowVisible(fillForm_, 1, true);     // Color
  setRowVisible(fillForm_, 2, idx == 1); // Gradient 2
  setRowVisible(fillForm_, 3, idx == 1); // Angle
  setRowVisible(fillForm_, 4, true);     // Opacity
}

QWidget *SubtitleListPanel::createBubbleStylePanel() {
  auto *container = new QWidget(this);
  container->setObjectName("BubbleStyleContainer");
  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(12);

  auto *listContainer = new QFrame(container);
  listContainer->setObjectName("SubtitleListContainer");
  listContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto *lcLayout = new QVBoxLayout(listContainer);
  lcLayout->setContentsMargins(6, 6, 6, 6);
  lcLayout->setSpacing(0);

  bubbleListWidget_ = new QListWidget(listContainer);
  bubbleListWidget_->setObjectName("BubbleListWidget");
  bubbleListWidget_->setViewMode(QListView::IconMode);
  bubbleListWidget_->setResizeMode(QListView::Adjust);
  bubbleListWidget_->setMovement(QListView::Static);
  bubbleListWidget_->setFlow(QListView::LeftToRight);
  bubbleListWidget_->setWrapping(true);
  bubbleListWidget_->setSpacing(8);
  bubbleListWidget_->setUniformItemSizes(true);
  bubbleListWidget_->setGridSize(QSize(76, 76));
  bubbleListWidget_->setIconSize(QSize(60, 60));
  bubbleListWidget_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  bubbleListWidget_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  bubbleListWidget_->setStyleSheet(
      "QListWidget#BubbleListWidget {"
      "  background-color: transparent;"
      "  border: none;"
      "}"
      "QListWidget#BubbleListWidget::item {"
      "  background-color: #242424;"
      "  border: 1px solid #3c3c3c;"
      "  border-radius: 6px;"
      "}"
      "QListWidget#BubbleListWidget::item:hover {"
      "  background-color: #2e2e2e;"
      "  border-color: #0088cc;"
      "}"
      "QListWidget#BubbleListWidget::item:selected {"
      "  background-color: #2e2e2e;"
      "  border-color: #0088cc;"
      "}");

  lcLayout->addWidget(bubbleListWidget_);
  layout->addWidget(listContainer);

  connect(bubbleListWidget_, &QListWidget::itemClicked, this,
          [this](QListWidgetItem *item) {
            if (!item || !track_)
              return;
            QVariantMap data = item->data(Qt::UserRole).toMap();
            QString imagePath = data["imagePath"].toString();
            int padLeft = data["padLeft"].toInt();
            int padRight = data["padRight"].toInt();
            int padTop = data["padTop"].toInt();
            int padBottom = data["padBottom"].toInt();
            int sliceLeft = data["sliceLeft"].toInt();
            int sliceRight = data["sliceRight"].toInt();
            int sliceTop = data["sliceTop"].toInt();
            int sliceBottom = data["sliceBottom"].toInt();

            if (!currentSelectedId_.isEmpty()) {
              SubtitleItem sItem;
              for (const auto &it : track_->items()) {
                if (it.id == currentSelectedId_) {
                  sItem = it;
                  break;
                }
              }
              sItem.bubbleEnabled = true;
              sItem.bubbleImagePath = imagePath;
              sItem.bubblePaddingLeft = padLeft;
              sItem.bubblePaddingRight = padRight;
              sItem.bubblePaddingTop = padTop;
              sItem.bubblePaddingBottom = padBottom;
              sItem.bubbleSliceLeft = sliceLeft;
              sItem.bubbleSliceRight = sliceRight;
              sItem.bubbleSliceTop = sliceTop;
              sItem.bubbleSliceBottom = sliceBottom;

              track_->updateItem(currentSelectedId_, sItem);
              loadStyleFromItem(sItem);
            } else {
              SubtitleItem sItem = track_->defaultStyleItem();
              sItem.bubbleEnabled = true;
              sItem.bubbleImagePath = imagePath;
              sItem.bubblePaddingLeft = padLeft;
              sItem.bubblePaddingRight = padRight;
              sItem.bubblePaddingTop = padTop;
              sItem.bubblePaddingBottom = padBottom;
              sItem.bubbleSliceLeft = sliceLeft;
              sItem.bubbleSliceRight = sliceRight;
              sItem.bubbleSliceTop = sliceTop;
              sItem.bubbleSliceBottom = sliceBottom;

              track_->setDefaultStyleItem(sItem);
              loadStyleFromItem(sItem);
            }
          });

  return container;
}

void SubtitleListPanel::addBubbleCard(const QString &name,
                                      const QString &imagePath, int padLeft,
                                      int padRight, int padTop, int padBottom,
                                      int sliceLeft, int sliceRight, int sliceTop,
                                      int sliceBottom) {
  if (!bubbleListWidget_)
    return;

  auto *item = new QListWidgetItem(bubbleListWidget_);
  QIcon icon(imagePath);
  item->setIcon(icon);
  item->setToolTip(name);

  QVariantMap data;
  data["imagePath"] = imagePath;
  data["padLeft"] = padLeft;
  data["padRight"] = padRight;
  data["padTop"] = padTop;
  data["padBottom"] = padBottom;
  data["sliceLeft"] = sliceLeft;
  data["sliceRight"] = sliceRight;
  data["sliceTop"] = sliceTop;
  data["sliceBottom"] = sliceBottom;
  item->setData(Qt::UserRole, data);

  bubbleListWidget_->addItem(item);
}

void SubtitleListPanel::populateBubbles() {
  if (!bubbleListWidget_)
    return;

  bubbleListWidget_->clear();

  addBubbleCard(tr("Glassmorphism"), ":/bubbles/bubble_glass.png", 10, 10, 10,
                10, 40, 40, 40, 40);
  addBubbleCard(tr("Dark Box"), ":/bubbles/bubble_dark.png", 10, 10, 10,
                10, 40, 40, 40, 40);
  addBubbleCard(tr("Cyberpunk Neon"), ":/bubbles/bubble_neon.png", 10, 10, 10,
                10, 40, 40, 40, 40);
  addBubbleCard(tr("Cute Yellow"), ":/bubbles/bubble_cute.png", 10, 10, 10,
                10, 40, 40, 40, 40);
  addBubbleCard(tr("Minimal Outline"), ":/bubbles/bubble_outline.png", 10, 10,
                10, 10, 40, 40, 40, 40);
}

#include "SubtitleListPanel.moc"
