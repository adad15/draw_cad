#include "AutoCadGui.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
#include <QStringList>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextOption>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QString workspaceRoot() {
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        if (QFileInfo::exists(dir.filePath("draw_cad.sln")) ||
            QFileInfo::exists(dir.filePath("Drawing1.dxf"))) {
            return QDir::cleanPath(dir.absolutePath());
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../..");
}

QString defaultBackendExe() {
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QDir root(workspaceRoot());
    const QStringList candidates = {
        appDir.filePath("draw_cad.exe"),
        root.filePath("x64/Debug/draw_cad.exe"),
        root.filePath("draw_cad/x64/Debug/draw_cad.exe"),
        root.filePath("x64/Release/draw_cad.exe"),
        root.filePath("draw_cad/x64/Release/draw_cad.exe")
    };

    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return QDir::toNativeSeparators(candidate);
        }
    }
    return QDir::toNativeSeparators(root.filePath("draw_cad/x64/Debug/draw_cad.exe"));
}

QString defaultPath(const QString& fileName) {
    return QDir::toNativeSeparators(QDir(workspaceRoot()).filePath(fileName));
}

bool sameCleanPath(const QString& left, const QString& right) {
    return QDir::cleanPath(QDir::fromNativeSeparators(left)).compare(
        QDir::cleanPath(QDir::fromNativeSeparators(right)),
        Qt::CaseInsensitive) == 0;
}

int countExcelFiles(const QString& directoryPath) {
    const QDir dir(directoryPath);
    if (!dir.exists()) {
        return 0;
    }

    return dir.entryInfoList(
        QStringList({"*.xlsx", "*.xlsm", "*.xls"}),
        QDir::Files | QDir::NoSymLinks).size();
}

