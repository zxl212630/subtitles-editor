#include "SubtitleListPanel.h"
#include "SubtitleListDelegate.h"
#include "SubtitleListModel.h"
#include "SubtitleTrack.h"

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
    mergeBtn_ = createBtn(":/icons/merge.svg", "合并");

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

    // Style adjustments based on enabled state
    auto updateBtnStyle = [](QPushButton *btn) {
      bool enabled = btn->isEnabled();
      QString color = enabled ? "#2dd4bf" : "#4b5563"; // Teal-400 vs Gray-600
      QString bg = enabled ? "#134e4a" : "#1f2937";    // Teal-900 vs Gray-800
      btn->setStyleSheet(QString(R"(
        QPushButton {
          background-color: %1;
          color: %2;
          border: none;
          border-radius: 6px;
          padding: 4px 12px;
          font-family: Inter, sans-serif;
          font-size: 11px;
          font-weight: bold;
        }
        QPushButton:hover:enabled {
          background-color: #115e59;
        }
      )")
                             .arg(bg, color));
    };

    updateBtnStyle(addBtn_);
    updateBtnStyle(mergeBtn_);
  }

signals:
  void addClicked(qint64 start, qint64 end);
  void mergeClicked();

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Green horizontal line
    painter.setPen(QPen(QColor("#2dd4bf"), 1));
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
  setStyleSheet(R"(
        QWidget#SubtitleListPanel {
            background-color: #1e1e1e;
            border-radius: 10px;
        }
    )");

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // --- Panel header (tabs) ---
  auto *panelHeader = new QFrame(this);
  panelHeader->setFixedHeight(40);
  panelHeader->setStyleSheet(R"(
        QFrame {
            background-color: #262626;
            border-top-left-radius: 10px;
            border-top-right-radius: 10px;
            border: none;
        }
    )");
  auto *phLayout = new QHBoxLayout(panelHeader);
  phLayout->setContentsMargins(12, 6, 0, 6);
  phLayout->setSpacing(4);
  phLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  auto addTab = [&](const QString &text, bool active) {
    auto *tab = new QPushButton(text, panelHeader);
    tab->setFixedSize(60, 28);
    QString bg = active ? "#333333" : "#262626";
    QString fg = active ? "#e5e5e5" : "#9ca3af";
    tab->setStyleSheet(QString(R"(
            QPushButton {
                background-color: %1;
                color: %2;
                border: none;
                border-radius: 5px;
                font-family: Inter, sans-serif;
                font-size: 12px;
            }
        )")
                           .arg(bg, fg));
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
  panelContent->setStyleSheet("background-color: transparent; border: none;");
  panelContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto *pcLayout = new QVBoxLayout(panelContent);
  pcLayout->setContentsMargins(12, 12, 12, 12);
  pcLayout->setSpacing(0);

  // Search bar
  auto *searchBar = new QFrame(panelContent);
  searchBar->setFixedHeight(40);
  searchBar->setStyleSheet("background-color: transparent; border: none;");
  auto *sbLayout = new QHBoxLayout(searchBar);
  sbLayout->setContentsMargins(0, 0, 0, 0);
  sbLayout->setAlignment(Qt::AlignVCenter);

  // Search input container (icon + text inside a single frame)
  auto *searchInput = new QFrame(searchBar);
  searchInput->setFixedHeight(28);
  searchInput->setStyleSheet("background-color: #141414; border-radius: 5px;");
  auto *siLayout = new QHBoxLayout(searchInput);
  siLayout->setContentsMargins(4, 0, 8, 0);
  siLayout->setSpacing(6);
  siLayout->setAlignment(Qt::AlignVCenter);

  auto *searchIcon = new QLabel(searchInput);
  searchIcon->setText("\u2315"); // ⌕ search icon
  searchIcon->setStyleSheet(
      "color: #6b7280; font-size: 24px; background: transparent;");
  searchIcon->setFixedSize(24, 24);
  searchIcon->setAlignment(Qt::AlignCenter);
  siLayout->addWidget(searchIcon);

  searchEdit_ = new QLineEdit(searchInput);
  searchEdit_->setPlaceholderText("请输入查找内容");
  searchEdit_->setFixedHeight(28);
  searchEdit_->setStyleSheet(R"(
        QLineEdit {
            background-color: transparent;
            color: #d1d5db;
            border: none;
            font-family: Inter, sans-serif;
            font-size: 12px;
        }
    )");
  siLayout->addWidget(searchEdit_);

  sbLayout->addWidget(searchInput);
  pcLayout->addWidget(searchBar);

  connect(searchEdit_, &QLineEdit::textChanged, this,
          [this](const QString &text) { model_->setFilterText(text); });

  // List container
  auto *listContainer = new QFrame(panelContent);
  listContainer->setStyleSheet(R"(
        QFrame {
            background-color: #141414;
            border-radius: 5px;
        }
    )");
  listContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto *lcLayout = new QVBoxLayout(listContainer);
  lcLayout->setContentsMargins(0, 0, 0, 0);
  lcLayout->setSpacing(0);

  // Table header
  auto *tableHeader = new QFrame(listContainer);
  tableHeader->setFixedHeight(32);
  tableHeader->setStyleSheet("background-color: transparent; border: none;");
  auto *thLayout = new QHBoxLayout(tableHeader);
  thLayout->setContentsMargins(12, 0, 12, 0);
  thLayout->setSpacing(12);
  thLayout->setAlignment(Qt::AlignVCenter);

  auto *headerLeft = new QFrame(tableHeader);
  headerLeft->setStyleSheet("background-color: transparent; border: none;");
  auto *hlLayout = new QHBoxLayout(headerLeft);
  hlLayout->setContentsMargins(0, 0, 0, 0);
  hlLayout->setSpacing(80);
  hlLayout->setAlignment(Qt::AlignVCenter);

  auto *headerTime = new QLabel("时间码", headerLeft);
  headerTime->setStyleSheet("color: #9ca3af; font-family: Inter, sans-serif; "
                            "font-size: 11px; background: transparent;");
  hlLayout->addWidget(headerTime);

  auto *headerText = new QLabel("字幕", headerLeft);
  headerText->setStyleSheet("color: #9ca3af; font-family: Inter, sans-serif; "
                            "font-size: 11px; background: transparent;");
  hlLayout->addWidget(headerText);

  thLayout->addWidget(headerLeft);
  thLayout->addStretch();

  auto *headerAction = new QLabel("操作", tableHeader);
  headerAction->setStyleSheet("color: #9ca3af; font-family: Inter, sans-serif; "
                              "font-size: 11px; background: transparent;");
  thLayout->addWidget(headerAction);

  lcLayout->addWidget(tableHeader);

  // Subtitle list
  listView_ = new QListView(listContainer);
  listView_->setStyleSheet(R"(
        QListView {
            background-color: transparent;
            border: none;
            outline: none;
        }
        QListView::item {
            height: 56px;
            background-color: transparent;
            border: none;
        }
        QListView::item:selected {
            background-color: transparent;
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
  listView_->setSelectionMode(QAbstractItemView::SingleSelection);
  listView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  model_ = new SubtitleListModel(this);
  listView_->setModel(model_);

  delegate_ = new SubtitleListDelegate(this);
  listView_->setItemDelegate(delegate_);

  actionOverlay_ = new SubtitleActionOverlay(listView_->viewport());
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

  listView_->setMouseTracking(true);
  listView_->viewport()->installEventFilter(this);

  connect(listView_, &QListView::clicked, this,
          &SubtitleListPanel::onItemClicked);
  connect(listView_, &QListView::doubleClicked, this,
          &SubtitleListPanel::onItemDoubleClicked);

  lcLayout->addWidget(listView_);
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
            actionOverlay_->move(0, y - actionOverlay_->height() / 2);
            actionOverlay_->resize(listView_->viewport()->width(),
                                   actionOverlay_->height());

            bool canMerge = !id1.isEmpty() && !id2.isEmpty();
            actionOverlay_->updateState(gapStart, gapEnd, canMerge, videoFps_);
            actionOverlay_->setProperty("id1", id1);
            actionOverlay_->setProperty("id2", id2);

            actionOverlay_->show();
            foundZone = true;
            break;
          }
        }

        if (!foundZone) {
          actionOverlay_->hide();
        }
      }

      return false;
    } else if (event->type() == QEvent::Leave) {
      delegate_->setHoveredIndex(QModelIndex(), 0);
      if (actionOverlay_)
        actionOverlay_->hide();
      return false;
    }
  }
  return QWidget::eventFilter(watched, event);
}
#include "SubtitleListPanel.moc"
