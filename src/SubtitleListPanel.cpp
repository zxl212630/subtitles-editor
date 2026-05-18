#include "SubtitleListPanel.h"
#include "SubtitleListDelegate.h"
#include "SubtitleListModel.h"
#include "SubtitleTrack.h"
#include "ThemeManager.h"

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

    addBtn_ = createBtn(":/icons/add.svg", "添加");
    addBtn_->setObjectName("SubtitleActionBtn");
    mergeBtn_ = createBtn(":/icons/merge.svg", "合并");
    mergeBtn_->setObjectName("SubtitleActionBtn");

    layout->addStretch();
    layout->addWidget(addBtn_);
    layout->addWidget(mergeBtn_);
    layout->addStretch();

    connect(addBtn_, &QPushButton::clicked, this, [this]() {
      emit addClicked(gapStart_, gapEnd_);
    });
    connect(mergeBtn_, &QPushButton::clicked, this, [this]() {
      emit mergeClicked();
    });
    }


  void updateState(qint64 gapStart, qint64 gapEnd, bool canMerge, double fps) {
    gapStart_ = gapStart;
    gapEnd_ = gapEnd;

    double minGap = 1000.0 / fps;
    bool canAdd = (gapEnd - gapStart) >= minGap;

    addBtn_->setEnabled(canAdd);
    mergeBtn_->setEnabled(canMerge);
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
}

void SubtitleListPanel::setTrack(SubtitleTrack *track) {
  track_ = track;
  model_->setTrack(track);
}

void SubtitleListPanel::setVideoFps(double fps) {
  if (fps > 0)
    videoFps_ = fps;
}

void SubtitleListPanel::setTotalDuration(qint64 ms) { totalDurationMs_ = ms; }

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

  auto addTab = [&](const QString &text, bool active) {
    auto *tab = new QPushButton(text, panelHeader);
    tab->setObjectName("SubtitleTabBtn");
    tab->setProperty("active", active);
    tab->setFixedSize(60, 28);
phLayout->addWidget(tab);
  };

  addTab("字幕", true);
  addTab("预设", false);
  addTab("自定义", false);
  addTab("动画", false);
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
  siLayout->setSpacing(8);
  siLayout->setAlignment(Qt::AlignVCenter);

  auto *searchIcon = new QLabel(searchInput);
  searchIcon->setObjectName("SubtitleSearchIcon");
  searchIcon->setText("\u2315"); // ⌕ search icon
searchIcon->setFixedSize(22, 22);
  searchIcon->setAlignment(Qt::AlignCenter);
  siLayout->addWidget(searchIcon);

  searchEdit_ = new QLineEdit(searchInput);
  searchEdit_->setObjectName("SubtitleSearchEdit");
  searchEdit_->setPlaceholderText("请输入查找内容");
  searchEdit_->setFixedHeight(28);
siLayout->addWidget(searchEdit_);

  sbLayout->addWidget(searchInput);
  pcLayout->addWidget(searchBar);

  connect(searchEdit_, &QLineEdit::textChanged, this,
          [this](const QString &text) { model_->setFilterText(text); });

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
  hlLayout->setSpacing(80);
  hlLayout->setAlignment(Qt::AlignVCenter);

  auto *headerTime = new QLabel("时间码", headerLeft);
  headerTime->setObjectName("SubtitleHeaderLabel");
  hlLayout->addWidget(headerTime);

  auto *headerText = new QLabel("字幕", headerLeft);
  headerText->setObjectName("SubtitleHeaderLabel");
  hlLayout->addWidget(headerText);

  thLayout->addWidget(headerLeft);
  thLayout->addStretch();

  auto *headerAction = new QLabel("操作", tableHeader);
  headerAction->setObjectName("SubtitleHeaderLabel");
  thLayout->addWidget(headerAction);

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

  connect(listView_, &QListView::clicked, this,
          &SubtitleListPanel::onItemClicked);
  connect(listView_, &QListView::doubleClicked, this,
          &SubtitleListPanel::onItemDoubleClicked);

  lcLayout->addWidget(listView_);
  lcLayout->addSpacing(12); // Extra space at bottom
  pcLayout->addWidget(listContainer);
  layout->addWidget(panelContent);
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

bool SubtitleListPanel::eventFilter(QObject *watched, QEvent *event) {
  if (watched == listView_->viewport()) {
    if (event->type() == QEvent::MouseButtonPress) {
      auto *me = static_cast<QMouseEvent *>(event);
      QModelIndex index = listView_->indexAt(me->pos());
      if (index.isValid()) {
        QStyleOptionViewItem option;
        option.initFrom(listView_);
        option.rect = listView_->visualRect(index);

        // Check Split Button
        if (delegate_->splitButtonRect(option).contains(me->pos())) {
          QString id = model_->data(index, SubtitleListModel::IdRole).toString();
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
          QString id = model_->data(index, SubtitleListModel::IdRole).toString();
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
            QPoint posInContainer = listView_->viewport()->mapTo(listView_->parentWidget(), QPoint(0, y));
            
            actionOverlay_->move(0, posInContainer.y() - actionOverlay_->height() / 2);
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
      // Logic for hiding: check if mouse actually left the logical interaction
      // zone
      if (actionOverlay_) {
        QPoint globalPos = QCursor::pos();
        QPoint localPos = actionOverlay_->mapFromGlobal(globalPos);
        if (!actionOverlay_->rect().contains(localPos)) {
          actionOverlay_->hide();
        }
      }
      return false;
    }
  }
  return QWidget::eventFilter(watched, event);
}
#include "SubtitleListPanel.moc"