QString nowText() {
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

void polish(QWidget* widget) {
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

} // namespace

AutoCadGui::AutoCadGui(QWidget* parent)
    : QWidget(parent),
      controller_(new AutoCadController(this)) {
    setupUi();
    applyStyles();
    setDefaults();
    refreshUploadStatus();
    refreshBoardLengthSourceStatus();

    connect(controller_, &AutoCadController::progressTextChanged, this, &AutoCadGui::appendLog);
    connect(controller_, &AutoCadController::progressUpdated, this,
        [this](int current, int total, const QString& message) {
            progressBar_->setRange(0, total);
            progressBar_->setValue(current);
            statusLabel_->setText(message);
            if (logDialogProgressBar_) {
                logDialogProgressBar_->setRange(0, total);
                logDialogProgressBar_->setValue(current);
            }
            if (logDialogStatusLabel_) {
                logDialogStatusLabel_->setText(message);
            }
            updateStepState(current, total);
        });
    connect(controller_, &AutoCadController::finished, this,
        [this](bool ok, const QString& summary) {
            setRunning(false);
            progressBar_->setRange(0, 1);
            progressBar_->setValue(ok ? 1 : 0);
            statusLabel_->setText(ok ? "完成" : "失败");
            outputSummaryLabel_->setText(summary);
            updateStepState(ok ? stepLabels_.size() : 0, stepLabels_.size());
            appendLog(summary);
            setLogDialogFinished(ok, summary);
        });
}

void AutoCadGui::setupUi() {
    setWindowTitle("报告转 CAD 工作台");
    setMinimumSize(1240, 740);
    setObjectName("AppShell");

    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* sidebar = new QWidget(this);
    sidebar->setObjectName("Sidebar");
    sidebar->setFixedWidth(244);
    auto* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(18, 18, 18, 18);
    sidebarLayout->setSpacing(18);

    auto* brandRow = new QHBoxLayout();
    brandRow->setSpacing(12);
    auto* brandIcon = new QLabel("CAD", sidebar);
    brandIcon->setObjectName("BrandIcon");
    brandIcon->setAlignment(Qt::AlignCenter);
    brandIcon->setFixedSize(44, 44);
    auto* brandText = new QLabel("ReportCAD", sidebar);
    brandText->setObjectName("BrandText");
    brandRow->addWidget(brandIcon);
    brandRow->addWidget(brandText, 1);
    sidebarLayout->addLayout(brandRow);

    auto* navLabel = new QLabel("工作模块", sidebar);
    navLabel->setObjectName("SidebarLabel");
    sidebarLayout->addWidget(navLabel);

    const auto makeNavButton = [this, sidebar](const QString& text, QStyle::StandardPixmap icon, bool active) {
        auto* button = new QPushButton(text, sidebar);
        button->setObjectName(active ? "ActiveNavButton" : "NavButton");
        button->setIcon(style()->standardIcon(icon));
        button->setFixedHeight(44);
        button->setCursor(Qt::PointingHandCursor);
        return button;
    };
    cadNavButton_ = makeNavButton("CAD 生成", QStyle::SP_FileDialogDetailedView, true);
    tenderNavButton_ = makeNavButton("标书生成", QStyle::SP_FileDialogListView, false);
    fileLibraryNavButton_ = makeNavButton("文件库", QStyle::SP_FileIcon, false);
    aiSettingsNavButton_ = makeNavButton("AI 设置", QStyle::SP_FileDialogInfoView, false);
    sidebarLayout->addWidget(cadNavButton_);
    sidebarLayout->addWidget(tenderNavButton_);
    sidebarLayout->addWidget(fileLibraryNavButton_);
    sidebarLayout->addWidget(aiSettingsNavButton_);
    sidebarLayout->addStretch();

    auto* sidebarStatus = new QWidget(sidebar);
    sidebarStatus->setObjectName("SidebarStatus");
    auto* sidebarStatusLayout = new QVBoxLayout(sidebarStatus);
    sidebarStatusLayout->setContentsMargins(14, 12, 14, 12);
    sidebarStatusLayout->setSpacing(6);
    auto* statusTitle = new QLabel("本地批量生成", sidebarStatus);
    statusTitle->setObjectName("SidebarStatusTitle");
    auto* statusHint = new QLabel("外观病害表作为任务输入，板长表作为基础数据源。", sidebarStatus);
    statusHint->setObjectName("SidebarStatusHint");
    statusHint->setWordWrap(true);
    sidebarStatusLayout->addWidget(statusTitle);
    sidebarStatusLayout->addWidget(statusHint);
    sidebarLayout->addWidget(sidebarStatus);
    rootLayout->addWidget(sidebar);

    auto* workspace = new QWidget(this);
    workspace->setObjectName("Workspace");
    auto* bodyLayout = new QVBoxLayout(workspace);
    bodyLayout->setContentsMargins(24, 22, 24, 22);
    bodyLayout->setSpacing(18);
    rootLayout->addWidget(workspace, 1);

    auto* header = new QWidget(workspace);
    header->setObjectName("HeaderBar");
    header->setFixedHeight(82);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 14, 20, 14);
    headerLayout->setSpacing(16);

    auto* titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(4);
    pageTitleLabel_ = new QLabel("报告转 CAD 工作台", header);
    pageTitleLabel_->setObjectName("PageTitle");
    pageSubtitleLabel_ = new QLabel("上传外观病害总表，选择衬砌长度/板长数据源，自动生成多个标准 CAD 图纸文档。", header);
    pageSubtitleLabel_->setObjectName("PageSubtitle");
    titleLayout->addWidget(pageTitleLabel_);
    titleLayout->addWidget(pageSubtitleLabel_);
    headerLayout->addLayout(titleLayout, 1);

    auto* searchEdit = new QLineEdit(header);
    searchEdit->setObjectName("SearchEdit");
    searchEdit->setPlaceholderText("搜索任务或文件...");
    searchEdit->setFixedSize(260, 40);
    searchEdit->addAction(style()->standardIcon(QStyle::SP_FileDialogContentsView), QLineEdit::LeadingPosition);
    headerLayout->addWidget(searchEdit);

    auto* notificationButton = new QPushButton("!", header);
    notificationButton->setObjectName("NotificationButton");
    notificationButton->setFixedSize(40, 40);
    notificationButton->setCursor(Qt::PointingHandCursor);
    headerLayout->addWidget(notificationButton);

    auto* divider = new QFrame(header);
    divider->setObjectName("TopDivider");
    divider->setFixedSize(1, 28);
    headerLayout->addWidget(divider);

    auto* userAvatar = new QLabel("US", header);
    userAvatar->setObjectName("UserAvatar");
    userAvatar->setAlignment(Qt::AlignCenter);
    userAvatar->setFixedSize(40, 40);
    headerLayout->addWidget(userAvatar);
    bodyLayout->addWidget(header);

    contentStack_ = new QStackedWidget(this);
    contentStack_->setObjectName("ContentStack");
    bodyLayout->addWidget(contentStack_, 1);

    auto* cadPage = new QWidget(contentStack_);
    cadPage->setObjectName("ModulePage");
    auto* cadPageLayout = new QVBoxLayout(cadPage);
    cadPageLayout->setContentsMargins(0, 0, 0, 0);
    cadPageLayout->setSpacing(0);

    auto* cadScroll = new QScrollArea(cadPage);
    cadScroll->setObjectName("ContentScroll");
    cadScroll->setWidgetResizable(true);
    cadScroll->setFrameShape(QFrame::NoFrame);
    cadScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    cadScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* cadDashboard = new QWidget(cadScroll);
    cadDashboard->setObjectName("CadDashboard");
    auto* mainLayout = new QHBoxLayout(cadDashboard);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(18);
    cadScroll->setWidget(cadDashboard);
    cadPageLayout->addWidget(cadScroll);
    contentStack_->addWidget(cadPage);

    exeEdit_ = new QLineEdit(this);
    exeEdit_->hide();

    auto* generatorCard = new QWidget(this);
    generatorCard->setObjectName("GeneratorCard");
    generatorCard->setMinimumWidth(600);
    auto* generatorLayout = new QVBoxLayout(generatorCard);
    generatorLayout->setContentsMargins(0, 0, 0, 0);
    generatorLayout->setSpacing(16);

    auto* heroPanel = new QWidget(generatorCard);
    heroPanel->setObjectName("HeroPanel");
    heroPanel->setFixedHeight(116);
    auto* generatorHeader = new QHBoxLayout(heroPanel);
    generatorHeader->setContentsMargins(22, 18, 22, 18);
    generatorHeader->setSpacing(12);
    auto* iconLabel = new QLabel("CAD", generatorCard);
    iconLabel->setObjectName("FeatureIcon");
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setFixedSize(42, 42);
    auto* headerText = new QVBoxLayout();
    headerText->setSpacing(4);
    auto* cardTitle = new QLabel("批量 CAD 图纸生成", generatorCard);
    cardTitle->setObjectName("CardBigTitle");
    auto* cardSubtitle = new QLabel("上传外观病害总表，选择衬砌长度数据源后批量生成 DXF 文件", generatorCard);
    cardSubtitle->setObjectName("MutedText");
    headerText->addWidget(cardTitle);
    headerText->addWidget(cardSubtitle);
    generatorHeader->addWidget(iconLabel);
    generatorHeader->addLayout(headerText, 1);
    auto* flowChip = new QLabel("Excel -> 数据源匹配 -> DXF", heroPanel);
    flowChip->setObjectName("HeroFlowChip");
    flowChip->setAlignment(Qt::AlignCenter);
    flowChip->setFixedHeight(34);
    flowChip->setMinimumWidth(178);
    generatorHeader->addStretch();
    generatorHeader->addWidget(flowChip);
    generatorLayout->addWidget(heroPanel);

    auto* inputLayout = new QVBoxLayout();
    inputLayout->setContentsMargins(22, 0, 22, 0);
    inputLayout->setSpacing(14);
    auto* stepOneLayout = new QHBoxLayout();
    stepOneLayout->setSpacing(12);
    auto* stepOne = new QLabel("1", generatorCard);
    stepOne->setObjectName("StepNumber");
    stepOne->setAlignment(Qt::AlignCenter);
    stepOne->setFixedSize(30, 30);
    auto* stepOneTitle = new QLabel("本次任务文件", generatorCard);
    stepOneTitle->setObjectName("SectionTitle");
    stepOneLayout->addWidget(stepOne);
    stepOneLayout->addWidget(stepOneTitle);
    stepOneLayout->addStretch();
    inputLayout->addLayout(stepOneLayout);

    diseaseWorkbookEdit_ = new QLineEdit(this);
    diseaseWorkbookEdit_->hide();
    boardLengthSourceEdit_ = new QLineEdit(this);
    boardLengthSourceEdit_->hide();

    auto* uploadBox = new QWidget(generatorCard);
    uploadBox->setObjectName("TaskFilePanel");
    uploadBox->setFixedHeight(158);
    uploadBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* uploadBoxLayout = new QVBoxLayout(uploadBox);
    uploadBoxLayout->setContentsMargins(0, 0, 0, 0);
    uploadBoxLayout->setSpacing(0);

    uploadStack_ = new QStackedWidget(uploadBox);
    uploadStack_->setObjectName("UploadStack");
    uploadBoxLayout->addWidget(uploadStack_);

    auto* uploadPromptPage = new QPushButton(uploadStack_);
    uploadPromptPage->setObjectName("UploadPromptPage");
    uploadPromptPage->setCursor(Qt::PointingHandCursor);
    auto* promptLayout = new QVBoxLayout(uploadPromptPage);
    promptLayout->setContentsMargins(22, 24, 22, 24);
    promptLayout->setSpacing(10);

    auto* cloudIcon = new QLabel("↥", uploadPromptPage);
    cloudIcon->setObjectName("UploadIcon");
    cloudIcon->setAlignment(Qt::AlignCenter);
    cloudIcon->setFixedSize(58, 58);
    auto* uploadHint = new QLabel("拖拽外观病害 Excel 到此处，或者 点击上传", uploadPromptPage);
    uploadHint->setObjectName("UploadTitle");
    uploadHint->setAlignment(Qt::AlignCenter);
    auto* uploadSubHint = new QLabel("支持格式: .xlsx, .xlsm  多数据表外观病害总表", uploadPromptPage);
    uploadSubHint->setObjectName("UploadHint");
    uploadSubHint->setAlignment(Qt::AlignCenter);
    promptLayout->addStretch();
    promptLayout->addWidget(cloudIcon, 0, Qt::AlignHCenter);
    promptLayout->addWidget(uploadHint);
    promptLayout->addWidget(uploadSubHint);
    promptLayout->addStretch();
    connect(uploadPromptPage, &QPushButton::clicked, this, &AutoCadGui::chooseProjectFiles);
    uploadStack_->addWidget(uploadPromptPage);

    auto* selectedFilesPage = new QWidget(uploadStack_);
    selectedFilesPage->setObjectName("SelectedFilesPage");
    auto* selectedPageLayout = new QVBoxLayout(selectedFilesPage);
    selectedPageLayout->setContentsMargins(18, 16, 18, 16);
    selectedPageLayout->setSpacing(12);

    auto* selectedHeaderLayout = new QHBoxLayout();
    selectedHeaderLayout->setSpacing(10);
    auto* selectedTitle = new QLabel("本次任务文件", selectedFilesPage);
    selectedTitle->setObjectName("SelectedFilesTitle");
    auto* selectedHint = new QLabel("外观病害 Excel 是本次生成任务的主输入", selectedFilesPage);
    selectedHint->setObjectName("UploadHint");
    auto* addFileButton = new QPushButton("重新选择", selectedFilesPage);
    addFileButton->setObjectName("SmallActionButton");
    addFileButton->setFixedHeight(32);
    addFileButton->setCursor(Qt::PointingHandCursor);
    selectedHeaderLayout->addWidget(selectedTitle);
    selectedHeaderLayout->addWidget(selectedHint);
    selectedHeaderLayout->addStretch();
    selectedHeaderLayout->addWidget(addFileButton);
    selectedPageLayout->addLayout(selectedHeaderLayout);

    selectedFilesLayout_ = new QVBoxLayout();
    selectedFilesLayout_->setSpacing(10);
    selectedPageLayout->addLayout(selectedFilesLayout_);
    selectedPageLayout->addStretch(1);
    connect(addFileButton, &QPushButton::clicked, this, &AutoCadGui::chooseProjectFiles);
    uploadStack_->addWidget(selectedFilesPage);

    inputLayout->addWidget(uploadBox);
    generatorLayout->addLayout(inputLayout);

    auto* outputLayout = new QVBoxLayout();
    outputLayout->setContentsMargins(22, 0, 22, 0);
    outputLayout->setSpacing(14);
    auto* stepTwoLayout = new QHBoxLayout();
    stepTwoLayout->setSpacing(12);
    auto* stepTwo = new QLabel("2", generatorCard);
    stepTwo->setObjectName("StepNumber");
    stepTwo->setAlignment(Qt::AlignCenter);
    stepTwo->setFixedSize(30, 30);
    auto* stepTwoTitle = new QLabel("基础数据源与输出配置", generatorCard);
    stepTwoTitle->setObjectName("SectionTitle");
    stepTwoLayout->addWidget(stepTwo);
    stepTwoLayout->addWidget(stepTwoTitle);
    stepTwoLayout->addStretch();
    outputLayout->addLayout(stepTwoLayout);
    auto* configGridLayout = new QHBoxLayout();
    configGridLayout->setSpacing(14);
    configGridLayout->addWidget(createBoardLengthDataSourceCard(), 2);
    configGridLayout->addWidget(createOutputDirectoryCard(), 1);
    outputLayout->addLayout(configGridLayout);
    generatorLayout->addLayout(outputLayout);

    auto* actionLayout = new QHBoxLayout();
    actionLayout->setContentsMargins(22, 2, 22, 22);
    actionLayout->setSpacing(12);
    startButton_ = new QPushButton("开始生成", this);
    startButton_->setObjectName("PrimaryButton");
    startButton_->setFixedHeight(56);
    startButton_->setCursor(Qt::PointingHandCursor);

    cancelButton_ = new QPushButton("取消", this);
    cancelButton_->setObjectName("SecondaryButton");
    cancelButton_->setFixedHeight(42);
    cancelButton_->setCursor(Qt::PointingHandCursor);
    cancelButton_->setEnabled(false);

    openOutputButton_ = new QPushButton("打开输出目录", this);
    openOutputButton_->setObjectName("SecondaryButton");
    openOutputButton_->setFixedHeight(42);
    openOutputButton_->setCursor(Qt::PointingHandCursor);

    actionLayout->addWidget(startButton_, 1);
    actionLayout->addWidget(cancelButton_);
    actionLayout->addWidget(openOutputButton_);
    generatorLayout->addLayout(actionLayout);
    mainLayout->addWidget(generatorCard, 1);

    auto* queueCard = new QWidget(this);
    queueCard->setObjectName("QueueCard");
    queueCard->setFixedWidth(308);
    auto* queueLayout = new QVBoxLayout(queueCard);
    queueLayout->setContentsMargins(16, 18, 16, 18);
    queueLayout->setSpacing(12);

    auto* queueHeader = new QHBoxLayout();
    queueHeader->setContentsMargins(0, 0, 0, 0);
    auto* queueTitle = new QLabel("生成任务队列", queueCard);
    queueTitle->setObjectName("QueueTitle");
    auto* taskCount = new QLabel("4 步", queueCard);
    taskCount->setObjectName("TaskCount");
    taskCount->setAlignment(Qt::AlignCenter);
    taskCount->setFixedHeight(28);
    queueHeader->addWidget(queueTitle);
    queueHeader->addStretch();
    queueHeader->addWidget(taskCount);
    queueLayout->addLayout(queueHeader);

    statusLabel_ = new QLabel("待运行", queueCard);
    statusLabel_->setObjectName("StatusText");
    progressBar_ = new QProgressBar(queueCard);
    progressBar_->setObjectName("MainProgress");
    progressBar_->setTextVisible(false);
    progressBar_->setRange(0, 1);
    progressBar_->setValue(0);
    progressBar_->setFixedHeight(8);

    outputSummaryLabel_ = new QLabel("等待开始生成", queueCard);
    outputSummaryLabel_->setObjectName("OutputSummary");
    outputSummaryLabel_->setWordWrap(true);

    queueLayout->addWidget(createTaskCard("当前批量任务", "按病害数据表批量生成 DXF", "等待中"));
    queueLayout->addWidget(createTaskCard("生成进度", "提取符号、识别数据表、匹配基础数据源、批量写入 DXF", "待启动", 0));
    queueLayout->addWidget(outputSummaryLabel_);

    const QStringList steps = {
        "提取病害符号",
        "识别病害数据表",
        "匹配基础数据源",
        "批量生成 DXF"
    };
    for (const QString& step : steps) {
        auto* label = new QLabel(step, queueCard);
        label->setObjectName("StepPill");
        label->setProperty("state", "pending");
        label->setFixedHeight(34);
        label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        stepLabels_.push_back(label);
        queueLayout->addWidget(label);
    }
    queueLayout->addStretch();
    mainLayout->addWidget(queueCard, 0);

    connect(startButton_, &QPushButton::clicked, this, [this]() {
        ensureLogDialog();
        logEdit_->clear();
        outputSummaryLabel_->setText("任务正在运行...");
        setLogDialogRunning();
        setRunning(true);
        controller_->run(collectInput());
    });
    connect(cancelButton_, &QPushButton::clicked, controller_, &AutoCadController::cancel);
    connect(openOutputButton_, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(outputDirEdit_->text()));
    });

    contentStack_->addWidget(createTenderGenerationPage());
    contentStack_->addWidget(createAiSettingsPage());
    connect(cadNavButton_, &QPushButton::clicked, this, [this]() { setActiveModule(0); });
    connect(tenderNavButton_, &QPushButton::clicked, this, [this]() { setActiveModule(1); });
    connect(aiSettingsNavButton_, &QPushButton::clicked, this, [this]() { setActiveModule(2); });
    setActiveModule(0);
}

