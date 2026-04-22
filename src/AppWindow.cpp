#include "AppWindow.h"

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QPushButton>
#include <QSlider>

#include <QWindowKit/QWKWidgets/widgetwindowagent.h>

struct AppWindow::Private
{
    QWK::WidgetWindowAgent* windowAgent = nullptr;
    QFrame* titleBar = nullptr;
    QLabel* titleLabel = nullptr;

    // Main content areas
    QWidget* mainContent = nullptr;
    QWidget* workArea = nullptr;
    QWidget* topPanelArea = nullptr;
    QWidget* videoPreviewPanel = nullptr;
    QWidget* subtitleListPanel = nullptr;
    QWidget* timelinePanel = nullptr;
};

AppWindow::AppWindow(QWidget* parent)
    : QMainWindow(parent)
    , d(std::make_unique<Private>())
{
    setupUi();
}

AppWindow::~AppWindow() = default;

static QPushButton* createIconBtn(QWidget* parent, const QString& text, int w, int h,
                                   const QString& bg = "#333333", const QString& color = "#d1d5db")
{
    auto* btn = new QPushButton(text, parent);
    btn->setFixedSize(w, h);
    btn->setStyleSheet(QString(R"(
        QPushButton {
            background-color: %1;
            color: %2;
            border: none;
            border-radius: 4px;
            font-family: Inter, sans-serif;
            font-size: 12px;
            font-weight: bold;
        }
    )").arg(bg, color));
    return btn;
}

static QFrame* createToolbarBtnGroup(QWidget* parent)
{
    auto* frame = new QFrame(parent);
    frame->setStyleSheet("background-color: transparent; border: none;");
    return frame;
}

void AppWindow::setupUi()
{
    setWindowTitle("字幕编辑");
    resize(1440, 900);
    setMinimumSize(960, 600);
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    move(100, 100);

    // Setup QWindowKit window agent
    d->windowAgent = new QWK::WidgetWindowAgent(this);
    d->windowAgent->setup(this);

    setupTitleBar();
    setupMainContent();

    // Set menu widget and title bar
    setMenuWidget(d->titleBar);
    d->windowAgent->setTitleBar(d->titleBar);
}

void AppWindow::setupTitleBar()
{
    d->titleBar = new QFrame(this);
    d->titleBar->setFixedHeight(36);
    d->titleBar->setObjectName("TitleBar");
    d->titleBar->setStyleSheet(R"(
        QFrame#TitleBar {
            background-color: #262626;
            border: none;
        }
    )");

    auto* layout = new QHBoxLayout(d->titleBar);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(8);
    layout->setAlignment(Qt::AlignVCenter);

    auto* closeBtn = new QPushButton(d->titleBar);
    closeBtn->setFixedSize(12, 12);
    closeBtn->setStyleSheet("QPushButton { background-color: #ff5f57; border-radius: 6px; border: none; }");
    layout->addWidget(closeBtn);

    auto* minBtn = new QPushButton(d->titleBar);
    minBtn->setFixedSize(12, 12);
    minBtn->setStyleSheet("QPushButton { background-color: #febc2e; border-radius: 6px; border: none; }");
    layout->addWidget(minBtn);

    auto* maxBtn = new QPushButton(d->titleBar);
    maxBtn->setFixedSize(12, 12);
    maxBtn->setStyleSheet("QPushButton { background-color: #28c840; border-radius: 6px; border: none; }");
    layout->addWidget(maxBtn);

    auto* leftSpacer = new QWidget(d->titleBar);
    leftSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(leftSpacer);

    d->titleLabel = new QLabel("字幕编辑", d->titleBar);
    d->titleLabel->setAlignment(Qt::AlignCenter);
    d->titleLabel->setStyleSheet(R"(
        QLabel {
            color: #9ca3af;
            font-family: Inter, sans-serif;
            font-size: 12px;
            font-weight: normal;
            background: transparent;
        }
    )");
    layout->addWidget(d->titleLabel);

    auto* rightSpacer = new QWidget(d->titleBar);
    rightSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(rightSpacer);
}

