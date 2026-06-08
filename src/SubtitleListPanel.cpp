#include "SubtitleListPanel.h"
#include "AppMessageBox.h"
#include "ConfigManager.h"
#include "SpeakerManagerDialog.h"
#include "SubtitleListDelegate.h"
#include "SubtitleListModel.h"
#include "SubtitleTrack.h"
#include "ThemeManager.h"
#include "TranslationManager.h"
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
#include <QPainter>
#include <QStyle>

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
  populatePresets();
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
  tabCustom_->setAttribute(Qt::WA_TransparentForMouseEvents, false);
  tabAnimation_->setAttribute(Qt::WA_TransparentForMouseEvents, true);

  tabPreset_->show();
  tabCustom_->show();
  tabAnimation_->hide();

  connect(tabSubtitle_, &QPushButton::clicked, this, [this]() {
    if (stackedWidget_)
      stackedWidget_->setCurrentIndex(0);
    tabSubtitle_->setProperty("active", true);
    tabPreset_->setProperty("active", false);
    tabCustom_->setProperty("active", false);
    tabSubtitle_->style()->unpolish(tabSubtitle_);
    tabSubtitle_->style()->polish(tabSubtitle_);
    tabPreset_->style()->unpolish(tabPreset_);
    tabPreset_->style()->polish(tabPreset_);
    tabCustom_->style()->unpolish(tabCustom_);
    tabCustom_->style()->polish(tabCustom_);
  });

  connect(tabPreset_, &QPushButton::clicked, this, [this]() {
    if (stackedWidget_)
      stackedWidget_->setCurrentIndex(1);
    tabSubtitle_->setProperty("active", false);
    tabPreset_->setProperty("active", true);
    tabCustom_->setProperty("active", false);
    tabSubtitle_->style()->unpolish(tabSubtitle_);
    tabSubtitle_->style()->polish(tabSubtitle_);
    tabPreset_->style()->unpolish(tabPreset_);
    tabPreset_->style()->polish(tabPreset_);
    tabCustom_->style()->unpolish(tabCustom_);
    tabCustom_->style()->polish(tabCustom_);
  });

  connect(tabCustom_, &QPushButton::clicked, this, [this]() {
    if (stackedWidget_) {
      stackedWidget_->setCurrentIndex(2);
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
    tabCustom_->setProperty("active", true);
    tabSubtitle_->style()->unpolish(tabSubtitle_);
    tabSubtitle_->style()->polish(tabSubtitle_);
    tabPreset_->style()->unpolish(tabPreset_);
    tabPreset_->style()->polish(tabPreset_);
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
  siLayout->setAlignment(Qt::AlignVCenter);

  auto *searchIcon = new QLabel(searchInput);
  searchIcon->setObjectName("SubtitleSearchIcon");
  searchIcon->setFixedSize(14, 14);
  searchIcon->setAlignment(Qt::AlignCenter);
  siLayout->addWidget(searchIcon);

  searchEdit_ = new QLineEdit(searchInput);
  searchEdit_->setObjectName("SubtitleSearchEdit");
  searchEdit_->setPlaceholderText(tr("Search..."));
  searchEdit_->setFixedHeight(28);
  siLayout->addWidget(searchEdit_);

  searchClearBtn_ = new QPushButton(searchInput);
  searchClearBtn_->setObjectName("SubtitleSearchClearButton");
  searchClearBtn_->setFixedSize(18, 18);
  searchClearBtn_->setCursor(Qt::PointingHandCursor);
  searchClearBtn_->setIcon(QIcon(":/icons/close.svg"));
  searchClearBtn_->setIconSize(QSize(10, 10));
  searchClearBtn_->setToolTip(tr("Clear"));
  searchClearBtn_->hide();
  siLayout->addWidget(searchClearBtn_);

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
  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(16);

  // Helper for layout styling (unifies with stylesheet themes)
  auto createGroup = [&](const QString &title, QLayout *insideLayout) {
    auto *group = new QGroupBox(title, container);
    group->setLayout(insideLayout);
    group->setStyleSheet(
        "QGroupBox { font-weight: bold; border: 1px solid #3c3c3c; "
        "border-radius: 6px; margin-top: 10px; padding: 10px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 "
        "3px; }");
    return group;
  };

  // --- TEXT FILL GROUP ---
  auto *fillForm = new QFormLayout();
  fillForm->setContentsMargins(0, 0, 0, 0);
  fillForm->setSpacing(8);

  fillTypeCombo_ = new QComboBox(container);
  fillTypeCombo_->addItems(
      {tr("Solid"), tr("Linear Gradient"), tr("Texture Image")});
  fillForm->addRow(tr("Fill Type"), fillTypeCombo_);

  fillColorBtn_ = new ColorButton(container);
  fillForm->addRow(tr("Color"), fillColorBtn_);

  fillColor2Btn_ = new ColorButton(container);
  fillForm->addRow(tr("Gradient 2"), fillColor2Btn_);

  auto *angleContainer = new QWidget(container);
  auto *angleLayout = new QHBoxLayout(angleContainer);
  angleLayout->setContentsMargins(0, 0, 0, 0);
  angleLayout->setSpacing(8);
  fillAngleSlider_ = new QSlider(Qt::Horizontal, angleContainer);
  fillAngleSlider_->setRange(0, 360);
  fillAngleSpin_ = new QSpinBox(angleContainer);
  fillAngleSpin_->setRange(0, 360);
  angleLayout->addWidget(fillAngleSlider_);
  angleLayout->addWidget(fillAngleSpin_);
  fillForm->addRow(tr("Angle"), angleContainer);

  auto *textureContainer = new QWidget(container);
  auto *textureLayout = new QHBoxLayout(textureContainer);
  textureLayout->setContentsMargins(0, 0, 0, 0);
  textureLayout->setSpacing(4);
  fillTextureEdit_ = new QLineEdit(textureContainer);
  fillTextureEdit_->setReadOnly(true);
  fillTextureBrowse_ = new QPushButton(tr("Browse"), textureContainer);
  fillTextureBrowse_->setFixedWidth(60);
  textureLayout->addWidget(fillTextureEdit_);
  textureLayout->addWidget(fillTextureBrowse_);
  fillForm->addRow(tr("Image"), textureContainer);

  fillTextureTileCheck_ = new QCheckBox(tr("Tile Image"), container);
  fillForm->addRow(QString(), fillTextureTileCheck_);

  textOpacitySlider_ = new QSlider(Qt::Horizontal, container);
  textOpacitySlider_->setRange(0, 100);
  fillForm->addRow(tr("Opacity"), textOpacitySlider_);

  layout->addWidget(createGroup(tr("Fill"), fillForm));

  // --- OUTLINE GROUP ---
  auto *strokeForm = new QFormLayout();
  strokeForm->setContentsMargins(0, 0, 0, 0);
  strokeForm->setSpacing(8);

  strokeEnableCheck_ = new QCheckBox(tr("Enabled"), container);
  strokeForm->addRow(QString(), strokeEnableCheck_);

  strokeColorBtn_ = new ColorButton(container);
  strokeForm->addRow(tr("Color"), strokeColorBtn_);

  strokeWidthSpin_ = new QSpinBox(container);
  strokeWidthSpin_->setRange(1, 20);
  strokeForm->addRow(tr("Thickness"), strokeWidthSpin_);

  strokeOpacitySlider_ = new QSlider(Qt::Horizontal, container);
  strokeOpacitySlider_->setRange(0, 100);
  strokeForm->addRow(tr("Opacity"), strokeOpacitySlider_);

  layout->addWidget(createGroup(tr("Outline"), strokeForm));

  // --- SHADOW GROUP ---
  auto *shadowForm = new QFormLayout();
  shadowForm->setContentsMargins(0, 0, 0, 0);
  shadowForm->setSpacing(8);

  shadowEnableCheck_ = new QCheckBox(tr("Enabled"), container);
  shadowForm->addRow(QString(), shadowEnableCheck_);

  shadowColorBtn_ = new ColorButton(container);
  shadowForm->addRow(tr("Color"), shadowColorBtn_);

  auto *offsetContainer = new QWidget(container);
  auto *offsetLayout = new QHBoxLayout(offsetContainer);
  offsetLayout->setContentsMargins(0, 0, 0, 0);
  offsetLayout->setSpacing(8);
  shadowOffsetXSpin_ = new QSpinBox(offsetContainer);
  shadowOffsetXSpin_->setRange(-30, 30);
  shadowOffsetYSpin_ = new QSpinBox(offsetContainer);
  shadowOffsetYSpin_->setRange(-30, 30);
  offsetLayout->addWidget(new QLabel("X:", offsetContainer));
  offsetLayout->addWidget(shadowOffsetXSpin_);
  offsetLayout->addWidget(new QLabel("Y:", offsetContainer));
  offsetLayout->addWidget(shadowOffsetYSpin_);
  shadowForm->addRow(tr("Offset"), offsetContainer);

  shadowBlurSlider_ = new QSlider(Qt::Horizontal, container);
  shadowBlurSlider_->setRange(0, 20);
  shadowForm->addRow(tr("Blur"), shadowBlurSlider_);

  shadowOpacitySlider_ = new QSlider(Qt::Horizontal, container);
  shadowOpacitySlider_->setRange(0, 100);
  shadowForm->addRow(tr("Opacity"), shadowOpacitySlider_);

  layout->addWidget(createGroup(tr("Shadow"), shadowForm));

  // --- BACKGROUND GROUP ---
  auto *bgForm = new QFormLayout();
  bgForm->setContentsMargins(0, 0, 0, 0);
  bgForm->setSpacing(8);

  bgTypeCombo_ = new QComboBox(container);
  bgTypeCombo_->addItems({tr("None"), tr("Solid Box"), tr("Custom Image")});
  bgForm->addRow(tr("Type"), bgTypeCombo_);

  bgColorBtn_ = new ColorButton(container);
  bgForm->addRow(tr("Color"), bgColorBtn_);

  bgOpacitySlider_ = new QSlider(Qt::Horizontal, container);
  bgOpacitySlider_->setRange(0, 100);
  bgForm->addRow(tr("Opacity"), bgOpacitySlider_);

  bgRoundnessSlider_ = new QSlider(Qt::Horizontal, container);
  bgRoundnessSlider_->setRange(0, 50);
  bgForm->addRow(tr("Roundness"), bgRoundnessSlider_);

  auto *paddingContainer = new QWidget(container);
  auto *paddingLayout = new QHBoxLayout(paddingContainer);
  paddingLayout->setContentsMargins(0, 0, 0, 0);
  paddingLayout->setSpacing(8);
  bgPaddingXSlider_ = new QSlider(Qt::Horizontal, paddingContainer);
  bgPaddingXSlider_->setRange(0, 50);
  bgPaddingYSlider_ = new QSlider(Qt::Horizontal, paddingContainer);
  bgPaddingYSlider_->setRange(0, 50);
  paddingLayout->addWidget(new QLabel("X:", paddingContainer));
  paddingLayout->addWidget(bgPaddingXSlider_);
  paddingLayout->addWidget(new QLabel("Y:", paddingContainer));
  paddingLayout->addWidget(bgPaddingYSlider_);
  bgForm->addRow(tr("Padding"), paddingContainer);

  auto *bgImageContainer = new QWidget(container);
  auto *bgImageLayout = new QHBoxLayout(bgImageContainer);
  bgImageLayout->setContentsMargins(0, 0, 0, 0);
  bgImageLayout->setSpacing(4);
  bgImagePathEdit_ = new QLineEdit(bgImageContainer);
  bgImagePathEdit_->setReadOnly(true);
  bgImageBrowse_ = new QPushButton(tr("Browse"), bgImageContainer);
  bgImageBrowse_->setFixedWidth(60);
  bgImageLayout->addWidget(bgImagePathEdit_);
  bgImageLayout->addWidget(bgImageBrowse_);
  bgForm->addRow(tr("Image"), bgImageContainer);

  bgImage9PatchCheck_ = new QCheckBox(tr("Nine-Patch"), container);
  bgForm->addRow(QString(), bgImage9PatchCheck_);

  layout->addWidget(createGroup(tr("Background"), bgForm));

  // Connect Slider and Spinbox for Angle
  connect(fillAngleSlider_, &QSlider::valueChanged, fillAngleSpin_,
          &QSpinBox::setValue);
  connect(fillAngleSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
          fillAngleSlider_, &QSlider::setValue);

  // File Browsers
  connect(fillTextureBrowse_, &QPushButton::clicked, this, [this]() {
    QString path = QFileDialog::getOpenFileName(
        this, tr("Select Texture Image"), QString(),
        tr("Images (*.png *.jpg *.jpeg)"));
    if (!path.isEmpty()) {
      fillTextureEdit_->setText(path);
      applyCustomStyleToActiveItem();
    }
  });

  connect(bgImageBrowse_, &QPushButton::clicked, this, [this]() {
    QString path = QFileDialog::getOpenFileName(
        this, tr("Select Background Image"), QString(),
        tr("Images (*.png *.jpg *.jpeg)"));
    if (!path.isEmpty()) {
      bgImagePathEdit_->setText(path);
      applyCustomStyleToActiveItem();
    }
  });

  // Setup live update slots
  auto triggerUpdate = [this]() { applyCustomStyleToActiveItem(); };

  // Connect interactive controls
  connect(fillTypeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, triggerUpdate);
  connect(fillColorBtn_, &ColorButton::colorChanged, this, triggerUpdate);
  connect(fillColor2Btn_, &ColorButton::colorChanged, this, triggerUpdate);
  connect(fillAngleSlider_, &QSlider::valueChanged, this, triggerUpdate);
  connect(fillTextureTileCheck_, &QCheckBox::stateChanged, this, triggerUpdate);
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
  connect(shadowOpacitySlider_, &QSlider::valueChanged, this, triggerUpdate);

  connect(bgTypeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, triggerUpdate);
  connect(bgColorBtn_, &ColorButton::colorChanged, this, triggerUpdate);
  connect(bgOpacitySlider_, &QSlider::valueChanged, this, triggerUpdate);
  connect(bgRoundnessSlider_, &QSlider::valueChanged, this, triggerUpdate);
  connect(bgPaddingXSlider_, &QSlider::valueChanged, this, triggerUpdate);
  connect(bgPaddingYSlider_, &QSlider::valueChanged, this, triggerUpdate);
  connect(bgImage9PatchCheck_, &QCheckBox::stateChanged, this, triggerUpdate);

  // Undo support on slider release
  auto recordUndoState = [this]() {
    if (track_ && !currentSelectedId_.isEmpty()) {
      SubtitleItem item;
      for (const auto &it : track_->items()) {
        if (it.id == currentSelectedId_) {
          item = it;
          break;
        }
      }

      item.fillType = fillTypeCombo_->currentIndex();
      item.fillColor = fillColorBtn_->color().name();
      item.fillColor2 = fillColor2Btn_->color().name();
      item.fillAngle = fillAngleSlider_->value();
      item.fillTexturePath = fillTextureEdit_->text();
      item.fillTextureTile = fillTextureTileCheck_->isChecked();
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

      item.bgType = bgTypeCombo_->currentIndex();
      item.bgColor = bgColorBtn_->color().name();
      item.bgOpacity = bgOpacitySlider_->value() / 100.0;
      item.bgRoundness = bgRoundnessSlider_->value();
      item.bgPaddingX = bgPaddingXSlider_->value();
      item.bgPaddingY = bgPaddingYSlider_->value();
      item.bgImagePath = bgImagePathEdit_->text();
      item.bgImage9Patch = bgImage9PatchCheck_->isChecked();

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
  connect(bgPaddingXSlider_, &QSlider::sliderReleased, this, recordUndoState);
  connect(bgPaddingYSlider_, &QSlider::sliderReleased, this, recordUndoState);

  scrollArea->setWidget(container);
  mainLayout->addWidget(scrollArea);

  // 保存当前样式为预设的按钮
  savePresetBtn_ = new QPushButton(tr("+ Save Current Style"), mainContainer);
  savePresetBtn_->setObjectName("SavePresetBtn");
  savePresetBtn_->setStyleSheet(
      "QPushButton { background-color: #2c2c2c; border: 1px solid #444; "
      "border-radius: 4px; padding: 8px 12px; min-height: 28px; color: #eee; "
      "margin: 12px; }"
      "QPushButton:hover { background-color: #3c3c3c; border-color: #555; }");
  mainLayout->addWidget(savePresetBtn_);

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
    styleObj["bgPaddingX"] = item.bgPaddingX;
    styleObj["bgPaddingY"] = item.bgPaddingY;
    styleObj["bgImagePath"] = item.bgImagePath;
    styleObj["bgImage9Patch"] = item.bgImage9Patch;

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

  // 3. 背景底框
  if (style.bgType == 1) {
    double rx = qMin(15.0, style.bgRoundness * 0.5);
    bgRect = QString("  <rect x=\"8\" y=\"16\" width=\"64\" height=\"48\" "
                     "rx=\"%1\" ry=\"%1\" fill=\"%2\" fill-opacity=\"%3\" />\n")
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
  presetTypeCombo_->setStyleSheet(
      "QComboBox { background-color: #2c2c2c; border: 1px solid #444; "
      "border-radius: 4px; padding: 4px 8px; color: #eee; }"
      "QComboBox::drop-down { border: none; }"
      "QComboBox QAbstractItemView { background-color: #2c2c2c; "
      "selection-background-color: #0088cc; }");
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
            style.bgPaddingX = styleObj["bgPaddingX"].toInt(15);
            style.bgPaddingY = styleObj["bgPaddingY"].toInt(10);
            style.bgImagePath = styleObj["bgImagePath"].toString();
            style.bgImage9Patch = styleObj["bgImage9Patch"].toBool(true);

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
              item.bgPaddingX = style.bgPaddingX;
              item.bgPaddingY = style.bgPaddingY;
              item.bgImagePath = style.bgImagePath;
              item.bgImage9Patch = style.bgImage9Patch;

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
              item.bgPaddingX = style.bgPaddingX;
              item.bgPaddingY = style.bgPaddingY;
              item.bgImagePath = style.bgImagePath;
              item.bgImage9Patch = style.bgImage9Patch;

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

  if (fillTypeCombo_)
    fillTypeCombo_->setCurrentIndex(item.fillType);
  if (fillColorBtn_)
    fillColorBtn_->setColor(QColor(item.fillColor));
  if (fillColor2Btn_)
    fillColor2Btn_->setColor(QColor(item.fillColor2));
  if (fillAngleSlider_)
    fillAngleSlider_->setValue(item.fillAngle);
  if (fillAngleSpin_)
    fillAngleSpin_->setValue(item.fillAngle);
  if (fillTextureEdit_)
    fillTextureEdit_->setText(item.fillTexturePath);
  if (fillTextureTileCheck_)
    fillTextureTileCheck_->setChecked(item.fillTextureTile);
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

  if (bgTypeCombo_)
    bgTypeCombo_->setCurrentIndex(item.bgType);
  if (bgColorBtn_)
    bgColorBtn_->setColor(QColor(item.bgColor));
  if (bgOpacitySlider_)
    bgOpacitySlider_->setValue(qRound(item.bgOpacity * 100.0));
  if (bgRoundnessSlider_)
    bgRoundnessSlider_->setValue(item.bgRoundness);
  if (bgPaddingXSlider_)
    bgPaddingXSlider_->setValue(item.bgPaddingX);
  if (bgPaddingYSlider_)
    bgPaddingYSlider_->setValue(item.bgPaddingY);
  if (bgImagePathEdit_)
    bgImagePathEdit_->setText(item.bgImagePath);
  if (bgImage9PatchCheck_)
    bgImage9PatchCheck_->setChecked(item.bgImage9Patch);

  isUpdatingControls_ = false;
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

  item.fillType = fillTypeCombo_->currentIndex();
  item.fillColor = fillColorBtn_->color().name();
  item.fillColor2 = fillColor2Btn_->color().name();
  item.fillAngle = fillAngleSlider_->value();
  item.fillTexturePath = fillTextureEdit_->text();
  item.fillTextureTile = fillTextureTileCheck_->isChecked();
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

  item.bgType = bgTypeCombo_->currentIndex();
  item.bgColor = bgColorBtn_->color().name();
  item.bgOpacity = bgOpacitySlider_->value() / 100.0;
  item.bgRoundness = bgRoundnessSlider_->value();
  item.bgPaddingX = bgPaddingXSlider_->value();
  item.bgPaddingY = bgPaddingYSlider_->value();
  item.bgImagePath = bgImagePathEdit_->text();
  item.bgImage9Patch = bgImage9PatchCheck_->isChecked();

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
    item.bgPaddingX = styleObj["bgPaddingX"].toInt(15);
    item.bgPaddingY = styleObj["bgPaddingY"].toInt(10);
    item.bgImagePath = styleObj["bgImagePath"].toString();
    item.bgImage9Patch = styleObj["bgImage9Patch"].toBool(true);

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
  styleObj["bgPaddingX"] = style.bgPaddingX;
  styleObj["bgPaddingY"] = style.bgPaddingY;
  styleObj["bgImagePath"] = style.bgImagePath;
  styleObj["bgImage9Patch"] = style.bgImage9Patch;

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
    p5.bgPaddingX = 15;
    p5.bgPaddingY = 10;

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

#include "SubtitleListPanel.moc"