QWidget* AutoCadGui::createTenderGenerationPage() {
    auto* page = new QWidget(contentStack_);
    page->setObjectName("ModulePage");
    auto* pageLayout = new QHBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(24);

    tenderFileEdit_ = new QLineEdit(page);
    tenderFileEdit_->hide();

    auto* uploadCard = createModuleCard(
        "1 上传招标文件",
        "支持 PDF、DOCX、DOC、TXT。系统将提取目录、技术要求、评分标准和格式约束。");
    auto* uploadLayout = qobject_cast<QVBoxLayout*>(uploadCard->layout());

    tenderUploadStack_ = new QStackedWidget(uploadCard);
    tenderUploadStack_->setObjectName("TenderUploadStack");
    tenderUploadStack_->setMinimumHeight(208);

    auto* uploadPrompt = new QPushButton(tenderUploadStack_);
    uploadPrompt->setObjectName("TenderUploadPrompt");
    uploadPrompt->setCursor(Qt::PointingHandCursor);
    auto* promptLayout = new QVBoxLayout(uploadPrompt);
    promptLayout->setContentsMargins(20, 26, 20, 26);
    promptLayout->setSpacing(10);
    auto* uploadIcon = new QLabel("↑", uploadPrompt);
    uploadIcon->setObjectName("LargeUploadIcon");
    uploadIcon->setAlignment(Qt::AlignCenter);
    auto* uploadTitle = new QLabel("拖拽招标文件到此处，或者点击上传", uploadPrompt);
    uploadTitle->setObjectName("UploadTitle");
    uploadTitle->setAlignment(Qt::AlignCenter);
    auto* uploadHint = new QLabel("上传后显示文件名、页数、解析状态", uploadPrompt);
    uploadHint->setObjectName("UploadHint");
    uploadHint->setAlignment(Qt::AlignCenter);
    promptLayout->addStretch();
    promptLayout->addWidget(uploadIcon);
    promptLayout->addWidget(uploadTitle);
    promptLayout->addWidget(uploadHint);
    promptLayout->addStretch();
    connect(uploadPrompt, &QPushButton::clicked, this, &AutoCadGui::chooseTenderFile);
    tenderUploadStack_->addWidget(uploadPrompt);

    auto* selectedPage = new QWidget(tenderUploadStack_);
    selectedPage->setObjectName("TenderSelectedPage");
    auto* selectedLayout = new QVBoxLayout(selectedPage);
    selectedLayout->setContentsMargins(18, 18, 18, 18);
    selectedLayout->setSpacing(14);
    auto* fileRow = new QWidget(selectedPage);
    fileRow->setObjectName("UploadedTenderFile");
    fileRow->setFixedHeight(74);
    auto* fileRowLayout = new QHBoxLayout(fileRow);
    fileRowLayout->setContentsMargins(12, 10, 12, 10);
    fileRowLayout->setSpacing(12);
    auto* fileBadge = new QLabel("DOCX", fileRow);
    fileBadge->setObjectName("SelectedFileType");
    fileBadge->setAlignment(Qt::AlignCenter);
    fileBadge->setFixedWidth(64);
    tenderFileNameLabel_ = new QLabel("未选择招标文件", fileRow);
    tenderFileNameLabel_->setObjectName("SelectedFileName");
    tenderFileNameLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tenderFileMetaLabel_ = new QLabel("待解析", fileRow);
    tenderFileMetaLabel_->setObjectName("TaskMeta");
    auto* fileTextLayout = new QVBoxLayout();
    fileTextLayout->setSpacing(2);
    fileTextLayout->addWidget(tenderFileNameLabel_);
    fileTextLayout->addWidget(tenderFileMetaLabel_);
    auto* removeButton = new QPushButton("×", fileRow);
    removeButton->setObjectName("RemoveFileButton");
    removeButton->setFixedSize(28, 28);
    removeButton->setCursor(Qt::PointingHandCursor);
    connect(removeButton, &QPushButton::clicked, this, [this]() {
        if (tenderFileEdit_) {
            tenderFileEdit_->clear();
        }
        refreshTenderFileStatus();
    });
    fileRowLayout->addWidget(fileBadge);
    fileRowLayout->addLayout(fileTextLayout, 1);
    fileRowLayout->addWidget(removeButton);
    selectedLayout->addStretch();
    selectedLayout->addWidget(fileRow);
    auto* reselectButton = new QPushButton("重新选择招标文件", selectedPage);
    reselectButton->setObjectName("SmallActionButton");
    reselectButton->setFixedHeight(34);
    reselectButton->setCursor(Qt::PointingHandCursor);
    connect(reselectButton, &QPushButton::clicked, this, &AutoCadGui::chooseTenderFile);
    selectedLayout->addWidget(reselectButton, 0, Qt::AlignRight);
    selectedLayout->addStretch();
    tenderUploadStack_->addWidget(selectedPage);
    uploadLayout->addWidget(tenderUploadStack_);

    uploadLayout->addWidget(createBodyText("解析结果", "MiniSectionTitle"));
    uploadLayout->addWidget(createInfoRow("项目类型", "隧道机电工程", "已识别"));
    uploadLayout->addWidget(createInfoRow("评分项", "商务 / 技术 / 报价", "已提取"));
    uploadLayout->addWidget(createInfoRow("格式要求", "目录、页眉页脚、编号", "可应用"));
    uploadLayout->addStretch();

    auto* configCard = createModuleCard(
        "2 选择 Word 框架规则",
        "将招标文件要求转换为 Word 章节结构与格式模板，生成后可继续编辑。");
    auto* configLayout = qobject_cast<QVBoxLayout*>(configCard->layout());
    configLayout->addWidget(createOptionRow("章节结构", "根据招标文件目录自动生成一级/二级/三级标题"));
    configLayout->addWidget(createOptionRow("封面与目录", "创建封面、自动目录、页眉页脚占位"));
    configLayout->addWidget(createOptionRow("格式规范", "宋体/黑体、字号、行距、页边距"));
    configLayout->addWidget(createOptionRow("评分响应点", "把评分办法映射到技术响应章节"));
    configLayout->addWidget(createOptionRow("AI 内容占位", "每个章节保留 AI 生成内容入口"));
    auto* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(12);
    auto* generateButton = new QPushButton("生成 Word 框架", configCard);
    generateButton->setObjectName("PrimaryButton");
    generateButton->setFixedHeight(42);
    generateButton->setCursor(Qt::PointingHandCursor);
    auto* saveSchemeButton = new QPushButton("保存配置方案", configCard);
    saveSchemeButton->setObjectName("SecondaryButton");
    saveSchemeButton->setFixedHeight(42);
    saveSchemeButton->setCursor(Qt::PointingHandCursor);
    actionLayout->addWidget(generateButton);
    actionLayout->addWidget(saveSchemeButton);
    actionLayout->addStretch();
    configLayout->addLayout(actionLayout);
    configLayout->addStretch();

    auto* previewCard = createModuleCard(
        "3 预览生成结构",
        "预览区展示最终 Word 文档骨架，蓝色标签表示可由 AI 自动补全内容。");
    previewCard->setFixedWidth(360);
    auto* previewLayout = qobject_cast<QVBoxLayout*>(previewCard->layout());
    auto* documentPreview = new QWidget(previewCard);
    documentPreview->setObjectName("DocumentPreview");
    auto* documentLayout = new QVBoxLayout(documentPreview);
    documentLayout->setContentsMargins(14, 14, 14, 14);
    documentLayout->setSpacing(8);

    const auto addOutlineLine = [this, documentLayout, documentPreview](
        int index,
        const QString& title,
        const QString& tag,
        bool aiTag) {
        auto* line = new QWidget(documentPreview);
        line->setObjectName("OutlineLine");
        line->setFixedHeight(34);
        auto* lineLayout = new QHBoxLayout(line);
        lineLayout->setContentsMargins(10, 0, 8, 0);
        lineLayout->setSpacing(10);

        auto* indexLabel = new QLabel(QString("%1").arg(index, 2, 10, QLatin1Char('0')), line);
        indexLabel->setObjectName("OutlineIndex");
        auto* titleLabel = new QLabel(title, line);
        titleLabel->setObjectName("OutlineTitle");
        titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto* tagLabel = new QLabel(tag, line);
        tagLabel->setObjectName(aiTag ? "OutlineTagAi" : "OutlineTag");
        tagLabel->setAlignment(Qt::AlignCenter);
        tagLabel->setFixedHeight(24);
        tagLabel->setMinimumWidth(44);

        lineLayout->addWidget(indexLabel);
        lineLayout->addWidget(titleLabel, 1);
        lineLayout->addWidget(tagLabel);
        documentLayout->addWidget(line);
    };

    addOutlineLine(1, "封面", "格式", false);
    addOutlineLine(2, "投标函", "AI", true);
    addOutlineLine(3, "项目理解与总体方案", "AI", true);
    addOutlineLine(4, "施工组织设计", "AI", true);
    addOutlineLine(5, "质量与安全保障措施", "AI", true);
    addOutlineLine(6, "主要设备材料响应表", "表格", false);
    addOutlineLine(7, "评分办法响应索引", "AI", true);
    addOutlineLine(8, "商务偏离表", "表格", false);
    addOutlineLine(9, "报价文件占位", "格式", false);
    previewLayout->addWidget(documentPreview);
    previewLayout->addStretch();

    pageLayout->addWidget(uploadCard, 1);
    pageLayout->addWidget(configCard, 1);
    pageLayout->addWidget(previewCard);
    refreshTenderFileStatus();
    return page;
}

