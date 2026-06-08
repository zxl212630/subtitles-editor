#include "SubtitleListPanel.h"
#include "ConfigManager.h"
#include "SpeakerManagerDialog.h"
#include "SubtitleListDelegate.h"
#include "SubtitleListModel.h"
#include "SubtitleTrack.h"
#include "ThemeManager.h"
#include "TranslationManager.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>

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

  QWidget *presetPanel = createPresetStylePanel();
  stackedWidget_->addWidget(presetPanel);

  QWidget *customPanel = createCustomStylePanel();
  stackedWidget_->addWidget(customPanel);

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
  auto *scrollArea = new QScrollArea(this);
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
  return scrollArea;
}

QWidget *SubtitleListPanel::createPresetStylePanel() {
  auto *scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  auto *container = new QWidget(scrollArea);
  container->setObjectName("PresetStyleContainer");
  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(12);

  auto *gridLayout = new QGridLayout();
  gridLayout->setSpacing(12);

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

  addPresetCard(tr("Default White"), p1, gridLayout, 0, 0);
  addPresetCard(tr("Classic Yellow"), p2, gridLayout, 0, 1);
  addPresetCard(tr("Soft Shadow"), p3, gridLayout, 1, 0);
  addPresetCard(tr("Neon Glow"), p4, gridLayout, 1, 1);
  addPresetCard(tr("Translucent Box"), p5, gridLayout, 2, 0);
  addPresetCard(tr("Silver Gradient"), p6, gridLayout, 2, 1);

  layout->addLayout(gridLayout);

  auto *separator = new QFrame(container);
  separator->setFrameShape(QFrame::HLine);
  separator->setStyleSheet(
      "background-color: #333; margin-top: 10px; margin-bottom: 10px;");
  layout->addWidget(separator);

  auto *customTitle = new QLabel(tr("Custom Presets"), container);
  customTitle->setStyleSheet(
      "font-weight: bold; font-size: 13px; color: #aaa;");
  layout->addWidget(customTitle);

  presetGridContainer_ = new QFrame(container);
  auto *pgLayout = new QGridLayout(presetGridContainer_);
  pgLayout->setContentsMargins(0, 0, 0, 0);
  pgLayout->setSpacing(12);
  layout->addWidget(presetGridContainer_);

  auto *saveBtn = new QPushButton(tr("+ Save Current Style"), container);
  saveBtn->setObjectName("SavePresetBtn");
  saveBtn->setStyleSheet(
      "QPushButton { background-color: #2c2c2c; border: 1px solid #444; "
      "border-radius: 4px; padding: 6px 12px; min-height: 28px; }"
      "QPushButton:hover { background-color: #3c3c3c; border-color: #555; }");
  layout->addWidget(saveBtn);

  connect(saveBtn, &QPushButton::clicked, this, [this]() {
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

    loadCustomPresets();
  });

  layout->addStretch();

  loadCustomPresets();

  scrollArea->setWidget(container);
  return scrollArea;
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
  if (!presetGridContainer_)
    return;

  QLayout *l = presetGridContainer_->layout();
  if (l) {
    QLayoutItem *child;
    while ((child = l->takeAt(0)) != nullptr) {
      if (child->widget()) {
        child->widget()->deleteLater();
      }
      delete child;
    }
  }

  auto *layout = qobject_cast<QGridLayout *>(l);
  if (!layout)
    return;

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

    int row = i / 2;
    int col = i % 2;
    addPresetCard(name, item, layout, row, col);
  }
}

void SubtitleListPanel::addPresetCard(const QString &name,
                                      const SubtitleItem &style,
                                      QGridLayout *layout, int row, int col) {
  auto *btn = new QPushButton(
      name, presetGridContainer_ ? (QWidget *)presetGridContainer_ : this);
  btn->setObjectName("PresetCardBtn");

  QString colorStyle;
  if (style.fillType == 0 || style.fillType == 1) {
    colorStyle = QString("color: %1;").arg(style.fillColor);
  }

  QString bgStyle;
  if (style.bgType == 1) {
    QColor bg(style.bgColor);
    bg.setAlphaF(style.bgOpacity);
    bgStyle = QString("background-color: rgba(%1, %2, %3, %4);")
                  .arg(bg.red())
                  .arg(bg.green())
                  .arg(bg.blue())
                  .arg(bg.alpha());
  } else {
    bgStyle = "background-color: #242424;";
  }

  btn->setStyleSheet(
      QString("QPushButton { %1 %2 border: 1px solid #444; border-radius: 6px; "
              "min-height: 48px; font-weight: bold; }"
              "QPushButton:hover { border-color: #0088cc; }")
          .arg(colorStyle)
          .arg(bgStyle));

  connect(btn, &QPushButton::clicked, this, [this, style]() {
    if (!track_)
      return;

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

  layout->addWidget(btn, row, col);
}

#include "SubtitleListPanel.moc"
