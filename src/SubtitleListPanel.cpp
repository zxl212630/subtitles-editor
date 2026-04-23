#include "SubtitleListPanel.h"
#include "SubtitleListModel.h"
#include "SubtitleListDelegate.h"
#include "SubtitleTrack.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListView>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QHeaderView>

SubtitleListPanel::SubtitleListPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void SubtitleListPanel::setTrack(SubtitleTrack* track)
{
    track_ = track;
    model_->setTrack(track);
}

void SubtitleListPanel::setupUi()
{
    setObjectName("SubtitleListPanel");
    setStyleSheet(R"(
        QWidget#SubtitleListPanel {
            background-color: #1e1e1e;
            border-radius: 10px;
        }
    )");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // --- Panel header (tabs) ---
    auto* panelHeader = new QFrame(this);
    panelHeader->setFixedHeight(40);
    panelHeader->setStyleSheet(R"(
        QFrame {
            background-color: #262626;
            border-top-left-radius: 10px;
            border-top-right-radius: 10px;
            border: none;
        }
    )");
    auto* phLayout = new QHBoxLayout(panelHeader);
    phLayout->setContentsMargins(12, 6, 0, 6);
    phLayout->setSpacing(4);
    phLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto addTab = [&](const QString& text, bool active) {
        auto* tab = new QPushButton(text, panelHeader);
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
        )").arg(bg, fg));
        phLayout->addWidget(tab);
    };

    addTab("字幕", true);
    addTab("预设", false);
    addTab("自定义", false);
    addTab("动画", false);
    phLayout->addStretch();
    layout->addWidget(panelHeader);

    // --- Panel content ---
    auto* panelContent = new QFrame(this);
    panelContent->setStyleSheet("background-color: transparent; border: none;");
    panelContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* pcLayout = new QVBoxLayout(panelContent);
    pcLayout->setContentsMargins(12, 12, 12, 12);
    pcLayout->setSpacing(0);

    // Search bar
    auto* searchBar = new QFrame(panelContent);
    searchBar->setFixedHeight(40);
    searchBar->setStyleSheet("background-color: transparent; border: none;");
    auto* sbLayout = new QHBoxLayout(searchBar);
    sbLayout->setContentsMargins(0, 0, 0, 0);
    sbLayout->setAlignment(Qt::AlignVCenter);

    auto* searchIcon = new QLabel(searchBar);
    searchIcon->setText("\u2315"); // ⌕ search icon
    searchIcon->setStyleSheet("color: #6b7280; font-size: 14px; background: transparent;");
    searchIcon->setFixedSize(20, 20);
    searchIcon->setAlignment(Qt::AlignCenter);
    sbLayout->addWidget(searchIcon);

    searchEdit_ = new QLineEdit(searchBar);
    searchEdit_->setPlaceholderText("请输入查找内容");
    searchEdit_->setFixedHeight(28);
    searchEdit_->setStyleSheet(R"(
        QLineEdit {
            background-color: #141414;
            color: #d1d5db;
            border: none;
            border-radius: 5px;
            padding-left: 8px;
            font-family: Inter, sans-serif;
            font-size: 12px;
        }
    )");
    sbLayout->addWidget(searchEdit_);
    pcLayout->addWidget(searchBar);

    connect(searchEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        model_->setFilterText(text);
    });

    // List container
    auto* listContainer = new QFrame(panelContent);
    listContainer->setStyleSheet(R"(
        QFrame {
            background-color: #141414;
            border-radius: 5px;
        }
    )");
    listContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* lcLayout = new QVBoxLayout(listContainer);
    lcLayout->setContentsMargins(0, 0, 0, 0);
    lcLayout->setSpacing(0);

    // Table header
    auto* tableHeader = new QFrame(listContainer);
    tableHeader->setFixedHeight(32);
    tableHeader->setStyleSheet("background-color: transparent; border: none;");
    auto* thLayout = new QHBoxLayout(tableHeader);
    thLayout->setContentsMargins(12, 0, 12, 0);
    thLayout->setSpacing(12);
    thLayout->setAlignment(Qt::AlignVCenter);

    auto* headerLeft = new QFrame(tableHeader);
    headerLeft->setStyleSheet("background-color: transparent; border: none;");
    auto* hlLayout = new QHBoxLayout(headerLeft);
    hlLayout->setContentsMargins(0, 0, 0, 0);
    hlLayout->setSpacing(80);
    hlLayout->setAlignment(Qt::AlignVCenter);

    auto* headerTime = new QLabel("时间码", headerLeft);
    headerTime->setStyleSheet("color: #9ca3af; font-family: Inter, sans-serif; font-size: 11px; background: transparent;");
    hlLayout->addWidget(headerTime);

    auto* headerText = new QLabel("字幕", headerLeft);
    headerText->setStyleSheet("color: #9ca3af; font-family: Inter, sans-serif; font-size: 11px; background: transparent;");
    hlLayout->addWidget(headerText);

    thLayout->addWidget(headerLeft);
    thLayout->addStretch();

    auto* headerAction = new QLabel("操作", tableHeader);
    headerAction->setStyleSheet("color: #9ca3af; font-family: Inter, sans-serif; font-size: 11px; background: transparent;");
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
    )");
    listView_->setSelectionMode(QAbstractItemView::SingleSelection);
    listView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    model_ = new SubtitleListModel(this);
    listView_->setModel(model_);

    delegate_ = new SubtitleListDelegate(this);
    listView_->setItemDelegate(delegate_);

    connect(listView_, &QListView::clicked, this, &SubtitleListPanel::onItemClicked);

    lcLayout->addWidget(listView_);
    pcLayout->addWidget(listContainer);
    layout->addWidget(panelContent);
}

void SubtitleListPanel::onItemClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;
    QString id = model_->data(index, SubtitleListModel::IdRole).toString();
    if (track_) {
        track_->selectItem(id);
    }
    emit itemSelected(id);
}