QWidget* AutoCadGui::createAiSettingsPage() {
    auto* page = new QWidget(contentStack_);
    page->setObjectName("ModulePage");
    auto* pageLayout = new QHBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(24);

    auto* apiCard = createModuleCard("1 API Key 与模型", "密钥仅保存在本机配置中，界面默认以掩码显示。");
    apiCard->setFixedWidth(360);
    auto* apiLayout = qobject_cast<QVBoxLayout*>(apiCard->layout());
    apiLayout->addWidget(createBodyText("服务商", "FieldTitle"));
    apiLayout->addWidget(createSettingsInput("OpenAI / 兼容接口"));
    apiLayout->addWidget(createBodyText("API Base URL", "FieldTitle"));
    apiLayout->addWidget(createSettingsInput("https://api.openai.com/v1"));
    apiLayout->addWidget(createBodyText("API Key", "FieldTitle"));
    apiLayout->addWidget(createSettingsInput("sk-...", true));
    apiLayout->addWidget(createBodyText("默认模型", "FieldTitle"));
    apiLayout->addWidget(createSettingsInput("gpt-4.1 / deepseek-chat / 自定义"));
    auto* apiActions = new QHBoxLayout();
    apiActions->setSpacing(12);
    auto* testButton = new QPushButton("测试连接", apiCard);
    testButton->setObjectName("PrimaryButton");
    testButton->setFixedHeight(42);
    auto* saveButton = new QPushButton("保存设置", apiCard);
    saveButton->setObjectName("SecondaryButton");
    saveButton->setFixedHeight(42);
    apiActions->addWidget(testButton);
    apiActions->addWidget(saveButton);
    apiLayout->addLayout(apiActions);
    auto* statusBox = new QWidget(apiCard);
    statusBox->setObjectName("SuccessNotice");
    auto* statusLayout = new QVBoxLayout(statusBox);
    statusLayout->setContentsMargins(14, 10, 14, 10);
    statusLayout->setSpacing(4);
    statusLayout->addWidget(createBodyText("连接状态：待测试", "SuccessTitle"));
    statusLayout->addWidget(createBodyText("测试成功后才能启用 AI 内容生成。", "MutedText"));
    apiLayout->addWidget(statusBox);
    apiLayout->addStretch();

    auto* promptCard = createModuleCard("2 自定义提示词", "提示词支持变量，可按章节生成内容并写入标书框架。");
    auto* promptLayout = qobject_cast<QVBoxLayout*>(promptCard->layout());
    auto* tabRow = new QHBoxLayout();
    tabRow->setSpacing(8);
    for (const QString& tab : {"技术方案", "商务响应", "评分响应"}) {
        auto* button = new QPushButton(tab, promptCard);
        button->setObjectName(tab == "技术方案" ? "PrimaryButton" : "SecondaryButton");
        button->setFixedHeight(36);
        tabRow->addWidget(button);
    }
    tabRow->addStretch();
    promptLayout->addLayout(tabRow);
    auto* promptEditor = new QTextEdit(promptCard);
    promptEditor->setObjectName("PromptEditor");
    promptEditor->setFixedHeight(240);
    promptEditor->setPlainText(
        "你是资深投标文件编制专家。\n"
        "请基于 {招标文件摘要}、{章节名称}、{评分标准}\n"
        "生成符合招标要求的技术响应内容。\n"
        "要求：结构清晰、避免夸大、可直接写入 Word。");
    promptLayout->addWidget(promptEditor);
    promptLayout->addWidget(createBodyText("可用变量", "MiniSectionTitle"));
    auto* variableRow = new QHBoxLayout();
    variableRow->setSpacing(8);
    for (const QString& variable : {"{招标文件摘要}", "{章节名称}", "{评分标准}", "{项目类型}"}) {
        auto* chip = new QLabel(variable, promptCard);
        chip->setObjectName("VariableChip");
        chip->setAlignment(Qt::AlignCenter);
        chip->setFixedHeight(30);
        variableRow->addWidget(chip);
    }
    variableRow->addStretch();
    promptLayout->addLayout(variableRow);
    auto* promptActions = new QHBoxLayout();
    promptActions->setSpacing(12);
    auto* previewButton = new QPushButton("预览生成内容", promptCard);
    previewButton->setObjectName("PrimaryButton");
    previewButton->setFixedHeight(42);
    auto* applyButton = new QPushButton("应用到标书章节", promptCard);
    applyButton->setObjectName("SecondaryButton");
    applyButton->setFixedHeight(42);
    promptActions->addWidget(previewButton);
    promptActions->addWidget(applyButton);
    promptActions->addStretch();
    promptLayout->addLayout(promptActions);

    auto* flowCard = createModuleCard("3 应用到 Word 框架", "AI 内容先进入候选区，由用户确认后写入对应章节。");
    flowCard->setFixedWidth(330);
    auto* flowLayout = qobject_cast<QVBoxLayout*>(flowCard->layout());
    flowLayout->addWidget(createFlowStep(1, "读取招标文件摘要", "来自标书解析结果"));
    flowLayout->addWidget(createFlowStep(2, "选择 Word 章节", "例如：施工组织设计"));
    flowLayout->addWidget(createFlowStep(3, "套用提示词生成内容", "调用用户配置的模型", true));
    flowLayout->addWidget(createFlowStep(4, "进入候选内容区", "用户编辑、确认、替换"));
    flowLayout->addWidget(createFlowStep(5, "写入 Word 框架", "生成 docx 或更新章节"));
    auto* candidateBox = new QWidget(flowCard);
    candidateBox->setObjectName("CandidateBox");
    auto* candidateLayout = new QVBoxLayout(candidateBox);
    candidateLayout->setContentsMargins(12, 12, 12, 12);
    candidateLayout->setSpacing(6);
    candidateLayout->addWidget(createBodyText("候选内容预览", "MiniSectionTitle"));
    auto* candidateText = createBodyText("本章节将从项目理解、施工组织、质量安全、进度保障四个方面展开...", "MutedText");
    candidateText->setWordWrap(true);
    candidateLayout->addWidget(candidateText);
    flowLayout->addWidget(candidateBox);
    flowLayout->addStretch();

    pageLayout->addWidget(apiCard);
    pageLayout->addWidget(promptCard, 1);
    pageLayout->addWidget(flowCard);
    return page;
}

QWidget* AutoCadGui::createCard(const QString& title, QLayout* contentLayout) {
    auto* card = new QWidget(this);
    card->setObjectName("Card");

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 18);
    layout->setSpacing(14);

    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("CardTitle");
    layout->addWidget(titleLabel);

    auto* content = new QWidget(card);
    content->setLayout(contentLayout);
    layout->addWidget(content);

    return card;
}

QWidget* AutoCadGui::createModuleCard(const QString& title, const QString& hint) {
    auto* card = new QWidget(this);
    card->setObjectName("ModuleCard");
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(14);

    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("ModuleCardTitle");
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    auto* hintLabel = new QLabel(hint, card);
    hintLabel->setObjectName("ModuleCardHint");
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);
    return card;
}

QWidget* AutoCadGui::createInfoRow(const QString& label, const QString& value, const QString& chip) {
    auto* row = new QWidget(this);
    row->setObjectName("InfoRow");
    row->setFixedHeight(38);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    auto* labelNode = new QLabel(label, row);
    labelNode->setObjectName("InfoLabel");
    auto* valueNode = new QLabel(value, row);
    valueNode->setObjectName("InfoValue");
    auto* chipNode = new QLabel(chip, row);
    chipNode->setObjectName("SuccessChip");
    chipNode->setAlignment(Qt::AlignCenter);
    chipNode->setFixedHeight(26);
    chipNode->setMinimumWidth(68);
    layout->addWidget(labelNode);
    layout->addWidget(valueNode);
    layout->addStretch();
    layout->addWidget(chipNode);
    return row;
}

QWidget* AutoCadGui::createOptionRow(const QString& title, const QString& description) {
    auto* row = new QWidget(this);
    row->setObjectName("OptionRow");
    row->setFixedHeight(62);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(12);
    auto* check = new QLabel("✓", row);
    check->setObjectName("OptionCheck");
    check->setAlignment(Qt::AlignCenter);
    check->setFixedSize(24, 24);
    auto* textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);
    auto* titleNode = new QLabel(title, row);
    titleNode->setObjectName("OptionTitle");
    auto* descNode = new QLabel(description, row);
    descNode->setObjectName("OptionDescription");
    descNode->setWordWrap(true);
    textLayout->addWidget(titleNode);
    textLayout->addWidget(descNode);
    layout->addWidget(check);
    layout->addLayout(textLayout, 1);
    return row;
}

QWidget* AutoCadGui::createFlowStep(int number, const QString& title, const QString& description, bool active) {
    auto* step = new QWidget(this);
    step->setObjectName("FlowStep");
    step->setFixedHeight(68);
    auto* layout = new QHBoxLayout(step);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(12);
    auto* numberLabel = new QLabel(QString::number(number), step);
    numberLabel->setObjectName(active ? "FlowNumberActive" : "FlowNumber");
    numberLabel->setAlignment(Qt::AlignCenter);
    numberLabel->setFixedSize(32, 32);
    auto* textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);
    auto* titleLabel = new QLabel(title, step);
    titleLabel->setObjectName("FlowTitle");
    auto* descLabel = new QLabel(description, step);
    descLabel->setObjectName("FlowDescription");
    descLabel->setWordWrap(true);
    textLayout->addWidget(titleLabel);
    textLayout->addWidget(descLabel);
    layout->addWidget(numberLabel);
    layout->addLayout(textLayout, 1);
    return step;
}

