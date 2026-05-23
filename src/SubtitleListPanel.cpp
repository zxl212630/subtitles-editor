#include "SubtitleListPanel.h"
#include "SubtitleListDelegate.h"
#include "SubtitleListModel.h"
#include "SubtitleTrack.h"
#include "ThemeManager.h"
#include "TranslationManager.h"
#include "SpeakerManagerDialog.h"
#include "ConfigManager.h"
#include <QMenu>

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

  tabSubtitle_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  tabPreset_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  tabCustom_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  tabAnimation_->setAttribute(Qt::WA_TransparentForMouseEvents, true);

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
  layout->addWidget(panelContent);

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
  if (!model_ || !listView_) return;
  QModelIndex index = model_->indexForId(id);
  if (index.isValid()) {
    if (listView_->currentIndex() != index) {
      listView_->setCurrentIndex(index);
    }
    listView_->scrollTo(index, QAbstractItemView::EnsureVisible);
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
        QRect timeRect(option.rect.left() + 12, option.rect.top() + 6, timecodeWidth, option.rect.height() - 12);
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
  if (!track_) return;

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
  if (!chosen) return;

  if (chosen == manageAction) {
    SpeakerManagerDialog dlg(track_, this);
    dlg.exec();
    return;
  }
  if (chosen == newAction) {
    int nextId = 0;
    for (const auto &spk : track_->allSpeakers()) {
      if (spk.id >= nextId) nextId = spk.id + 1;
    }
    track_->autoRegisterSpeaker(nextId);
    model_->setData(index, nextId, SubtitleListModel::SpeakerIdRole);
    return;
  }

  int speakerId = chosen->data().toInt();
  model_->setData(index, speakerId, SubtitleListModel::SpeakerIdRole);
}

#include "SubtitleListPanel.moc"