void AppWindow::setupMainContent()
{
    d->mainContent = new QWidget(this);
    d->mainContent->setObjectName("MainContent");
    d->mainContent->setStyleSheet(R"(
        QWidget#MainContent {
            background-color: #0a0a0a;
        }
    )");

    auto* mainLayout = new QVBoxLayout(d->mainContent);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    d->workArea = new QWidget(d->mainContent);
    d->workArea->setObjectName("WorkArea");
    d->workArea->setStyleSheet("background-color: transparent;");
    auto* workLayout = new QVBoxLayout(d->workArea);
    workLayout->setContentsMargins(10, 10, 10, 10);
    workLayout->setSpacing(10);

    d->topPanelArea = new QWidget(d->workArea);
    d->topPanelArea->setObjectName("TopPanelArea");
    d->topPanelArea->setStyleSheet("background-color: transparent;");
    auto* topLayout = new QHBoxLayout(d->topPanelArea);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(10);

    d->videoPreviewPanel = new QWidget(d->topPanelArea);
    d->videoPreviewPanel->setObjectName("VideoPreviewPanel");
    d->videoPreviewPanel->setStyleSheet(R"(
        QWidget#VideoPreviewPanel {
            background-color: #1e1e1e;
            border-radius: 10px;
            border: 1px solid #333333;
        }
    )");
    topLayout->addWidget(d->videoPreviewPanel, 1);

    d->subtitleListPanel = new QWidget(d->topPanelArea);
    d->subtitleListPanel->setObjectName("SubtitleListPanel");
    d->subtitleListPanel->setFixedWidth(558);
    d->subtitleListPanel->setStyleSheet(R"(
        QWidget#SubtitleListPanel {
            background-color: #1e1e1e;
            border-radius: 10px;
        }
    )");
    topLayout->addWidget(d->subtitleListPanel);

    workLayout->addWidget(d->topPanelArea, 1);

    d->timelinePanel = new QWidget(d->workArea);
    d->timelinePanel->setObjectName("TimelinePanel");
    d->timelinePanel->setFixedHeight(220);
    d->timelinePanel->setStyleSheet(R"(
        QWidget#TimelinePanel {
            background-color: #1e1e1e;
            border-radius: 10px;
        }
    )");
    workLayout->addWidget(d->timelinePanel);

    mainLayout->addWidget(d->workArea);
    setCentralWidget(d->mainContent);

    setupVideoPreviewPanel();
    setupSubtitleListPanel();
    setupTimelinePanel();
}