QWidget* AutoCadGui::createBoardLengthDataSourceCard() {
    auto* card = new QWidget(this);
    card->setObjectName("BoardLengthDataSourceCard");
    card->setFixedHeight(174);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* layout = new QHBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);

    auto* icon = new QLabel("DATA", card);
    icon->setObjectName("DataSourceIcon");
    icon->setAlignment(Qt::AlignCenter);
    icon->setFixedSize(48, 48);
    layout->addWidget(icon, 0, Qt::AlignTop);

    auto* textLayout = new QVBoxLayout();
    textLayout->setSpacing(6);

    auto* titleRow = new QHBoxLayout();
    titleRow->setSpacing(10);
    auto* title = new QLabel("衬砌长度/板长数据源", card);
    title->setObjectName("DataSourceTitle");
    titleRow->addWidget(title);
    boardLengthSourceStatusLabel_ = new QLabel("默认", card);
    boardLengthSourceStatusLabel_->setObjectName("DataSourceStatusChip");
    boardLengthSourceStatusLabel_->setProperty("state", "default");
    boardLengthSourceStatusLabel_->setAlignment(Qt::AlignCenter);
    boardLengthSourceStatusLabel_->setFixedHeight(26);
    boardLengthSourceStatusLabel_->setMinimumWidth(70);
    titleRow->addWidget(boardLengthSourceStatusLabel_);
    titleRow->addStretch();
    textLayout->addLayout(titleRow);

    auto* description = new QLabel("基础数据源用于匹配病害数据表，不作为本次任务上传文件。", card);
    description->setObjectName("DataSourceDescription");
    description->setWordWrap(true);
    textLayout->addWidget(description);

    auto* currentRow = new QHBoxLayout();
    currentRow->setSpacing(8);
    auto* currentLabel = new QLabel("当前使用", card);
    currentLabel->setObjectName("DataSourceCurrentLabel");
    boardLengthSourceNameLabel_ = new QLabel("未选择数据源", card);
    boardLengthSourceNameLabel_->setObjectName("BoardLengthSourceName");
    boardLengthSourceNameLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    currentRow->addWidget(currentLabel);
    currentRow->addWidget(boardLengthSourceNameLabel_, 1);
    textLayout->addLayout(currentRow);

    boardLengthSourceMetaLabel_ = new QLabel("请选择板长分布表目录", card);
    boardLengthSourceMetaLabel_->setObjectName("BoardLengthSourceMeta");
    boardLengthSourceMetaLabel_->setWordWrap(true);
    textLayout->addWidget(boardLengthSourceMetaLabel_);

    layout->addLayout(textLayout, 1);

    auto* buttonLayout = new QVBoxLayout();
    buttonLayout->setSpacing(8);
    auto* changeButton = new QPushButton("更换数据源", card);
    changeButton->setObjectName("ChangeDataSourceButton");
    changeButton->setFixedHeight(34);
    changeButton->setCursor(Qt::PointingHandCursor);
    auto* defaultButton = new QPushButton("恢复默认", card);
    defaultButton->setObjectName("DefaultSourceButton");
    defaultButton->setFixedHeight(34);
    defaultButton->setCursor(Qt::PointingHandCursor);
    buttonLayout->addWidget(changeButton);
    buttonLayout->addWidget(defaultButton);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);

    connect(changeButton, &QPushButton::clicked, this, &AutoCadGui::chooseBoardLengthSource);
    connect(defaultButton, &QPushButton::clicked, this, &AutoCadGui::resetBoardLengthSource);

    return card;
}

QWidget* AutoCadGui::createOutputDirectoryCard() {
    auto* card = new QWidget(this);
    card->setObjectName("OutputPathCard");
    card->setFixedHeight(174);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto* title = new QLabel("输出目录", card);
    title->setObjectName("OutputPathTitle");
    auto* hint = new QLabel("生成的 DXF、JSON 和日志文件将写入此目录", card);
    hint->setObjectName("OutputPathHint");
    hint->setWordWrap(true);
    layout->addWidget(title);
    layout->addWidget(hint);

    auto* row = new QHBoxLayout();
    row->setSpacing(12);
    outputDirEdit_ = new QLineEdit(card);
    outputDirEdit_->setObjectName("PathEdit");
    outputDirEdit_->setFixedHeight(40);
    auto* button = new QPushButton(card);
    button->setObjectName("IconButton");
    button->setFixedSize(40, 40);
    button->setCursor(Qt::PointingHandCursor);
    button->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
    connect(button, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getExistingDirectory(this, "选择输出目录", outputDirEdit_->text());
        if (!path.isEmpty()) {
            outputDirEdit_->setText(QDir::toNativeSeparators(path));
        }
    });

    row->addWidget(outputDirEdit_, 1);
    row->addWidget(button);
    layout->addLayout(row);

    return card;
}

QLineEdit* AutoCadGui::createSettingsInput(const QString& placeholder, bool password) {
    auto* input = new QLineEdit(this);
    input->setObjectName("SettingsInput");
    input->setFixedHeight(40);
    input->setPlaceholderText(placeholder);
    if (password) {
        input->setEchoMode(QLineEdit::Password);
    }
    return input;
}

QLabel* AutoCadGui::createBodyText(const QString& text, const QString& objectName) {
    auto* label = new QLabel(text, this);
    if (!objectName.isEmpty()) {
        label->setObjectName(objectName);
    }
    return label;
}

QHBoxLayout* AutoCadGui::createPathRow(
    const QString& labelText,
    QLineEdit** lineEdit,
    BrowseMode mode,
    const QString& dialogTitle,
    const QString& filter) {
    auto* layout = new QHBoxLayout();
    layout->setSpacing(12);

    auto* label = new QLabel(labelText, this);
    label->setObjectName("FieldLabel");
    label->setFixedWidth(88);

    *lineEdit = new QLineEdit(this);
    (*lineEdit)->setObjectName("PathEdit");
    (*lineEdit)->setFixedHeight(38);

    auto* button = new QPushButton(this);
    button->setObjectName("IconButton");
    button->setFixedSize(38, 38);
    button->setCursor(Qt::PointingHandCursor);
    button->setIcon(style()->standardIcon(
        mode == BrowseMode::SaveFile ? QStyle::SP_DialogSaveButton : QStyle::SP_DirOpenIcon));

    connect(button, &QPushButton::clicked, this, [this, lineEdit, mode, dialogTitle, filter]() {
        const QString current = (*lineEdit)->text();
        QString path;
        if (mode == BrowseMode::ExistingDirectory) {
            path = QFileDialog::getExistingDirectory(this, dialogTitle, current);
        }
        else if (mode == BrowseMode::SaveFile) {
            path = QFileDialog::getSaveFileName(this, dialogTitle, current, filter);
        }
        else {
            path = QFileDialog::getOpenFileName(this, dialogTitle, current, filter);
        }

        if (!path.isEmpty()) {
            (*lineEdit)->setText(QDir::toNativeSeparators(path));
        }
    });

    layout->addWidget(label);
    layout->addWidget(*lineEdit, 1);
    layout->addWidget(button);
    return layout;
}

void AutoCadGui::applyStyles() {
    setStyleSheet(R"(
        QWidget {
            background: #eef2f7;
            color: #1b2733;
            font-family: "Microsoft YaHei UI", "Segoe UI";
            font-size: 13px;
        }
        QLabel {
            background: transparent;
        }
        QWidget#AppShell {
            background: #f3f6fb;
        }
        QWidget#Sidebar {
            background: #101827;
            border: none;
        }
        QWidget#Workspace,
        QWidget#CadDashboard {
            background: #f3f6fb;
            border: none;
        }
        QWidget#HeaderBar {
            background: #ffffff;
            border: 1px solid #dde7f3;
            border-radius: 16px;
        }
        QScrollArea#ContentScroll {
            background: transparent;
            border: none;
        }
        QScrollArea#ContentScroll > QWidget > QWidget {
            background: transparent;
        }
        QWidget#TopBar {
            background: #ffffff;
            border: none;
            border-bottom: 1px solid #dce5f1;
        }
        QLabel#BrandIcon {
            background: #38bdf8;
            border-radius: 12px;
            color: #082f49;
            font-size: 10px;
            font-weight: 800;
        }
        QLabel#BrandText {
            color: #f8fafc;
            font-size: 21px;
            font-weight: 800;
        }
        QLabel#SidebarLabel {
            color: #8090a7;
            font-size: 12px;
            font-weight: 700;
            padding-left: 4px;
        }
        QWidget#SidebarStatus {
            background: #172033;
            border: 1px solid #25324a;
            border-radius: 14px;
        }
        QLabel#SidebarStatusTitle {
            color: #f8fafc;
            font-weight: 700;
        }
        QLabel#SidebarStatusHint {
            color: #a8b5c7;
            font-size: 12px;
            line-height: 18px;
        }
        QPushButton#NavButton,
        QPushButton#ActiveNavButton {
            border: none;
            border-radius: 12px;
            padding: 0 14px;
            font-size: 14px;
            text-align: left;
        }
        QPushButton#NavButton {
            background: transparent;
            color: #cbd5e1;
        }
        QPushButton#NavButton:hover {
            background: #1c273a;
            color: #ffffff;
        }
        QPushButton#ActiveNavButton {
            background: #2563eb;
            color: #ffffff;
            font-weight: 700;
        }
        QLineEdit#SearchEdit {
            background: #f1f5f9;
            border: 1px solid #eef2f7;
            border-radius: 12px;
            padding: 0 14px;
            color: #23344f;
            font-size: 14px;
        }
        QLineEdit#SearchEdit:focus {
            background: #ffffff;
            border-color: #93b4f5;
        }
        QPushButton#NotificationButton {
            background: transparent;
            border: none;
            border-radius: 20px;
            color: #49617f;
            font-size: 20px;
            font-weight: 700;
        }
        QPushButton#NotificationButton:hover {
            background: #f1f5fb;
        }
        QFrame#TopDivider {
            background: #e6edf5;
            border: none;
        }
        QLabel#UserAvatar {
            background: #eaf2ff;
            border-radius: 20px;
            color: #1d4ed8;
            font-size: 14px;
            font-weight: 800;
        }
        QWidget#Header {
            background: #ffffff;
            border: 1px solid #dfe7f1;
            border-radius: 8px;
        }
        QWidget#ConfigPane {
            background: transparent;
        }
        QLabel#PageTitle {
            font-size: 24px;
            font-weight: 600;
            color: #132033;
        }
        QLabel#PageSubtitle {
            font-size: 13px;
            color: #6b7788;
        }
        QWidget#GeneratorCard,
        QWidget#QueueCard {
            background: #ffffff;
            border: 1px solid #dce5f1;
            border-radius: 18px;
        }
        QWidget#HeroPanel {
            background: #f8fbff;
            border: 1px solid #e4edf8;
            border-radius: 16px;
        }
        QLabel#HeroFlowChip {
            background: #ecfdf5;
            border: 1px solid #bbf7d0;
            border-radius: 17px;
            color: #16794a;
            font-size: 12px;
            font-weight: 700;
            padding: 0 12px;
        }
        QStackedWidget#ContentStack,
        QWidget#ModulePage {
            background: transparent;
            border: none;
        }
        QWidget#ModuleCard {
            background: #ffffff;
            border: 1px solid #dce5f1;
            border-radius: 12px;
        }
        QLabel#ModuleCardTitle {
            color: #071a38;
            font-size: 19px;
            font-weight: 700;
        }
        QLabel#ModuleCardHint {
            color: #526889;
            font-size: 13px;
        }
        QLabel#MiniSectionTitle {
            color: #071a38;
            font-size: 15px;
            font-weight: 700;
        }
        QLabel#FieldTitle {
            color: #526889;
            font-size: 13px;
            font-weight: 600;
        }
        QLabel#FeatureIcon {
            background: #eaf2ff;
            border-radius: 12px;
            color: #2563eb;
            font-size: 12px;
            font-weight: 700;
        }
        QLabel#CardBigTitle {
            color: #071a38;
            font-size: 20px;
            font-weight: 700;
        }
        QLabel#MutedText,
        QLabel#UploadHint {
            color: #526889;
        }
        QLabel#SectionTitle {
            color: #071a38;
            font-size: 17px;
            font-weight: 600;
        }
        QLabel#StepNumber {
            background: #eef4fb;
            border-radius: 15px;
            color: #0f3767;
            font-size: 14px;
            font-weight: 700;
        }
        QWidget#UploadBox,
        QWidget#TaskFilePanel {
            background: #fbfdff;
            border: 1px solid #d7e4fb;
            border-radius: 12px;
        }
        QStackedWidget#UploadStack,
        QWidget#SelectedFilesPage {
            background: transparent;
            border: none;
        }
        QPushButton#UploadPromptPage {
            background: #fbfdff;
            border: 2px dashed #c9d8eb;
            border-radius: 12px;
            text-align: center;
        }
        QPushButton#UploadPromptPage:hover {
            background: #f5f9ff;
            border-color: #87aef0;
        }
        QStackedWidget#TenderUploadStack,
        QWidget#TenderSelectedPage {
            background: transparent;
            border: none;
        }
        QPushButton#TenderUploadPrompt {
            background: #fbfdff;
            border: 2px dashed #c9d8eb;
            border-radius: 14px;
        }
        QPushButton#TenderUploadPrompt:hover {
            background: #f5f9ff;
            border-color: #87aef0;
        }
        QLabel#UploadIcon {
            background: #ffffff;
            border: 1px solid #eef2f7;
            border-radius: 29px;
            color: #8aa0bc;
            font-size: 30px;
            font-weight: 600;
        }
        QLabel#LargeUploadIcon {
            color: #8aa0bc;
            font-size: 42px;
            font-weight: 400;
        }
        QLabel#UploadTitle {
            color: #17304f;
            font-size: 16px;
            font-weight: 600;
        }
        QWidget#UploadedTenderFile {
            background: #ffffff;
            border: 1px solid #e0e8f3;
            border-radius: 10px;
        }
        QLabel#SelectedFilesTitle {
            color: #071a38;
            font-size: 16px;
            font-weight: 700;
        }
        QWidget#SelectedFileRow {
            background: #ffffff;
            border: 1px solid #e0e8f3;
            border-radius: 10px;
        }
        QLabel#SelectedFileType {
            background: #eaf2ff;
            border-radius: 8px;
            color: #1d4ed8;
            font-weight: 700;
            padding: 6px 10px;
        }
        QLabel#SelectedFileName {
            color: #17304f;
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton#SmallActionButton {
            background: #eef4ff;
            border: 1px solid #cddcf6;
            border-radius: 8px;
            color: #1d4ed8;
            padding: 0 12px;
            font-weight: 600;
        }
        QPushButton#SmallActionButton:hover {
            background: #e0ecff;
            border-color: #8fb7f7;
        }
        QPushButton#RemoveFileButton {
            background: transparent;
            border: none;
            border-radius: 14px;
            color: #8a98ab;
            font-size: 16px;
            font-weight: 700;
        }
        QPushButton#RemoveFileButton:hover {
            background: #fee2e2;
            color: #dc2626;
        }
        QWidget#InfoRow {
            background: transparent;
            border: none;
        }
        QLabel#InfoLabel {
            color: #526889;
            font-size: 13px;
        }
        QLabel#InfoValue {
            color: #17304f;
            font-size: 13px;
            font-weight: 600;
        }
        QLabel#SuccessChip {
            background: #eaf7f0;
            border-radius: 13px;
            color: #16794a;
            font-size: 12px;
            font-weight: 700;
            padding: 0 10px;
        }
        QWidget#OptionRow {
            background: #f9fbfe;
            border: 1px solid #e1e8f2;
            border-radius: 9px;
        }
        QLabel#OptionCheck {
            background: #2563eb;
            border-radius: 12px;
            color: #ffffff;
            font-size: 14px;
            font-weight: 800;
        }
        QLabel#OptionTitle {
            color: #17304f;
            font-size: 14px;
            font-weight: 700;
        }
        QLabel#OptionDescription {
            color: #526889;
            font-size: 12px;
        }
        QWidget#DocumentPreview {
            background: #fbfdff;
            border: 1px solid #e1e8f2;
            border-radius: 10px;
        }
        QWidget#OutlineLine {
            background: #ffffff;
            border: 1px solid #edf2f7;
            border-radius: 6px;
        }
        QLabel#OutlineIndex {
            color: #8a98ab;
            font-size: 12px;
            font-weight: 700;
        }
        QLabel#OutlineTitle {
            color: #17304f;
            font-size: 13px;
            font-weight: 600;
        }
        QLabel#OutlineTag,
        QLabel#OutlineTagAi {
            border-radius: 12px;
            font-size: 11px;
            font-weight: 700;
            padding: 0 8px;
        }
        QLabel#OutlineTag {
            background: #f4f7fb;
            color: #617084;
        }
        QLabel#OutlineTagAi {
            background: #eaf2ff;
            color: #1d4ed8;
        }
        QLineEdit#SettingsInput {
            background: #f9fbfe;
            border: 1px solid #cfd9e8;
            border-radius: 7px;
            padding: 0 12px;
            color: #17304f;
        }
        QLineEdit#SettingsInput:focus {
            border-color: #2563eb;
            background: #ffffff;
        }
        QWidget#SuccessNotice {
            background: #f0fdf4;
            border: 1px solid #bbf7d0;
            border-radius: 10px;
        }
        QLabel#SuccessTitle {
            color: #16794a;
            font-size: 13px;
            font-weight: 700;
        }
        QTextEdit#PromptEditor {
            background: #101828;
            border: 1px solid #1f2a3d;
            border-radius: 10px;
            padding: 10px;
            color: #d9e5f2;
            font-family: "Consolas", "Microsoft YaHei UI";
            font-size: 13px;
        }
        QLabel#VariableChip {
            background: #eef4ff;
            border: 1px solid #d7e4fb;
            border-radius: 15px;
            color: #1d4ed8;
            font-size: 12px;
            font-weight: 700;
            padding: 0 10px;
        }
        QWidget#FlowStep {
            background: #f9fbfe;
            border: 1px solid #e1e8f2;
            border-radius: 10px;
        }
        QLabel#FlowNumber,
        QLabel#FlowNumberActive {
            border-radius: 16px;
            font-size: 13px;
            font-weight: 800;
        }
        QLabel#FlowNumber {
            background: #eaf2ff;
            color: #1d4ed8;
        }
        QLabel#FlowNumberActive {
            background: #2563eb;
            color: #ffffff;
        }
        QLabel#FlowTitle {
            color: #17304f;
            font-size: 13px;
            font-weight: 700;
        }
        QLabel#FlowDescription {
            color: #526889;
            font-size: 12px;
        }
        QWidget#CandidateBox {
            background: #fbfdff;
            border: 1px solid #c9d8eb;
            border-radius: 10px;
        }
    )");

    setStyleSheet(styleSheet() + R"(
        QLabel#QueueTitle {
            color: #071a38;
            font-size: 16px;
            font-weight: 700;
        }
        QLabel#TaskCount {
            background: #eef4fb;
            border-radius: 14px;
            color: #526889;
            padding: 0 10px;
        }
        QWidget#TaskCard {
            background: #ffffff;
            border: 1px solid #e4ebf5;
            border-radius: 14px;
        }
        QLabel#TaskTitle {
            color: #071a38;
            font-size: 15px;
            font-weight: 600;
        }
        QLabel#TaskMeta,
        QLabel#StatusText {
            color: #526889;
        }
        QLabel#TaskState {
            color: #2563eb;
            font-weight: 600;
        }
        QLabel#StatusBadge {
            background: #eaf2ff;
            border: 1px solid #b8cff6;
            border-radius: 6px;
            color: #1d4ed8;
            font-weight: 600;
        }
        QWidget#Card {
            background: #ffffff;
            border: 1px solid #dfe7f1;
            border-radius: 8px;
        }
        QLabel#CardTitle {
            font-size: 15px;
            font-weight: 600;
            color: #182538;
            padding-bottom: 10px;
            border-bottom: 1px solid #e7edf5;
        }
        QLabel#FieldLabel {
            color: #536173;
            font-weight: 500;
        }
        QLabel#SidebarBrand {
            color: #2563eb;
            font-size: 12px;
            font-weight: 700;
            letter-spacing: 0px;
        }
        QLabel#SidebarTitle {
            color: #182538;
            font-size: 16px;
            font-weight: 600;
            padding-bottom: 8px;
        }
        QLabel#StepPill {
            background: #f4f7fb;
            border: 1px solid #e1e8f2;
            border-radius: 6px;
            color: #617084;
            padding-left: 12px;
            font-weight: 500;
        }
        QLabel#StepPill[state="running"] {
            background: #eaf2ff;
            border-color: #8fb7f7;
            color: #1d4ed8;
        }
        QLabel#StepPill[state="done"] {
            background: #eaf7f0;
            border-color: #94d3ad;
            color: #16794a;
        }
        QLabel#SummaryTitle {
            color: #182538;
            font-size: 18px;
            font-weight: 600;
        }
        QLabel#SummaryHint {
            color: #6b7788;
        }
        QLabel#OutputSummary {
            background: #f7f9fc;
            border: 1px solid #e1e8f2;
            border-radius: 6px;
            color: #3d4b5c;
            padding: 10px;
        }
        QLineEdit#PathEdit {
            background: #f9fbfe;
            border: 1px solid #cfd9e8;
            border-radius: 6px;
            padding: 0 10px;
            selection-background-color: #2563eb;
        }
        QLineEdit#PathEdit:focus {
            border-color: #2563eb;
        }
        QPushButton#IconButton,
        QPushButton#SecondaryButton {
            background: #f9fbfe;
            border: 1px solid #cfd9e8;
            border-radius: 6px;
        }
        QPushButton#IconButton:hover,
        QPushButton#SecondaryButton:hover {
            background: #eef4ff;
            border-color: #2563eb;
        }
        QPushButton#PrimaryButton {
            background: #2563eb;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 0 22px;
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton#PrimaryButton:hover {
            background: #1d4ed8;
        }
        QPushButton#PrimaryButton:disabled,
        QPushButton#SecondaryButton:disabled {
            background: #e5e7eb;
            color: #9ca3af;
            border-color: #e5e7eb;
        }
        QTextEdit#RunLog {
            background: #101828;
            border: 1px solid #1f2a3d;
            border-radius: 6px;
            padding: 8px;
            color: #d9e5f2;
            font-family: "Consolas", "Microsoft YaHei UI";
        }
        QProgressBar#MainProgress {
            background: #e7edf5;
            border: none;
            border-radius: 5px;
        }
        QProgressBar#MainProgress::chunk {
            background: #2563eb;
            border-radius: 5px;
        }
        QProgressBar#DialogProgress {
            background: #e7edf5;
            border: none;
            border-radius: 4px;
        }
        QProgressBar#DialogProgress::chunk {
            background: #2563eb;
            border-radius: 4px;
        }
        QDialog#LogDialog {
            background: #f5f7fb;
        }
        QLabel#DialogTitle {
            color: #071a38;
            font-size: 17px;
            font-weight: 700;
        }
        QSplitter::handle {
            background: #d9e0ea;
            border-radius: 2px;
        }
        QSplitter::handle:vertical {
            height: 8px;
            margin: 2px 0;
        }
        QSplitter::handle:hover {
            background: #b8c4d6;
        }
        QWidget#BoardLengthDataSourceCard {
            background: #fbfdff;
            border: 1px solid #d7e4fb;
            border-radius: 12px;
        }
        QWidget#OutputPathCard {
            background: #fbfdff;
            border: 1px solid #d7e4fb;
            border-radius: 12px;
        }
        QLabel#OutputPathTitle {
            color: #071a38;
            font-size: 15px;
            font-weight: 700;
        }
        QLabel#OutputPathHint {
            color: #526889;
            font-size: 12px;
        }
        QLabel#DataSourceIcon {
            background: #eef4ff;
            border: 1px solid #d7e4fb;
            border-radius: 12px;
            color: #1d4ed8;
            font-size: 10px;
            font-weight: 800;
        }
        QLabel#DataSourceTitle {
            color: #071a38;
            font-size: 15px;
            font-weight: 700;
        }
        QLabel#DataSourceDescription,
        QLabel#BoardLengthSourceMeta {
            color: #526889;
            font-size: 12px;
        }
        QLabel#DataSourceCurrentLabel {
            color: #7b8798;
            font-size: 12px;
            font-weight: 600;
        }
        QLabel#BoardLengthSourceName {
            color: #17304f;
            font-size: 13px;
            font-weight: 700;
        }
        QLabel#DataSourceStatusChip {
            border-radius: 13px;
            font-size: 12px;
            font-weight: 700;
            padding: 0 10px;
        }
        QLabel#DataSourceStatusChip[state="default"] {
            background: #eaf2ff;
            color: #1d4ed8;
        }
        QLabel#DataSourceStatusChip[state="custom"] {
            background: #f0fdf4;
            color: #16794a;
        }
        QLabel#DataSourceStatusChip[state="warning"] {
            background: #fff7ed;
            color: #c2410c;
        }
        QPushButton#ChangeDataSourceButton,
        QPushButton#DefaultSourceButton {
            background: #eef4ff;
            border: 1px solid #cddcf6;
            border-radius: 8px;
            color: #1d4ed8;
            padding: 0 12px;
            font-weight: 600;
        }
        QPushButton#ChangeDataSourceButton:hover,
        QPushButton#DefaultSourceButton:hover {
            background: #e0ecff;
            border-color: #8fb7f7;
        }
    )");
}