void AppWindow::setupVideoPreviewPanel()
{
    auto* layout = new QVBoxLayout(d->videoPreviewPanel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // --- Toolbar ---
    auto* toolbar = new QFrame(d->videoPreviewPanel);
    toolbar->setFixedHeight(40);
    toolbar->setStyleSheet(R"(
        QFrame {
            background-color: #262626;
            border-top-left-radius: 10px;
            border-top-right-radius: 10px;
            border: none;
        }
    )");
    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(12, 0, 16, 0);
    tbLayout->setSpacing(12);
    tbLayout->setAlignment(Qt::AlignVCenter);

    // Font group
    auto* fontGroup = new QFrame(toolbar);
    fontGroup->setStyleSheet("background-color: transparent; border: none;");
    auto* fgLayout = new QHBoxLayout(fontGroup);
    fgLayout->setContentsMargins(0, 0, 0, 0);
    fgLayout->setSpacing(4);

    auto* fontSelect = new QFrame(fontGroup);
    fontSelect->setFixedSize(140, 28);
    fontSelect->setStyleSheet(R"(
        QFrame {
            background-color: #141414;
            border-radius: 4px;
        }
        QLabel { color: #d1d5db; font-family: Inter; font-size: 12px; background: transparent; }
    )");
    auto* fsLayout = new QHBoxLayout(fontSelect);
    fsLayout->setContentsMargins(8, 0, 6, 0);
    fsLayout->setSpacing(8);
    auto* fontLabel = new QLabel("Arial", fontSelect);
    fontLabel->setStyleSheet("color: #d1d5db; font-family: Inter, sans-serif; font-size: 12px; background: transparent;");
    fsLayout->addWidget(fontLabel);
    fsLayout->addStretch();
    auto* fontArrow = new QLabel("v", fontSelect);
    fontArrow->setStyleSheet("color: #d1d5db; font-family: Inter; font-size: 10px; background: transparent;");
    fsLayout->addWidget(fontArrow);
    fgLayout->addWidget(fontSelect);

    auto* sizeSelect = new QFrame(fontGroup);
    sizeSelect->setFixedSize(50, 28);
    sizeSelect->setStyleSheet(R"(
        QFrame {
            background-color: #141414;
            border-radius: 4px;
        }
        QLabel { color: #d1d5db; font-family: Inter; font-size: 12px; background: transparent; }
    )");
    auto* szLayout = new QHBoxLayout(sizeSelect);
    szLayout->setContentsMargins(8, 0, 6, 0);
    szLayout->setSpacing(4);
    auto* sizeLabel = new QLabel("24", sizeSelect);
    sizeLabel->setStyleSheet("color: #d1d5db; font-family: Inter, sans-serif; font-size: 12px; background: transparent;");
    szLayout->addWidget(sizeLabel);
    szLayout->addStretch();
    auto* sizeArrow = new QLabel("v", sizeSelect);
    sizeArrow->setStyleSheet("color: #d1d5db; font-family: Inter; font-size: 10px; background: transparent;");
    szLayout->addWidget(sizeArrow);
    fgLayout->addWidget(sizeSelect);

    tbLayout->addWidget(fontGroup);

    // Elastic spacer
    auto* tbSpacer = new QWidget(toolbar);
    tbSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tbLayout->addWidget(tbSpacer);

    // Format buttons
    auto* fmtGroup = new QFrame(toolbar);
    fmtGroup->setStyleSheet("background-color: transparent; border: none;");
    auto* fmtLayout = new QHBoxLayout(fmtGroup);
    fmtLayout->setContentsMargins(0, 0, 0, 0);
    fmtLayout->setSpacing(4);
    fmtLayout->setAlignment(Qt::AlignVCenter);

    fmtLayout->addWidget(createIconBtn(fmtGroup, "B", 28, 28));
    fmtLayout->addWidget(createIconBtn(fmtGroup, "I", 28, 28));
    fmtLayout->addWidget(createIconBtn(fmtGroup, "U", 28, 28));

    auto* alignGroup = new QFrame(fmtGroup);
    alignGroup->setStyleSheet("background-color: transparent; border: none;");
    auto* agLayout = new QHBoxLayout(alignGroup);
    agLayout->setContentsMargins(0, 0, 0, 0);
    agLayout->setSpacing(4);
    agLayout->addWidget(createIconBtn(alignGroup, QString(QChar(0x2261)), 28, 28)); // ≡
    agLayout->addWidget(createIconBtn(alignGroup, QString(QChar(0x2261)), 28, 28));
    agLayout->addWidget(createIconBtn(alignGroup, QString(QChar(0x2261)), 28, 28));
    fmtLayout->addWidget(alignGroup);

    tbLayout->addWidget(fmtGroup);
    layout->addWidget(toolbar);

    // --- Video display area ---
    auto* videoArea = new QFrame(d->videoPreviewPanel);
    videoArea->setStyleSheet("background-color: transparent; border: none;");
    videoArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* vaLayout = new QVBoxLayout(videoArea);
    vaLayout->setContentsMargins(40, 0, 40, 0);
    vaLayout->setAlignment(Qt::AlignCenter);

    auto* blackRect = new QFrame(videoArea);
    blackRect->setStyleSheet("background-color: #000000; border: none;");
    blackRect->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    vaLayout->addWidget(blackRect);

    // Handles (8 small blue squares) - placed on videoArea with absolute-like positioning
    // We'll add them after layout is set, using a helper container with no layout
    auto* handleContainer = new QFrame(videoArea);
    handleContainer->setStyleSheet("background-color: transparent; border: none;");
    handleContainer->setGeometry(40, 0, videoArea->width() - 80, videoArea->height());

    auto addHandle = [&](int x, int y) {
        auto* h = new QFrame(handleContainer);
        h->setFixedSize(6, 6);
        h->setStyleSheet("background-color: #38bdf8; border-radius: 1px;");
        h->move(x, y);
        return h;
    };

    // Store handles to reposition later
    // For now use placeholder positions; they'll be updated on resize
    addHandle(0, 0);
    addHandle(100, 0);
    addHandle(200, 0);
    addHandle(0, 100);
    addHandle(200, 100);
    addHandle(0, 200);
    addHandle(100, 200);
    addHandle(200, 200);

    layout->addWidget(videoArea, 1);

    // --- Playback control bar ---
    auto* controlBar = new QFrame(d->videoPreviewPanel);
    controlBar->setFixedHeight(36);
    controlBar->setStyleSheet(R"(
        QFrame {
            background-color: #262626;
            border-bottom-left-radius: 10px;
            border-bottom-right-radius: 10px;
            border: none;
        }
        QLabel { background: transparent; }
    )");
    auto* cbLayout = new QHBoxLayout(controlBar);
    cbLayout->setContentsMargins(8, 0, 12, 0);
    cbLayout->setSpacing(8);
    cbLayout->setAlignment(Qt::AlignVCenter);

    auto addIconLabel = [&](const QString& text, int w, int h) {
        auto* lbl = new QLabel(text, controlBar);
        lbl->setFixedSize(w, h);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("color: #d1d5db; font-family: Inter; font-size: 12px; background: transparent;");
        cbLayout->addWidget(lbl);
    };

    addIconLabel(QString(QChar(0x23EE)), 16, 16); // ⏮
    addIconLabel(QString(QChar(0x23ED)), 16, 16); // ⏭
    addIconLabel(QString(QChar(0x25B6)), 16, 16); // ▶
    addIconLabel(QString(QChar(0x25A0)), 14, 14); // ■

    // Progress bar container
    auto* progressContainer = new QFrame(controlBar);
    progressContainer->setFixedSize(550, 4);
    progressContainer->setStyleSheet("background-color: #333333; border-radius: 2px;");
    auto* progressFill = new QFrame(progressContainer);
    progressFill->setFixedSize(260, 4);
    progressFill->setStyleSheet("background-color: #38bdf8; border-radius: 2px;");
    progressFill->move(0, 0);
    cbLayout->addWidget(progressContainer);

    auto* timeLabel = new QLabel("00:00:06:04/00:00:11:17", controlBar);
    timeLabel->setStyleSheet("color: #d1d5db; font-family: Inter, sans-serif; font-size: 11px; background: transparent;");
    cbLayout->addWidget(timeLabel);

    addIconLabel("Vol", 24, 16);
    addIconLabel("FS", 20, 16);

    layout->addWidget(controlBar);
}

void AppWindow::setupSubtitleListPanel()
{
    auto* layout = new QVBoxLayout(d->subtitleListPanel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // --- Panel header (tabs) ---
    auto* panelHeader = new QFrame(d->subtitleListPanel);
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
    phLayout->setContentsMargins(0, 6, 0, 6);
    phLayout->setSpacing(0);
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
    auto* panelContent = new QFrame(d->subtitleListPanel);
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

    auto* searchInput = new QFrame(searchBar);
    searchInput->setFixedHeight(28);
    searchInput->setStyleSheet(R"(
        QFrame {
            background-color: #141414;
            border-radius: 5px;
        }
        QLabel { background: transparent; }
    )");
    auto* siLayout = new QHBoxLayout(searchInput);
    siLayout->setContentsMargins(8, 0, 8, 0);
    siLayout->setSpacing(6);
    siLayout->setAlignment(Qt::AlignVCenter);
    auto* searchIcon = new QLabel(QString(QChar(0x2315)), searchInput); // ⌕
    searchIcon->setStyleSheet("color: #6b7280; font-size: 14px; background: transparent;");
    siLayout->addWidget(searchIcon);
    auto* searchPlaceholder = new QLabel("请输入查找内容", searchInput);
    searchPlaceholder->setStyleSheet("color: #6b7280; font-family: Inter, sans-serif; font-size: 12px; background: transparent;");
    siLayout->addWidget(searchPlaceholder);
    siLayout->addStretch();
    sbLayout->addWidget(searchInput);
    pcLayout->addWidget(searchBar);

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
    thLayout->setContentsMargins(0, 0, 12, 0);
    thLayout->setSpacing(12);
    thLayout->setAlignment(Qt::AlignVCenter);

    auto* headerLeft = new QFrame(tableHeader);
    headerLeft->setStyleSheet("background-color: transparent; border: none;");
    auto* hlLayout = new QHBoxLayout(headerLeft);
    hlLayout->setContentsMargins(12, 0, 0, 0);
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
    auto* subtitleList = new QFrame(listContainer);
    subtitleList->setStyleSheet("background-color: transparent; border: none;");
    subtitleList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* slLayout = new QVBoxLayout(subtitleList);
    slLayout->setContentsMargins(0, 0, 0, 0);
    slLayout->setSpacing(0);
    slLayout->setAlignment(Qt::AlignTop);

    auto addSubtitleItem = [&](const QString& start, const QString& end, const QString& text, bool selected) {
        auto* item = new QFrame(subtitleList);
        item->setFixedHeight(56);
        QString bg = selected ? "#1f2937" : "transparent";
        QString radius = selected ? "border-radius: 5px;" : "";
        item->setStyleSheet(QString(R"(
            QFrame {
                background-color: %1;
                %2
            }
            QLabel { background: transparent; }
        )").arg(bg, radius));
        auto* itemLayout = new QHBoxLayout(item);
        itemLayout->setContentsMargins(8, 0, 12, 0);
        itemLayout->setSpacing(12);
        itemLayout->setAlignment(Qt::AlignVCenter);

        auto* leftContainer = new QFrame(item);
        leftContainer->setStyleSheet("background-color: transparent; border: none;");
        auto* leftLayout = new QHBoxLayout(leftContainer);
        leftLayout->setContentsMargins(0, 0, 0, 0);
        leftLayout->setSpacing(12);
        leftLayout->setAlignment(Qt::AlignVCenter);

        auto* timeCode = new QFrame(leftContainer);
        timeCode->setFixedWidth(100);
        timeCode->setStyleSheet("background-color: transparent; border: none;");
        auto* tcLayout = new QVBoxLayout(timeCode);
        tcLayout->setContentsMargins(0, 0, 0, 0);
        tcLayout->setSpacing(4);
        tcLayout->setAlignment(Qt::AlignVCenter);
        auto* startLabel = new QLabel(start, timeCode);
        startLabel->setStyleSheet("color: #858e9f; font-family: Inter, sans-serif; font-size: 11px;");
        tcLayout->addWidget(startLabel);
        auto* endLabel = new QLabel(end, timeCode);
        endLabel->setStyleSheet("color: #858e9f; font-family: Inter, sans-serif; font-size: 11px;");
        tcLayout->addWidget(endLabel);
        leftLayout->addWidget(timeCode);

        auto* textLabel = new QLabel(text, leftContainer);
        textLabel->setStyleSheet("color: #d1d5db; font-family: Inter, sans-serif; font-size: 12px;");
        leftLayout->addWidget(textLabel);
        leftLayout->addStretch();

        itemLayout->addWidget(leftContainer);
        itemLayout->addStretch();

        auto* actions = new QFrame(item);
        actions->setStyleSheet("background-color: transparent; border: none;");
        auto* actLayout = new QHBoxLayout(actions);
        actLayout->setContentsMargins(0, 0, 0, 0);
        actLayout->setSpacing(8);
        actLayout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto* splitBtn = createIconBtn(actions, QString(QChar(0x2702)), 20, 20, "transparent", "#6b7280"); // ✂
        auto* delBtn = createIconBtn(actions, QString(QChar(0x2715)), 20, 20, "transparent", "#6b7280"); // ✕
        actLayout->addWidget(splitBtn);
        actLayout->addWidget(delBtn);
        itemLayout->addWidget(actions);

        slLayout->addWidget(item);
    };

    addSubtitleItem("00:00:01:10", "00:00:03:17", "Online tool to convert", false);
    addSubtitleItem("00:00:05:10", "00:00:07:17", "the subtitle file (SRT) to", true);
    addSubtitleItem("00:00:08:10", "00:00:11:17", "PremierePro-supported XML format", false);

    slLayout->addStretch();
    lcLayout->addWidget(subtitleList);
    pcLayout->addWidget(listContainer);
    layout->addWidget(panelContent);
}

void AppWindow::setupTimelinePanel()
{
    auto* layout = new QVBoxLayout(d->timelinePanel);
    layout->setContentsMargins(8, 0, 0, 0);
    layout->setSpacing(0);

    // --- Time ruler ---
    auto* ruler = new QFrame(d->timelinePanel);
    ruler->setFixedHeight(36);
    ruler->setStyleSheet("background-color: transparent; border: none;");

    // Major ticks and labels
    struct TickInfo { int x; QString label; };
    QList<TickInfo> majorTicks = {
        {120, "00:00"}, {220, "00:00:01:00"}, {320, "00:00:02:00"},
        {420, "00:00:03:00"}, {520, "00:00:04:00"}, {620, "00:00:05:00"},
        {720, "00:00:06:00"}, {820, "00:00:07:00"}, {920, "00:00:08:00"},
        {1020, "00:00:09:00"}, {1120, "00:00:10:00"}, {1220, "00:00:11:00"}
    };

    for (const auto& tick : majorTicks) {
        auto* lbl = new QLabel(tick.label, ruler);
        lbl->setStyleSheet("color: #6b7280; font-family: Inter, sans-serif; font-size: 9px; background: transparent;");
        lbl->move(tick.x, 8);
        lbl->adjustSize();

        auto* line = new QFrame(ruler);
        line->setFixedSize(1, 10);
        line->setStyleSheet("background-color: #333333;");
        line->move(tick.x, 24);
    }

    // Minor ticks
    for (int x = 170; x <= 870; x += 100) {
        auto* line = new QFrame(ruler);
        line->setFixedSize(1, 3);
        line->setStyleSheet("background-color: #404040;");
        line->move(x, 28);
    }

    layout->addWidget(ruler);

    // --- Track area ---
    auto* trackArea = new QFrame(d->timelinePanel);
    trackArea->setStyleSheet("background-color: transparent; border: none;");
    trackArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* taLayout = new QVBoxLayout(trackArea);
    taLayout->setContentsMargins(0, 0, 0, 0);
    taLayout->setSpacing(0);

    // Subtitle track
    auto* subtitleTrack = new QFrame(trackArea);
    subtitleTrack->setFixedHeight(48);
    subtitleTrack->setStyleSheet("background-color: #2a2a2a; border: none; border-bottom: 1px solid #333333;");
    auto* stLayout = new QHBoxLayout(subtitleTrack);
    stLayout->setContentsMargins(0, 0, 0, 0);
    stLayout->setSpacing(0);

    auto* subTrackHead = new QFrame(subtitleTrack);
    subTrackHead->setFixedWidth(120);
    subTrackHead->setStyleSheet("background-color: transparent; border: none;");
    auto* sthLayout = new QHBoxLayout(subTrackHead);
    sthLayout->setContentsMargins(12, 0, 12, 0);
    sthLayout->setSpacing(4);
    sthLayout->setAlignment(Qt::AlignVCenter);
    auto* subIcon = new QLabel("T", subTrackHead); // placeholder for type icon
    subIcon->setStyleSheet("color: #d1d5db; font-size: 14px; font-weight: bold; background: transparent;");
    sthLayout->addWidget(subIcon);
    auto* subName = new QLabel("字幕1", subTrackHead);
    subName->setStyleSheet("color: #9ca3af; font-family: Inter, sans-serif; font-size: 12px; background: transparent;");
    sthLayout->addWidget(subName);
    stLayout->addWidget(subTrackHead);

    auto* subTrackContent = new QFrame(subtitleTrack);
    subTrackContent->setStyleSheet("background-color: transparent; border: none;");
    subTrackContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // Subtitle bars placed with absolute positioning
    auto* subBar1 = new QFrame(subTrackContent);
    subBar1->setFixedSize(280, 46);
    subBar1->move(0, 1);
    subBar1->setStyleSheet("background-color: #38bdf8; border-radius: 5px;");
    auto* subBar1Text = new QLabel("Online tool to convert", subBar1);
    subBar1Text->setStyleSheet("color: #e5e5e5; font-family: Inter, sans-serif; font-size: 11px; background: transparent;");
    subBar1Text->move(12, 15);

    auto* subBar2 = new QFrame(subTrackContent);
    subBar2->setFixedSize(180, 46);
    subBar2->move(290, 1);
    subBar2->setStyleSheet("background-color: #38bdf8; border-radius: 5px;");
    auto* subBar2Text = new QLabel("the subtitle ...", subBar2);
    subBar2Text->setStyleSheet("color: #e5e5e5; font-family: Inter, sans-serif; font-size: 11px; background: transparent;");
    subBar2Text->move(12, 15);

    auto* subBar3 = new QFrame(subTrackContent);
    subBar3->setFixedSize(320, 46);
    subBar3->move(480, 1);
    subBar3->setStyleSheet("background-color: #38bdf8; border-radius: 5px;");
    auto* subBar3Text = new QLabel("PremierePro-supported XML format", subBar3);
    subBar3Text->setStyleSheet("color: #e5e5e5; font-family: Inter, sans-serif; font-size: 11px; background: transparent;");
    subBar3Text->move(12, 15);

    stLayout->addWidget(subTrackContent);
    taLayout->addWidget(subtitleTrack);

    // Video track
    auto* videoTrack = new QFrame(trackArea);
    videoTrack->setFixedHeight(96);
    videoTrack->setStyleSheet("background-color: #2a2a2a; border: none;");
    auto* vtLayout = new QHBoxLayout(videoTrack);
    vtLayout->setContentsMargins(0, 0, 0, 0);
    vtLayout->setSpacing(0);

    auto* vidTrackHead = new QFrame(videoTrack);
    vidTrackHead->setFixedWidth(120);
    vidTrackHead->setStyleSheet("background-color: transparent; border: none;");
    auto* vthLayout = new QHBoxLayout(vidTrackHead);
    vthLayout->setContentsMargins(12, 0, 12, 0);
    vthLayout->setSpacing(4);
    vthLayout->setAlignment(Qt::AlignVCenter);
    auto* vidIcon = new QLabel("F", vidTrackHead); // placeholder film icon
    vidIcon->setStyleSheet("color: #d1d5db; font-size: 14px; font-weight: bold; background: transparent;");
    vthLayout->addWidget(vidIcon);
    auto* vidName = new QLabel("视频1", vidTrackHead);
    vidName->setStyleSheet("color: #9ca3af; font-family: Inter, sans-serif; font-size: 12px; background: transparent;");
    vthLayout->addWidget(vidName);
    vtLayout->addWidget(vidTrackHead);

    auto* vidTrackContent = new QFrame(videoTrack);
    vidTrackContent->setStyleSheet("background-color: transparent; border: none;");
    vidTrackContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* vidBar = new QFrame(vidTrackContent);
    vidBar->setFixedSize(400, 94);
    vidBar->move(0, 1);
    vidBar->setStyleSheet("background-color: #0284c7; border-radius: 5px;");
    auto* vidBarText = new QLabel("video.mp4", vidBar);
    vidBarText->setStyleSheet("color: #e5e5e5; font-family: Inter, sans-serif; font-size: 11px; background: transparent;");
    vidBarText->move(12, 38);
    vtLayout->addWidget(vidTrackContent);
    taLayout->addWidget(videoTrack);

    layout->addWidget(trackArea);

    // Playhead (absolute positioned over trackArea)
    auto* playhead = new QFrame(trackArea);
    playhead->setFixedSize(1, 160);
    playhead->move(250, -6);
    playhead->setStyleSheet("background-color: #f59e0b;");

    auto* playheadPointer = new QFrame(trackArea);
    playheadPointer->setFixedSize(12, 12);
    playheadPointer->move(244, -16);
    playheadPointer->setStyleSheet("background-color: #f59e0b;");
}