void AutoCadGui::setDefaults() {
    exeEdit_->setText(defaultBackendExe());
    diseaseWorkbookEdit_->setText(defaultPath("2025年鹤大隧道外观整理--本溪原始版本.xlsx"));
    boardLengthSourceEdit_->setText(defaultPath("板长分布表"));
    outputDirEdit_->setText(defaultPath("batch_output"));
}

WorkflowInput AutoCadGui::collectInput() const {
    WorkflowInput input;
    input.exePath = QDir::fromNativeSeparators(exeEdit_->text().trimmed());
    input.diseaseWorkbook = QDir::fromNativeSeparators(diseaseWorkbookEdit_->text().trimmed());
    input.boardLengthSource = QDir::fromNativeSeparators(boardLengthSourceEdit_->text().trimmed());
    input.outputDir = QDir::fromNativeSeparators(outputDirEdit_->text().trimmed());
    return input;
}

void AutoCadGui::appendLog(const QString& text) {
    ensureLogDialog();

    QTextCharFormat format;
    const QString lower = text.toLower();
    if (lower.startsWith("error:") || lower.startsWith("stderr:") || lower.contains("failed")) {
        format.setForeground(QColor("#ffb4a8"));
    }
    else if (lower.startsWith("warning:") || lower.contains(" warning")) {
        format.setForeground(QColor("#ffd166"));
    }
    else {
        format.setForeground(QColor("#d9e5f2"));
    }

    QTextCursor cursor(logEdit_->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(QString("[%1] %2\n").arg(nowText(), text), format);
    logEdit_->setTextCursor(cursor);
    logEdit_->ensureCursorVisible();
}

void AutoCadGui::setRunning(bool running) {
    startButton_->setEnabled(!running);
    cancelButton_->setEnabled(running);
    openOutputButton_->setEnabled(!running);
    if (running) {
        progressBar_->setRange(0, 0);
        statusLabel_->setText("运行中");
        for (QLabel* label : stepLabels_) {
            setStepLabelState(label, "pending");
        }
        if (!stepLabels_.isEmpty()) {
            setStepLabelState(stepLabels_.front(), "running");
        }
    }
}

void AutoCadGui::updateStepState(int current, int total) {
    Q_UNUSED(total);
    for (int i = 0; i < stepLabels_.size(); ++i) {
        if (i < current) {
            setStepLabelState(stepLabels_[i], "done");
        }
        else if (i == current && progressBar_->maximum() != 0) {
            setStepLabelState(stepLabels_[i], "running");
        }
        else {
            setStepLabelState(stepLabels_[i], "pending");
        }
    }
}

void AutoCadGui::setStepLabelState(QLabel* label, const QString& state) {
    if (!label) {
        return;
    }
    label->setProperty("state", state);
    polish(label);
}

void AutoCadGui::ensureLogDialog() {
    if (logDialog_) {
        return;
    }

    logDialog_ = new QDialog(this);
    logDialog_->setObjectName("LogDialog");
    logDialog_->setWindowTitle("运行日志");
    logDialog_->setModal(false);
    logDialog_->resize(780, 500);

    auto* layout = new QVBoxLayout(logDialog_);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);

    auto* headerLayout = new QHBoxLayout();
    auto* title = new QLabel("运行日志", logDialog_);
    title->setObjectName("DialogTitle");
    logDialogStatusLabel_ = new QLabel("待运行", logDialog_);
    logDialogStatusLabel_->setObjectName("TaskMeta");
    headerLayout->addWidget(title);
    headerLayout->addStretch();
    headerLayout->addWidget(logDialogStatusLabel_);
    layout->addLayout(headerLayout);

    logDialogProgressBar_ = new QProgressBar(logDialog_);
    logDialogProgressBar_->setObjectName("DialogProgress");
    logDialogProgressBar_->setTextVisible(false);
    logDialogProgressBar_->setFixedHeight(8);
    logDialogProgressBar_->setRange(0, 1);
    logDialogProgressBar_->setValue(0);
    layout->addWidget(logDialogProgressBar_);

    logEdit_ = new QTextEdit(logDialog_);
    logEdit_->setObjectName("RunLog");
    logEdit_->setReadOnly(true);
    logEdit_->setLineWrapMode(QTextEdit::NoWrap);
    logEdit_->setWordWrapMode(QTextOption::NoWrap);
    logEdit_->document()->setMaximumBlockCount(3000);
    layout->addWidget(logEdit_, 1);

    auto* footerLayout = new QHBoxLayout();
    footerLayout->addStretch();
    logDialogCloseButton_ = new QPushButton("关闭", logDialog_);
    logDialogCloseButton_->setObjectName("SecondaryButton");
    logDialogCloseButton_->setFixedHeight(34);
    logDialogCloseButton_->setCursor(Qt::PointingHandCursor);
    connect(logDialogCloseButton_, &QPushButton::clicked, logDialog_, &QDialog::hide);
    footerLayout->addWidget(logDialogCloseButton_);
    layout->addLayout(footerLayout);
}

void AutoCadGui::setLogDialogRunning() {
    ensureLogDialog();
    logDialogStatusLabel_->setText("运行中");
    logDialogProgressBar_->setRange(0, 0);
    logDialogProgressBar_->setValue(0);
    logDialog_->show();
    logDialog_->raise();
    logDialog_->activateWindow();
}

void AutoCadGui::setLogDialogFinished(bool ok, const QString& summary) {
    ensureLogDialog();
    logDialogStatusLabel_->setText(ok ? "完成" : "失败");
    logDialogProgressBar_->setRange(0, 1);
    logDialogProgressBar_->setValue(ok ? 1 : 0);
    if (!summary.isEmpty()) {
        logDialog_->show();
    }
}

QWidget* AutoCadGui::createTaskCard(
    const QString& title,
    const QString& meta,
    const QString& state,
    int progress) {
    auto* card = new QWidget(this);
    card->setObjectName("TaskCard");
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    auto* titleRow = new QHBoxLayout();
    auto* icon = new QLabel("CAD", card);
    icon->setObjectName("FeatureIcon");
    icon->setAlignment(Qt::AlignCenter);
    icon->setFixedSize(36, 36);
    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("TaskTitle");
    titleRow->addWidget(icon);
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    layout->addLayout(titleRow);

    auto* metaLabel = new QLabel(meta, card);
    metaLabel->setObjectName("TaskMeta");
    metaLabel->setWordWrap(true);
    layout->addWidget(metaLabel);

    if (progress >= 0) {
        auto* stateRow = new QHBoxLayout();
        stateRow->addWidget(statusLabel_);
        stateRow->addStretch();
        auto* percentLabel = new QLabel(QString::number(progress) + "%", card);
        percentLabel->setObjectName("TaskMeta");
        stateRow->addWidget(percentLabel);
        layout->addLayout(stateRow);
        layout->addWidget(progressBar_);
    }
    else {
        auto* stateLabel = new QLabel(state, card);
        stateLabel->setObjectName("TaskState");
        layout->addWidget(stateLabel);
    }

    return card;
}

void AutoCadGui::chooseProjectFiles() {
    const QString file = QFileDialog::getOpenFileName(
        this,
        "选择外观病害总表",
        workspaceRoot(),
        "Excel Workbooks (*.xlsx *.xlsm);;All Files (*)");

    if (file.isEmpty()) {
        return;
    }

    diseaseWorkbookEdit_->setText(QDir::toNativeSeparators(file));
    refreshUploadStatus();
}

void AutoCadGui::refreshUploadStatus() {
    if (!uploadStack_ || !selectedFilesLayout_) {
        return;
    }

    while (QLayoutItem* item = selectedFilesLayout_->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    addSelectedFileRow("外观病害", diseaseWorkbookEdit_);

    uploadStack_->setCurrentIndex(selectedFilesLayout_->count() > 0 ? 1 : 0);
}

void AutoCadGui::addSelectedFileRow(const QString& label, QLineEdit* lineEdit) {
    if (!selectedFilesLayout_ || !lineEdit) {
        return;
    }

    const QString path = lineEdit->text().trimmed();
    if (path.isEmpty()) {
        return;
    }

    const QFileInfo info(path);
    const QString fileName = info.fileName().isEmpty() ? path : info.fileName();

    auto* row = new QWidget(this);
    row->setObjectName("SelectedFileRow");
    row->setFixedHeight(58);

    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(12, 8, 10, 8);
    rowLayout->setSpacing(12);

    auto* typeLabel = new QLabel(label, row);
    typeLabel->setObjectName("SelectedFileType");
    typeLabel->setAlignment(Qt::AlignCenter);
    typeLabel->setFixedWidth(86);

    auto* fileNameLabel = new QLabel(fileName, row);
    fileNameLabel->setObjectName("SelectedFileName");
    fileNameLabel->setToolTip(QDir::toNativeSeparators(path));
    fileNameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto* removeButton = new QPushButton("×", row);
    removeButton->setObjectName("RemoveFileButton");
    removeButton->setFixedSize(28, 28);
    removeButton->setCursor(Qt::PointingHandCursor);

    rowLayout->addWidget(typeLabel);
    rowLayout->addWidget(fileNameLabel, 1);
    rowLayout->addWidget(removeButton);

    connect(removeButton, &QPushButton::clicked, this, [this, lineEdit]() {
        clearProjectFile(lineEdit);
    });

    selectedFilesLayout_->addWidget(row);
}

void AutoCadGui::clearProjectFile(QLineEdit* lineEdit) {
    if (!lineEdit) {
        return;
    }

    lineEdit->clear();
    refreshUploadStatus();
}

void AutoCadGui::chooseBoardLengthSource() {
    const QString current = boardLengthSourceEdit_ && !boardLengthSourceEdit_->text().trimmed().isEmpty()
        ? boardLengthSourceEdit_->text().trimmed()
        : workspaceRoot();
    const QString directory = QFileDialog::getExistingDirectory(
        this,
        "选择衬砌长度/板长数据源目录",
        current);

    if (directory.isEmpty()) {
        return;
    }

    boardLengthSourceEdit_->setText(QDir::toNativeSeparators(directory));
    refreshBoardLengthSourceStatus();
}

void AutoCadGui::resetBoardLengthSource() {
    if (!boardLengthSourceEdit_) {
        return;
    }

    boardLengthSourceEdit_->setText(defaultPath("板长分布表"));
    refreshBoardLengthSourceStatus();
}

void AutoCadGui::refreshBoardLengthSourceStatus() {
    if (!boardLengthSourceNameLabel_ || !boardLengthSourceMetaLabel_ ||
        !boardLengthSourceStatusLabel_ || !boardLengthSourceEdit_) {
        return;
    }

    const QString path = boardLengthSourceEdit_->text().trimmed();
    if (path.isEmpty()) {
        boardLengthSourceNameLabel_->setText("未选择数据源");
        boardLengthSourceNameLabel_->setToolTip(QString());
        boardLengthSourceMetaLabel_->setText("请选择板长分布表目录，否则无法匹配衬砌长度数据。");
        boardLengthSourceStatusLabel_->setText("缺失");
        boardLengthSourceStatusLabel_->setProperty("state", "warning");
        polish(boardLengthSourceStatusLabel_);
        return;
    }

    const QFileInfo info(path);
    const bool isDefaultSource = sameCleanPath(path, defaultPath("板长分布表"));
    const QString displayName = info.fileName().isEmpty() ? QDir::toNativeSeparators(path) : info.fileName();
    boardLengthSourceNameLabel_->setText(displayName);
    boardLengthSourceNameLabel_->setToolTip(QDir::toNativeSeparators(path));

    if (!info.exists() || !info.isDir()) {
        boardLengthSourceMetaLabel_->setText("目录不存在或不可访问: " + QDir::toNativeSeparators(path));
        boardLengthSourceStatusLabel_->setText("异常");
        boardLengthSourceStatusLabel_->setProperty("state", "warning");
        polish(boardLengthSourceStatusLabel_);
        return;
    }

    const int workbookCount = countExcelFiles(path);
    if (workbookCount <= 0) {
        boardLengthSourceMetaLabel_->setText("当前目录未识别到 Excel 板长表，请更换数据源目录。");
        boardLengthSourceStatusLabel_->setText("空目录");
        boardLengthSourceStatusLabel_->setProperty("state", "warning");
        polish(boardLengthSourceStatusLabel_);
        return;
    }

    boardLengthSourceMetaLabel_->setText(
        QString("已识别 %1 个 Excel 板长表，生成时将按病害数据表名称自动匹配。")
            .arg(workbookCount));
    boardLengthSourceStatusLabel_->setText(isDefaultSource ? "默认" : "自定义");
    boardLengthSourceStatusLabel_->setProperty("state", isDefaultSource ? "default" : "custom");
    polish(boardLengthSourceStatusLabel_);
}

void AutoCadGui::chooseTenderFile() {
    const QString file = QFileDialog::getOpenFileName(
        this,
        "选择招标文件",
        workspaceRoot(),
        "Tender Files (*.pdf *.docx *.doc *.txt);;Word Files (*.docx *.doc);;PDF Files (*.pdf);;Text Files (*.txt);;All Files (*)");

    if (file.isEmpty()) {
        return;
    }

    tenderFileEdit_->setText(QDir::toNativeSeparators(file));
    refreshTenderFileStatus();
}

void AutoCadGui::refreshTenderFileStatus() {
    if (!tenderUploadStack_ || !tenderFileEdit_) {
        return;
    }

    const QString path = tenderFileEdit_->text().trimmed();
    if (path.isEmpty()) {
        tenderUploadStack_->setCurrentIndex(0);
        return;
    }

    const QFileInfo info(path);
    if (tenderFileNameLabel_) {
        tenderFileNameLabel_->setText(info.fileName());
        tenderFileNameLabel_->setToolTip(QDir::toNativeSeparators(path));
    }
    if (tenderFileMetaLabel_) {
        const qint64 kb = qMax<qint64>(1, info.size() / 1024);
        tenderFileMetaLabel_->setText(QString("%1 KB · 待解析章节和格式要求").arg(kb));
    }
    tenderUploadStack_->setCurrentIndex(1);
}

void AutoCadGui::setActiveModule(int index) {
    if (!contentStack_) {
        return;
    }

    contentStack_->setCurrentIndex(index);
    const QVector<QPushButton*> buttons = {cadNavButton_, tenderNavButton_, aiSettingsNavButton_};
    for (int i = 0; i < buttons.size(); ++i) {
        if (!buttons[i]) {
            continue;
        }
        buttons[i]->setObjectName(i == index ? "ActiveNavButton" : "NavButton");
        polish(buttons[i]);
    }
    if (fileLibraryNavButton_) {
        fileLibraryNavButton_->setObjectName("NavButton");
        polish(fileLibraryNavButton_);
    }

    if (index == 0) {
        pageTitleLabel_->setText("报告转 CAD 工作台");
        pageSubtitleLabel_->setText("上传外观病害总表，选择衬砌长度/板长数据源，自动生成多个标准 CAD 图纸文档。");
    }
    else if (index == 1) {
        pageTitleLabel_->setText("标书 Word 框架生成");
        pageSubtitleLabel_->setText("上传招标文件，自动解析章节、评分项与格式要求，生成可编辑的 Word 标书框架。");
    }
    else {
        pageTitleLabel_->setText("AI 设置与提示词管理");
        pageSubtitleLabel_->setText("配置 AI API Key、模型参数和自定义提示词，生成内容可应用到标书 Word 框架中的章节。");
    }
}
