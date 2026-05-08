#include "AutoCadGui.h"

#include "AntDesignStyle.h"

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
#include <QPair>
#include <QProgressBar>
#include <QPushButton>
#include <QComboBox>
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

    connect(controller_, &AutoCadController::progressTextChanged, this, [this](const QString& text) {
        appendLog(text);
        const QString lowerText = text.toLower();
        const bool isError = lowerText.contains("error") || text.contains("错误") || text.contains("失败");
        const bool isWarning = lowerText.contains("warning") || text.contains("警告");
        if (generationInfoMode_ && (isError || isWarning)) {
            appendGenerationIssueMessage(text, isError ? "error" : "warning");
        }
    });
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
            statusLabel_->setText(ok ? "生成完成" : "生成失败，请查看日志");
            statusLabel_->show();
            appendGenerationCompletionMessage(ok, summary);
            if (openOutputButton_) {
                openOutputButton_->show();
            }
            setGenerationStatus(ok ? "已完成" : "有错误", ok ? "done" : "error");
            updateStepState(ok ? stepLabels_.size() : 0, stepLabels_.size());
            appendLog(summary);
            setLogDialogFinished(ok, summary);
        });
}

void AutoCadGui::setupUi() {
    setWindowTitle("报告转 CAD 工作台");
    setMinimumSize(1280, 740);
    setObjectName("AppShell");

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* topBar = new QWidget(this);
    topBar->setObjectName("TopBar");
    AntDesignStyle::applyTopBarShadow(topBar);
    topBar->setFixedHeight(72);
    auto* topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setContentsMargins(28, 8, 28, 8);
    topBarLayout->setSpacing(18);

    auto* brandGroup = new QHBoxLayout();
    brandGroup->setSpacing(8);
    auto* brandRow = new QHBoxLayout();
    brandRow->setSpacing(8);
    auto* brandIcon = new QLabel("✎", topBar);
    brandIcon->setObjectName("BrandIcon");
    brandIcon->setAlignment(Qt::AlignCenter);
    brandIcon->setFixedSize(40, 40);
    auto* brandText = new QLabel("ReportCAD.", topBar);
    brandText->setObjectName("BrandText");
    brandRow->addWidget(brandIcon);
    brandRow->addWidget(brandText);
    brandGroup->addLayout(brandRow);
    topBarLayout->addLayout(brandGroup);
    topBarLayout->addSpacing(24);

    const auto makeNavButton = [this, topBar](const QString& text, QStyle::StandardPixmap icon, bool active) {
        auto* button = new QPushButton(text, topBar);
        button->setObjectName(active ? "ActiveNavButton" : "NavButton");
        button->setIcon(style()->standardIcon(icon));
        button->setFixedHeight(46);
        button->setMinimumWidth(92);
        button->setCursor(Qt::PointingHandCursor);
        return button;
    };
    cadNavButton_ = makeNavButton("CAD 生成", QStyle::SP_FileDialogDetailedView, true);
    tenderNavButton_ = makeNavButton("标书生成", QStyle::SP_FileDialogListView, false);
    fileLibraryNavButton_ = makeNavButton("文件库", QStyle::SP_FileIcon, false);
    aiSettingsNavButton_ = makeNavButton("AI 设置", QStyle::SP_FileDialogInfoView, false);
    topBarLayout->addWidget(cadNavButton_);
    topBarLayout->addWidget(tenderNavButton_);
    topBarLayout->addWidget(fileLibraryNavButton_);
    topBarLayout->addWidget(aiSettingsNavButton_);
    topBarLayout->addStretch();

    auto* searchEdit = new QLineEdit(topBar);
    searchEdit->setObjectName("SearchEdit");
    searchEdit->setPlaceholderText("搜索...");
    searchEdit->setFixedSize(320, 42);
    searchEdit->addAction(style()->standardIcon(QStyle::SP_FileDialogContentsView), QLineEdit::LeadingPosition);
    topBarLayout->addWidget(searchEdit);

    auto* notificationButton = new QPushButton("!", topBar);
    notificationButton->setObjectName("NotificationButton");
    notificationButton->setFixedSize(38, 38);
    notificationButton->setCursor(Qt::PointingHandCursor);
    topBarLayout->addWidget(notificationButton);

    auto* divider = new QFrame(topBar);
    divider->setObjectName("TopDivider");
    divider->setFixedSize(1, 28);
    topBarLayout->addWidget(divider);

    auto* userAvatar = new QLabel("US", topBar);
    userAvatar->setObjectName("UserAvatar");
    userAvatar->setAlignment(Qt::AlignCenter);
    userAvatar->setFixedSize(40, 40);
    topBarLayout->addWidget(userAvatar);
    rootLayout->addWidget(topBar);

    auto* bodyLayout = new QVBoxLayout();
    bodyLayout->setContentsMargins(28, 24, 28, 12);
    bodyLayout->setSpacing(8);
    rootLayout->addLayout(bodyLayout, 1);

    auto* pageTitleRow = new QHBoxLayout();
    pageTitleRow->setSpacing(12);
    pageTitleIconLabel_ = new QLabel("≡", this);
    pageTitleIconLabel_->setObjectName("PageTitleIcon");
    pageTitleIconLabel_->setAlignment(Qt::AlignCenter);
    pageTitleIconLabel_->setFixedSize(28, 28);
    pageTitleLabel_ = new QLabel("批量生成衬砌平面图", this);
    pageTitleLabel_->setObjectName("PageTitle");
    pageTitleRow->addWidget(pageTitleIconLabel_);
    pageTitleRow->addWidget(pageTitleLabel_);
    pageTitleRow->addStretch();
    bodyLayout->addLayout(pageTitleRow);

    pageSubtitleLabel_ = new QLabel("上传外观病害总表，自动匹配板长基础数据，批量生成各个数据表的 DXF 图纸。", this);
    pageSubtitleLabel_->setObjectName("PageSubtitle");
    bodyLayout->addWidget(pageSubtitleLabel_);
    bodyLayout->addSpacing(18);

    contentStack_ = new QStackedWidget(this);
    contentStack_->setObjectName("ContentStack");
    bodyLayout->addWidget(contentStack_, 1);

    auto* cadPage = new QWidget(contentStack_);
    cadPage->setObjectName("ModulePage");
    auto* cadPageLayout = new QVBoxLayout(cadPage);
    cadPageLayout->setContentsMargins(0, 0, 0, 0);
    cadPageLayout->setSpacing(0);

    auto* cadDashboard = new QWidget(cadPage);
    cadDashboard->setObjectName("CadDashboard");
    cadDashboard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* mainLayout = new QHBoxLayout(cadDashboard);
    mainLayout->setContentsMargins(0, 0, 0, 12);
    mainLayout->setSpacing(24);
    cadPageLayout->addWidget(cadDashboard);
    contentStack_->addWidget(cadPage);

    exeEdit_ = new QLineEdit(this);
    exeEdit_->hide();

    auto* leftColumn = new QWidget(this);
    leftColumn->setObjectName("LeftWorkflowColumn");
    leftColumn->setMinimumWidth(600);
    auto* leftColumnLayout = new QVBoxLayout(leftColumn);
    leftColumnLayout->setContentsMargins(0, 0, 0, 0);
    leftColumnLayout->setSpacing(22);

    auto* generatorCard = new QWidget(leftColumn);
    generatorCard->setObjectName("GeneratorCard");
    AntDesignStyle::applyCardShadow(generatorCard);
    auto* generatorLayout = new QVBoxLayout(generatorCard);
    generatorLayout->setContentsMargins(24, 20, 24, 22);
    generatorLayout->setSpacing(18);

    auto* generatorHeader = new QHBoxLayout();
    generatorHeader->setSpacing(10);
    auto* iconLabel = new QLabel("▦", generatorCard);
    iconLabel->setObjectName("FeatureIcon");
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setFixedSize(32, 32);
    auto* headerText = new QVBoxLayout();
    headerText->setSpacing(2);
    auto* cardTitle = new QLabel("本次任务文件", generatorCard);
    cardTitle->setObjectName("CardBigTitle");
    auto* cardSubtitle = new QLabel("外观病害 Excel 是本次生成任务的主输入", generatorCard);
    cardSubtitle->setObjectName("MutedText");
    headerText->addWidget(cardTitle);
    headerText->addWidget(cardSubtitle);
    generatorHeader->addWidget(iconLabel);
    generatorHeader->addLayout(headerText, 1);
    generatorHeader->addStretch();
    generatorLayout->addLayout(generatorHeader);

    auto* inputLayout = new QVBoxLayout();
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->setSpacing(14);

    diseaseWorkbookEdit_ = new QLineEdit(this);
    diseaseWorkbookEdit_->hide();
    boardLengthSourceEdit_ = new QLineEdit(this);
    boardLengthSourceEdit_->hide();

    auto* uploadBox = new QWidget(generatorCard);
    uploadBox->setObjectName("TaskFilePanel");
    uploadBox->setFixedHeight(112);
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
    promptLayout->setContentsMargins(20, 12, 20, 12);
    promptLayout->setSpacing(4);

    auto* cloudIcon = new QLabel("▦", uploadPromptPage);
    cloudIcon->setObjectName("UploadIcon");
    cloudIcon->setAlignment(Qt::AlignCenter);
    cloudIcon->setFixedSize(44, 44);
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
    selectedPageLayout->setContentsMargins(0, 12, 0, 12);
    selectedPageLayout->setSpacing(0);

    selectedFilesLayout_ = new QVBoxLayout();
    selectedFilesLayout_->setSpacing(0);
    selectedPageLayout->addLayout(selectedFilesLayout_);
    uploadStack_->addWidget(selectedFilesPage);

    inputLayout->addWidget(uploadBox);
    generatorLayout->addLayout(inputLayout);
    leftColumnLayout->addWidget(generatorCard);

    auto* outputLayout = new QVBoxLayout();
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->setSpacing(0);
    auto* configGridLayout = new QHBoxLayout();
    configGridLayout->setSpacing(24);
    configGridLayout->addWidget(createBoardLengthDataSourceCard(), 1);
    configGridLayout->addWidget(createOutputDirectoryCard(), 1);
    outputLayout->addLayout(configGridLayout);
    leftColumnLayout->addLayout(outputLayout);
    mainLayout->addWidget(leftColumn, 1, Qt::AlignTop);

    auto* generationControlCard = createGenerationControlCard();
    mainLayout->addWidget(generationControlCard, 0, Qt::AlignTop);

    connect(startButton_, &QPushButton::clicked, this, [this]() {
        ensureLogDialog();
        logEdit_->clear();
        generationIssueMessages_.clear();
        setGenerationInfoMode(true);
        setGenerationOutputSummary("正在生成 DXF，警告和错误信息会显示在这里。", "running");
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

QWidget* AutoCadGui::createGenerationControlCard() {
    auto* card = new QWidget(this);
    card->setObjectName("GenerationControlCard");
    AntDesignStyle::applyCardShadow(card);
    card->setFixedWidth(440);
    card->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(14);

    auto* header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    generationTitleLabel_ = new QLabel("生成控制", card);
    generationTitleLabel_->setObjectName("GenerationTitle");
    generationStatusBadge_ = new QLabel("待检查", card);
    generationStatusBadge_->setObjectName("GenerationStatusBadge");
    generationStatusBadge_->setProperty("state", "pending");
    generationStatusBadge_->setAlignment(Qt::AlignCenter);
    generationStatusBadge_->setFixedHeight(26);
    generationStatusBadge_->setMinimumWidth(70);
    header->addWidget(generationTitleLabel_);
    header->addStretch();
    header->addWidget(generationStatusBadge_);
    layout->addLayout(header);

    auto* checkTitle = new QLabel("生成前检查", card);
    checkTitle->setObjectName("GenerationSectionTitle");
    generationPreflightWidgets_.push_back(checkTitle);
    layout->addWidget(checkTitle);
    auto* workbookRow = createGenerationCheckRow(&checkWorkbookIconLabel_, &checkWorkbookTextLabel_);
    auto* boardSourceRow = createGenerationCheckRow(&checkBoardSourceIconLabel_, &checkBoardSourceTextLabel_);
    auto* outputRow = createGenerationCheckRow(&checkOutputIconLabel_, &checkOutputTextLabel_);
    auto* backendRow = createGenerationCheckRow(&checkBackendIconLabel_, &checkBackendTextLabel_);
    generationPreflightWidgets_.push_back(workbookRow);
    generationPreflightWidgets_.push_back(boardSourceRow);
    generationPreflightWidgets_.push_back(outputRow);
    generationPreflightWidgets_.push_back(backendRow);
    layout->addWidget(workbookRow);
    layout->addWidget(boardSourceRow);
    layout->addWidget(outputRow);
    layout->addWidget(backendRow);

    auto* compactNote = new QLabel("配置项在左侧维护，这里只显示是否满足生成条件。", card);
    compactNote->setObjectName("GenerationCompactNote");
    compactNote->setWordWrap(true);
    generationPreflightWidgets_.push_back(compactNote);
    layout->addWidget(compactNote);

    statusLabel_ = new QLabel("等待开始生成", card);
    statusLabel_->setObjectName("GenerationRuntimeText");
    statusLabel_->setWordWrap(true);
    statusLabel_->hide();
    layout->addWidget(statusLabel_);

    progressBar_ = new QProgressBar(card);
    progressBar_->setObjectName("MainProgress");
    progressBar_->setTextVisible(false);
    progressBar_->setRange(0, 1);
    progressBar_->setValue(0);
    progressBar_->setFixedHeight(8);
    progressBar_->hide();
    layout->addWidget(progressBar_);

    outputSummaryLabel_ = new QTextEdit(card);
    outputSummaryLabel_->setObjectName("OutputSummary");
    outputSummaryLabel_->setReadOnly(true);
    outputSummaryLabel_->setLineWrapMode(QTextEdit::WidgetWidth);
    outputSummaryLabel_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    outputSummaryLabel_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    outputSummaryLabel_->setMinimumHeight(180);
    outputSummaryLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    outputSummaryLabel_->hide();
    layout->addWidget(outputSummaryLabel_, 1);

    layout->addStretch();

    startButton_ = new AntPrimaryButton("▷  开始生成", card);
    startButton_->setObjectName("QueuePrimaryButton");
    startButton_->setFixedHeight(58);
    startButton_->setCursor(Qt::PointingHandCursor);
    layout->addWidget(startButton_);

    auto* secondaryActions = new QHBoxLayout();
    secondaryActions->setSpacing(10);
    cancelButton_ = new QPushButton("取消生成", card);
    cancelButton_->setObjectName("SecondaryButton");
    cancelButton_->setFixedHeight(36);
    cancelButton_->setCursor(Qt::PointingHandCursor);
    cancelButton_->setEnabled(false);
    openOutputButton_ = new QPushButton("打开输出目录", card);
    openOutputButton_->setObjectName("SecondaryButton");
    openOutputButton_->setFixedHeight(36);
    openOutputButton_->setCursor(Qt::PointingHandCursor);
    secondaryActions->addWidget(cancelButton_, 1);
    secondaryActions->addWidget(openOutputButton_, 1);
    cancelButton_->hide();
    openOutputButton_->hide();
    layout->addLayout(secondaryActions);

    updateGenerationPanel();
    return card;
}

QWidget* AutoCadGui::createGenerationCheckRow(QLabel** iconLabel, QLabel** textLabel) {
    auto* row = new QWidget(this);
    row->setObjectName("GenerationCheckRow");
    row->setFixedHeight(42);

    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(12);

    auto* icon = new QLabel("-", row);
    icon->setObjectName("GenerationCheckIcon");
    icon->setProperty("state", "pending");
    icon->setAlignment(Qt::AlignCenter);
    icon->setFixedSize(24, 24);

    auto* text = new QLabel("待检查", row);
    text->setObjectName("GenerationCheckText");
    text->setProperty("state", "pending");
    text->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    layout->addWidget(icon);
    layout->addWidget(text, 1);

    if (iconLabel) {
        *iconLabel = icon;
    }
    if (textLabel) {
        *textLabel = text;
    }
    return row;
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
    auto* generateButton = new AntPrimaryButton("生成 Word 框架", configCard);
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
    pageLayout->setSpacing(30);

    const auto createAiCard = [page](const QString& iconText, const QString& title, const QString& tone) {
        auto* card = new QWidget(page);
        card->setObjectName("AiSettingsCard");
        AntDesignStyle::applyCardShadow(card);
        card->setMinimumWidth(300);

        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(26, 28, 26, 26);
        layout->setSpacing(18);

        auto* header = new QHBoxLayout();
        header->setSpacing(12);
        auto* icon = new QLabel(iconText, card);
        icon->setObjectName("AiCardIcon");
        icon->setProperty("tone", tone);
        icon->setAlignment(Qt::AlignCenter);
        icon->setFixedSize(26, 26);
        auto* titleLabel = new QLabel(title, card);
        titleLabel->setObjectName("AiCardTitle");
        header->addWidget(icon);
        header->addWidget(titleLabel, 1);
        layout->addLayout(header);

        auto* divider = new QFrame(card);
        divider->setObjectName("AiCardDivider");
        divider->setFixedHeight(1);
        layout->addWidget(divider);
        return QPair<QWidget*, QVBoxLayout*>(card, layout);
    };

    const auto addFieldLabel = [](QVBoxLayout* layout, QWidget* parent, const QString& text) {
        auto* label = new QLabel(text, parent);
        label->setObjectName("AiFieldLabel");
        layout->addWidget(label);
        return label;
    };

    auto apiPair = createAiCard("▣", "API Key 与模型", "blue");
    auto* apiCard = apiPair.first;
    auto* apiLayout = apiPair.second;
    apiLayout->addSpacing(14);
    addFieldLabel(apiLayout, apiCard, "服务商");
    auto* serviceCombo = new QComboBox(apiCard);
    serviceCombo->setObjectName("AiComboBox");
    serviceCombo->addItem("OpenAI");
    serviceCombo->addItem("兼容接口");
    serviceCombo->setFixedHeight(46);
    apiLayout->addWidget(serviceCombo);
    addFieldLabel(apiLayout, apiCard, "API Base URL");
    auto* baseUrlInput = createSettingsInput(QString());
    baseUrlInput->setText("https://api.openai.com/v1");
    apiLayout->addWidget(baseUrlInput);
    addFieldLabel(apiLayout, apiCard, "API Key");
    auto* keyInput = createSettingsInput(QString(), true);
    keyInput->setText("sk-placeholder-key");
    keyInput->addAction(style()->standardIcon(QStyle::SP_DialogYesButton), QLineEdit::LeadingPosition);
    apiLayout->addWidget(keyInput);
    addFieldLabel(apiLayout, apiCard, "默认模型");
    auto* modelInput = createSettingsInput(QString());
    modelInput->setText("gpt-4o");
    apiLayout->addWidget(modelInput);
    apiLayout->addStretch();
    auto* apiFooter = new QHBoxLayout();
    apiFooter->setSpacing(10);
    auto* statusBadge = new QLabel("●  状态：待测试", apiCard);
    statusBadge->setObjectName("AiStatusBadge");
    statusBadge->setAlignment(Qt::AlignCenter);
    statusBadge->setFixedHeight(34);
    auto* testButton = new QPushButton("测试连接", apiCard);
    testButton->setObjectName("AiGhostButton");
    testButton->setFixedHeight(42);
    auto* saveButton = new AntPrimaryButton("▣  保存", apiCard);
    saveButton->setObjectName("AiBlueButton");
    saveButton->setFixedHeight(42);
    apiFooter->addWidget(statusBadge);
    apiFooter->addStretch();
    apiFooter->addWidget(testButton);
    apiFooter->addWidget(saveButton);
    apiLayout->addLayout(apiFooter);

    auto promptPair = createAiCard("⌘", "自定义提示词", "purple");
    auto* promptCard = promptPair.first;
    auto* promptLayout = promptPair.second;
    auto* tabBar = new QWidget(promptCard);
    tabBar->setObjectName("PromptTabBar");
    tabBar->setFixedHeight(50);
    auto* tabRow = new QHBoxLayout(tabBar);
    tabRow->setContentsMargins(4, 4, 4, 4);
    tabRow->setSpacing(6);
    for (const QString& tab : {"技术方案", "商务响应", "评分响应"}) {
        auto* button = new QPushButton(tab, tabBar);
        button->setObjectName(tab == "商务响应" ? "PromptTabActive" : "PromptTab");
        button->setFixedHeight(42);
        tabRow->addWidget(button, 1);
    }
    promptLayout->addWidget(tabBar);

    auto* promptEditorBox = new QWidget(promptCard);
    promptEditorBox->setObjectName("PromptEditorBox");
    auto* editorBoxLayout = new QVBoxLayout(promptEditorBox);
    editorBoxLayout->setContentsMargins(0, 0, 0, 0);
    editorBoxLayout->setSpacing(0);
    auto* promptEditor = new QTextEdit(promptEditorBox);
    promptEditor->setObjectName("PromptEditor");
    promptEditor->setFixedHeight(196);
    promptEditor->setPlainText(
        "你是资深投标文件编制专家。\n"
        "请基于 {招标文件摘要}、{章节名称}、\n"
        "{评分标准}\n"
        "生成符合招标要求的技术响应内容。\n"
        "要求：结构清晰、避免夸大、可直接写入\n"
        "Word。");
    editorBoxLayout->addWidget(promptEditor);
    auto* variablesPanel = new QWidget(promptEditorBox);
    variablesPanel->setObjectName("PromptVariablePanel");
    auto* variablesLayout = new QVBoxLayout(variablesPanel);
    variablesLayout->setContentsMargins(16, 12, 16, 14);
    variablesLayout->setSpacing(8);
    auto* variableTitle = new QLabel("可用变量", variablesPanel);
    variableTitle->setObjectName("AiSmallLabel");
    variablesLayout->addWidget(variableTitle);
    auto* variableRow1 = new QHBoxLayout();
    variableRow1->setSpacing(8);
    auto* variableRow2 = new QHBoxLayout();
    variableRow2->setSpacing(8);
    int variableIndex = 0;
    for (const QString& variable : {"{招标文件摘要}", "{章节名称}", "{评分标准}", "{项目类型}"}) {
        auto* chip = new QLabel(variable, variablesPanel);
        chip->setObjectName("VariableChip");
        chip->setAlignment(Qt::AlignCenter);
        chip->setFixedHeight(32);
        (variableIndex < 3 ? variableRow1 : variableRow2)->addWidget(chip);
        ++variableIndex;
    }
    variableRow1->addStretch();
    variableRow2->addStretch();
    variablesLayout->addLayout(variableRow1);
    variablesLayout->addLayout(variableRow2);
    editorBoxLayout->addWidget(variablesPanel);
    promptLayout->addWidget(promptEditorBox, 1);
    auto* promptActions = new QHBoxLayout();
    promptActions->setSpacing(10);
    promptActions->addStretch();
    auto* previewButton = new QPushButton("▷  预览生成", promptCard);
    previewButton->setObjectName("AiGhostButton");
    previewButton->setFixedHeight(46);
    auto* applyButton = new AntPrimaryButton("应用到标书章节", promptCard);
    applyButton->setObjectName("AiPurpleButton");
    applyButton->setAccentColor(QColor("#722ed1"));
    applyButton->setFixedHeight(46);
    promptActions->addWidget(previewButton);
    promptActions->addWidget(applyButton);
    promptLayout->addLayout(promptActions);

    auto flowPair = createAiCard("✓", "应用到 Word 框架", "teal");
    auto* flowCard = flowPair.first;
    auto* flowLayout = flowPair.second;
    flowLayout->addSpacing(10);
    const QVector<QPair<QString, QString>> flowSteps = {
        {"✓", "读取招标文件摘要"},
        {"✓", "选择 Word 章节"},
        {"3", "套用提示词生成内容"},
        {"4", "进入候选内容区"},
        {"5", "写入 Word 框架"}
    };
    int flowIndex = 0;
    for (const auto& step : flowSteps) {
        auto* stepWidget = new QWidget(flowCard);
        stepWidget->setObjectName("AiFlowStep");
        auto* stepLayout = new QHBoxLayout(stepWidget);
        stepLayout->setContentsMargins(0, 0, 0, 0);
        stepLayout->setSpacing(14);
        auto* number = new QLabel(step.first, stepWidget);
        number->setObjectName("AiFlowNumber");
        number->setProperty("state", flowIndex < 2 ? "done" : (flowIndex == 2 ? "active" : "pending"));
        number->setAlignment(Qt::AlignCenter);
        number->setFixedSize(flowIndex == 2 ? 38 : 34, flowIndex == 2 ? 38 : 34);
        auto* text = new QLabel(step.second, stepWidget);
        text->setObjectName("AiFlowText");
        text->setProperty("state", flowIndex == 2 ? "active" : (flowIndex > 2 ? "pending" : "done"));
        stepLayout->addWidget(number);
        stepLayout->addWidget(text, 1);
        flowLayout->addWidget(stepWidget);
        flowLayout->addSpacing(flowIndex == 2 ? 10 : 6);
        ++flowIndex;
    }
    flowLayout->addSpacing(10);
    auto* candidateTitle = new QLabel("候选内容预览", flowCard);
    candidateTitle->setObjectName("AiPreviewTitle");
    flowLayout->addWidget(candidateTitle);
    auto* candidateBox = new QWidget(flowCard);
    candidateBox->setObjectName("AiCandidateBox");
    candidateBox->setFixedHeight(160);
    auto* candidateLayout = new QVBoxLayout(candidateBox);
    candidateLayout->setContentsMargins(14, 14, 14, 14);
    auto* candidateText = new QLabel("本章节将从项目理解、施工组织、质量安全、\n进度保障四个方面展开论述。针对本项目的地\n质特点，我们采用...", candidateBox);
    candidateText->setObjectName("AiCandidateText");
    candidateText->setWordWrap(true);
    candidateLayout->addWidget(candidateText);
    flowLayout->addWidget(candidateBox);
    flowLayout->addStretch();

    pageLayout->addWidget(apiCard, 1);
    pageLayout->addWidget(promptCard, 1);
    pageLayout->addWidget(flowCard, 1);
    return page;
}

QWidget* AutoCadGui::createCard(const QString& title, QLayout* contentLayout) {
    auto* card = new QWidget(this);
    card->setObjectName("Card");
    AntDesignStyle::applyCardShadow(card, 24, 6, 16);

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
    AntDesignStyle::applyCardShadow(card);
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
    AntDesignStyle::applyCardShadow(card);
    card->setFixedHeight(286);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(24, 20, 24, 22);
    layout->setSpacing(12);

    auto* titleRow = new QHBoxLayout();
    titleRow->setSpacing(8);
    auto* icon = new QLabel("⌘", card);
    icon->setObjectName("MiniCardIcon");
    icon->setAlignment(Qt::AlignCenter);
    icon->setFixedSize(24, 24);
    auto* title = new QLabel("衬砌长度/板长数据源", card);
    title->setObjectName("DataSourceTitle");
    titleRow->addWidget(icon);
    titleRow->addWidget(title, 1);
    boardLengthSourceStatusLabel_ = new QLabel("默认", card);
    boardLengthSourceStatusLabel_->setObjectName("DataSourceStatusChip");
    boardLengthSourceStatusLabel_->setProperty("state", "default");
    boardLengthSourceStatusLabel_->setAlignment(Qt::AlignCenter);
    boardLengthSourceStatusLabel_->setFixedHeight(22);
    boardLengthSourceStatusLabel_->setMinimumWidth(48);
    titleRow->addWidget(boardLengthSourceStatusLabel_);
    layout->addLayout(titleRow);

    auto* description = new QLabel("基础数据源用于匹配病害数据表，不作为本次任务上传文件。", card);
    description->setObjectName("DataSourceDescription");
    description->setWordWrap(true);
    layout->addWidget(description);
    layout->addStretch();

    auto* pathBox = new QWidget(card);
    pathBox->setObjectName("DataSourcePathBox");
    pathBox->setFixedHeight(96);
    auto* pathBoxLayout = new QVBoxLayout(pathBox);
    pathBoxLayout->setContentsMargins(12, 8, 12, 8);
    pathBoxLayout->setSpacing(4);
    auto* currentLabel = new QLabel("当前使用目录", pathBox);
    currentLabel->setObjectName("DataSourceCurrentLabel");
    boardLengthSourceNameLabel_ = new QLabel("未选择数据源", pathBox);
    boardLengthSourceNameLabel_->setObjectName("BoardLengthSourceName");
    boardLengthSourceNameLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    boardLengthSourceMetaLabel_ = new QLabel("请选择板长分布表目录", pathBox);
    boardLengthSourceMetaLabel_->setObjectName("BoardLengthSourceMeta");
    boardLengthSourceMetaLabel_->setWordWrap(true);
    pathBoxLayout->addWidget(currentLabel);
    pathBoxLayout->addWidget(boardLengthSourceNameLabel_);
    pathBoxLayout->addWidget(boardLengthSourceMetaLabel_);
    layout->addWidget(pathBox);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);
    auto* changeButton = new QPushButton("更换数据源", card);
    changeButton->setObjectName("ChangeDataSourceButton");
    changeButton->setFixedHeight(34);
    changeButton->setCursor(Qt::PointingHandCursor);
    auto* defaultButton = new QPushButton("恢复默认", card);
    defaultButton->setObjectName("DefaultSourceButton");
    defaultButton->setFixedHeight(34);
    defaultButton->setCursor(Qt::PointingHandCursor);
    buttonLayout->addWidget(changeButton, 1);
    buttonLayout->addWidget(defaultButton);
    layout->addLayout(buttonLayout);

    connect(changeButton, &QPushButton::clicked, this, &AutoCadGui::chooseBoardLengthSource);
    connect(defaultButton, &QPushButton::clicked, this, &AutoCadGui::resetBoardLengthSource);

    return card;
}

QWidget* AutoCadGui::createOutputDirectoryCard() {
    auto* card = new QWidget(this);
    card->setObjectName("OutputPathCard");
    AntDesignStyle::applyCardShadow(card);
    card->setFixedHeight(286);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(24, 20, 24, 22);
    layout->setSpacing(12);

    auto* titleRow = new QHBoxLayout();
    titleRow->setSpacing(8);
    auto* icon = new QLabel("□", card);
    icon->setObjectName("MiniCardIcon");
    icon->setAlignment(Qt::AlignCenter);
    icon->setFixedSize(24, 24);
    auto* title = new QLabel("批量输出目录", card);
    title->setObjectName("OutputPathTitle");
    titleRow->addWidget(icon);
    titleRow->addWidget(title, 1);
    layout->addLayout(titleRow);

    auto* hint = new QLabel("生成的 DXF、JSON 和日志文件将写入此目录", card);
    hint->setObjectName("OutputPathHint");
    hint->setWordWrap(true);
    layout->addWidget(hint);
    layout->addStretch();

    auto* pathBox = new QWidget(card);
    pathBox->setObjectName("DataSourcePathBox");
    pathBox->setFixedHeight(76);
    auto* pathBoxLayout = new QVBoxLayout(pathBox);
    pathBoxLayout->setContentsMargins(12, 8, 12, 8);
    pathBoxLayout->setSpacing(5);
    auto* currentLabel = new QLabel("当前输出路径", pathBox);
    currentLabel->setObjectName("DataSourceCurrentLabel");
    outputDirEdit_ = new QLineEdit(card);
    outputDirEdit_->setObjectName("PathEdit");
    outputDirEdit_->setFixedHeight(34);
    connect(outputDirEdit_, &QLineEdit::textChanged, this, [this]() {
        updateGenerationPanel();
    });
    pathBoxLayout->addWidget(currentLabel);
    pathBoxLayout->addWidget(outputDirEdit_);
    layout->addWidget(pathBox);

    auto* button = new QPushButton("更改输出目录", card);
    button->setObjectName("ChangeDataSourceButton");
    button->setFixedHeight(34);
    button->setCursor(Qt::PointingHandCursor);
    connect(button, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getExistingDirectory(this, "选择输出目录", outputDirEdit_->text());
        if (!path.isEmpty()) {
            outputDirEdit_->setText(QDir::toNativeSeparators(path));
            updateGenerationPanel();
        }
    });
    layout->addWidget(button);

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
            background: #f8fafc;
            color: #1e293b;
            font-family: "Microsoft YaHei UI", "Segoe UI";
            font-size: 13px;
        }
        QLabel {
            background: transparent;
        }
        QWidget#AppShell {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #f4edff, stop:0.48 #f8fafc, stop:1 #eefdfa);
        }
        QWidget#Workspace,
        QWidget#LeftWorkflowColumn {
            background: transparent;
            border: none;
        }
        QWidget#CadDashboard {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #f4edff, stop:0.48 #f8fafc, stop:1 #eefdfa);
            border: none;
        }
        QWidget#TopBar {
            background: #ffffff;
            border: none;
            border-bottom: 1px solid #e2e8f0;
        }
        QLabel#BrandIcon {
            background: #2563eb;
            border-radius: 12px;
            color: #ffffff;
            font-size: 20px;
            font-weight: 800;
        }
        QLabel#BrandText {
            color: #0f172a;
            font-size: 25px;
            font-weight: 800;
        }
        QPushButton#NavButton,
        QPushButton#ActiveNavButton {
            border: none;
            border-radius: 12px;
            padding: 0 18px;
            font-size: 16px;
        }
        QPushButton#NavButton {
            background: transparent;
            color: #475569;
        }
        QPushButton#NavButton:hover {
            background: #f1f5f9;
            color: #0f172a;
        }
        QPushButton#ActiveNavButton {
            background: #eff6ff;
            color: #1d4ed8;
            font-weight: 600;
        }
        QLineEdit#SearchEdit {
            background: #f8fafc;
            border: 1px solid transparent;
            border-radius: 21px;
            padding: 0 16px;
            color: #334155;
            font-size: 16px;
        }
        QLineEdit#SearchEdit:focus {
            background: #ffffff;
            border-color: #93c5fd;
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
            background: #dbeafe;
            border-radius: 17px;
            color: #1d4ed8;
            font-size: 13px;
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
            font-size: 28px;
            font-weight: 800;
            color: #0f172a;
        }
        QLabel#PageTitleIcon {
            color: #2563eb;
            font-size: 24px;
            font-weight: 900;
        }
        QLabel#PageTitleIcon[tone="ai"] {
            color: #a855f7;
        }
        QLabel#PageTitleIcon[tone="word"] {
            color: #6366f1;
        }
        QLabel#PageSubtitle {
            font-size: 16px;
            color: #52678a;
        }
        QWidget#GeneratorCard,
        QWidget#QueueCard {
            background: #ffffff;
            border: 1px solid #ffffff;
            border-radius: 20px;
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
            background: transparent;
            border-radius: 0px;
            color: #2563eb;
            font-size: 22px;
            font-weight: 700;
        }
        QLabel#CardBigTitle {
            color: #0f172a;
            font-size: 24px;
            font-weight: 800;
        }
        QLabel#MutedText,
        QLabel#UploadHint {
            color: #64748b;
            font-size: 13px;
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
            background: #ffffff;
            border: none;
            border-radius: 12px;
        }
        QStackedWidget#UploadStack,
        QWidget#SelectedFilesPage {
            background: transparent;
            border: none;
        }
        QPushButton#UploadPromptPage {
            background: #f8fafc;
            border: 2px dashed #e2e8f0;
            border-radius: 12px;
            text-align: center;
        }
        QPushButton#UploadPromptPage:hover {
            background: #eff6ff;
            border-color: #93c5fd;
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
            border: 1px solid #f1f5f9;
            border-radius: 22px;
            color: #60a5fa;
            font-size: 22px;
            font-weight: 600;
        }
        QLabel#LargeUploadIcon {
            color: #8aa0bc;
            font-size: 42px;
            font-weight: 400;
        }
        QLabel#UploadTitle {
            color: #334155;
            font-size: 14px;
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
            background: #eff6ff;
            border: 1px solid #bfdbfe;
            border-radius: 16px;
        }
        QLabel#SelectedFileType {
            background: #ffffff;
            border: 1px solid #e2e8f0;
            border-radius: 12px;
            color: #2563eb;
            font-weight: 700;
            font-size: 20px;
        }
        QLabel#SelectedFileCategory {
            color: #2563eb;
            font-size: 13px;
            font-weight: 700;
        }
        QLabel#SelectedFileName {
            color: #1e293b;
            font-size: 15px;
            font-weight: 500;
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
            border-radius: 17px;
            color: #8aa0bc;
            font-size: 22px;
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
        QWidget#AiSettingsCard {
            background: #ffffff;
            border: 1px solid #ffffff;
            border-radius: 18px;
        }
        QLabel#AiCardIcon {
            background: transparent;
            border: none;
            font-size: 22px;
            font-weight: 900;
        }
        QLabel#AiCardIcon[tone="blue"] {
            color: #2563eb;
        }
        QLabel#AiCardIcon[tone="purple"] {
            color: #a855f7;
        }
        QLabel#AiCardIcon[tone="teal"] {
            color: #0fbaaa;
        }
        QLabel#AiCardTitle {
            color: #1e293b;
            font-size: 24px;
            font-weight: 800;
        }
        QFrame#AiCardDivider {
            background: #edf2f7;
            border: none;
        }
        QLabel#AiFieldLabel {
            color: #334155;
            font-size: 16px;
            font-weight: 500;
        }
        QComboBox#AiComboBox,
        QLineEdit#SettingsInput {
            background: #f8fafc;
            border: 1px solid #dbe3ef;
            border-radius: 10px;
            padding: 0 14px;
            color: #1e293b;
            font-size: 16px;
            selection-background-color: #2563eb;
        }
        QComboBox#AiComboBox:focus,
        QLineEdit#SettingsInput:focus {
            background: #ffffff;
            border-color: #93c5fd;
        }
        QComboBox#AiComboBox::drop-down {
            width: 28px;
            border: none;
        }
        QLabel#AiStatusBadge {
            background: #fff7ed;
            border: 1px solid #fed7aa;
            border-radius: 6px;
            color: #f59e0b;
            padding: 0 10px;
            font-size: 13px;
            font-weight: 700;
        }
        QPushButton#AiGhostButton {
            background: #ffffff;
            border: 1px solid #dbe3ef;
            border-radius: 10px;
            color: #475569;
            padding: 0 18px;
            font-size: 15px;
            font-weight: 600;
        }
        QPushButton#AiGhostButton:hover {
            background: #f8fafc;
            border-color: #cbd5e1;
        }
        QPushButton#AiBlueButton {
            background: #2563eb;
            border: none;
            border-radius: 10px;
            color: #ffffff;
            padding: 0 18px;
            font-size: 15px;
            font-weight: 700;
        }
        QPushButton#AiPurpleButton {
            background: #9d19f5;
            border: none;
            border-radius: 10px;
            color: #ffffff;
            padding: 0 20px;
            font-size: 15px;
            font-weight: 700;
        }
        QWidget#PromptTabBar {
            background: #f8fafc;
            border: none;
            border-radius: 12px;
        }
        QPushButton#PromptTab,
        QPushButton#PromptTabActive {
            border-radius: 10px;
            border: none;
            font-size: 16px;
            font-weight: 500;
        }
        QPushButton#PromptTab {
            background: transparent;
            color: #64748b;
        }
        QPushButton#PromptTabActive {
            background: #ffffff;
            border: 1px solid #dbe3ef;
            color: #2563eb;
        }
        QWidget#PromptEditorBox {
            background: #f8fafc;
            border: 1px solid #dbe3ef;
            border-radius: 10px;
        }
        QTextEdit#PromptEditor {
            background: #f8fafc;
            border: none;
            border-radius: 10px;
            color: #334155;
            padding: 12px 14px;
            font-family: "Microsoft YaHei UI", "Segoe UI";
            font-size: 15px;
            line-height: 22px;
        }
        QWidget#PromptVariablePanel {
            background: #f1f5f9;
            border-top: 1px solid #dbe3ef;
            border-radius: 0px;
        }
        QLabel#AiSmallLabel {
            color: #52678a;
            font-size: 13px;
            font-weight: 500;
        }
        QLabel#VariableChip {
            background: #ffffff;
            border: 1px solid #dbe3ef;
            border-radius: 6px;
            color: #8b1cf6;
            font-size: 13px;
            font-weight: 700;
            padding: 0 10px;
        }
        QWidget#AiFlowStep {
            background: transparent;
            border: none;
        }
        QLabel#AiFlowNumber {
            border-radius: 17px;
            font-size: 14px;
            font-weight: 800;
        }
        QLabel#AiFlowNumber[state="done"] {
            background: #b9f6ec;
            color: #0f766e;
        }
        QLabel#AiFlowNumber[state="active"] {
            background: #14b8a6;
            color: #ffffff;
            border-radius: 19px;
            font-size: 15px;
        }
        QLabel#AiFlowNumber[state="pending"] {
            background: #f1f5f9;
            color: #94a3b8;
        }
        QLabel#AiFlowText {
            background: transparent;
            border: none;
            color: #52678a;
            font-size: 16px;
            font-weight: 500;
        }
        QLabel#AiFlowText[state="active"] {
            color: #0f766e;
            font-weight: 800;
        }
        QLabel#AiFlowText[state="pending"] {
            color: #94a3b8;
        }
        QLabel#AiPreviewTitle {
            color: #334155;
            font-size: 15px;
            font-weight: 600;
        }
        QWidget#AiCandidateBox {
            background: #ecfdf5;
            border: 1px solid #b9f6ec;
            border-radius: 10px;
        }
        QLabel#AiCandidateText {
            color: #64748b;
            font-size: 15px;
            line-height: 24px;
        }
        QLabel#QueueTitle {
            color: #1e293b;
            font-size: 24px;
            font-weight: 800;
        }
        QLabel#TaskCount {
            background: #dbeafe;
            border-radius: 13px;
            color: #1d4ed8;
            padding: 0 10px;
            font-size: 13px;
            font-weight: 700;
        }
        QWidget#QueueStepItem {
            background: transparent;
            border: none;
        }
        QLabel#QueueStepNumber {
            background: #f1f5f9;
            border: none;
            border-radius: 15px;
            color: #8aa0bc;
            font-size: 15px;
            font-weight: 800;
        }
        QLabel#QueueStepText {
            background: transparent;
            border: none;
            color: #607590;
            font-size: 16px;
            font-weight: 500;
        }
        QLabel#QueueStepText[state="running"] {
            color: #1d4ed8;
        }
        QLabel#QueueStepText[state="done"] {
            color: #0f766e;
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
        QLabel#StepPill {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 12px;
            color: #64748b;
            padding-left: 18px;
            font-size: 17px;
            font-weight: 600;
        }
        QLabel#StepPill[state="running"] {
            background: #eff6ff;
            border-color: #bfdbfe;
            color: #1d4ed8;
        }
        QLabel#StepPill[state="done"] {
            background: #f8fafc;
            border-color: #f1f5f9;
            color: #0f766e;
        }
        QLabel#SummaryTitle {
            color: #182538;
            font-size: 18px;
            font-weight: 600;
        }
        QLabel#SummaryHint {
            color: #6b7788;
        }
        QTextEdit#OutputSummary {
            background: #f8fafc;
            border: 1px solid #e2e8f0;
            border-radius: 10px;
            color: #475569;
            padding: 10px;
        }
        QTextEdit#OutputSummary[state="running"] {
            background: #eff6ff;
            border-color: #bfdbfe;
            color: #1d4ed8;
        }
        QTextEdit#OutputSummary[state="warning"] {
            background: #fffbeb;
            border-color: #fde68a;
            color: #92400e;
        }
        QTextEdit#OutputSummary[state="error"] {
            background: #fef2f2;
            border-color: #fecaca;
            color: #991b1b;
        }
        QTextEdit#OutputSummary[state="done"] {
            background: #ecfdf5;
            border-color: #a7f3d0;
            color: #065f46;
        }
        QLineEdit#PathEdit {
            background: transparent;
            border: none;
            border-radius: 6px;
            padding: 0 0;
            selection-background-color: #2563eb;
            color: #1e293b;
            font-weight: 600;
        }
        QLineEdit#PathEdit:focus {
            background: #ffffff;
        }
        QPushButton#IconButton,
        QPushButton#SecondaryButton {
            background: #ffffff;
            border: 1px solid #e2e8f0;
            border-radius: 8px;
            color: #475569;
            font-weight: 600;
        }
        QPushButton#IconButton:hover,
        QPushButton#SecondaryButton:hover {
            background: #f8fafc;
            border-color: #cbd5e1;
        }
        QPushButton#PrimaryButton {
            background: #2563eb;
            color: #ffffff;
            border: none;
            border-radius: 14px;
            padding: 0 22px;
            font-size: 20px;
            font-weight: 600;
        }
        QPushButton#PrimaryButton:hover {
            background: #1d4ed8;
        }
        QPushButton#QueuePrimaryButton {
            background: #2563eb;
            color: #ffffff;
            border: none;
            border-radius: 14px;
            padding: 0 22px;
            font-size: 18px;
            font-weight: 600;
        }
        QPushButton#QueuePrimaryButton:hover {
            background: #1d4ed8;
        }
        QPushButton#QueuePrimaryButton:disabled {
            background: #e5e7eb;
            color: #94a3b8;
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
            background: #ffffff;
            border: 1px solid #ffffff;
            border-radius: 20px;
        }
        QWidget#OutputPathCard {
            background: #ffffff;
            border: 1px solid #ffffff;
            border-radius: 20px;
        }
        QLabel#OutputPathTitle {
            color: #1e293b;
            font-size: 17px;
            font-weight: 500;
        }
        QLabel#OutputPathHint {
            color: #52678a;
            font-size: 14px;
        }
        QWidget#DataSourcePathBox {
            background: #f8fafc;
            border: 1px solid #dbe3ef;
            border-radius: 10px;
        }
        QLabel#MiniCardIcon {
            background: transparent;
            border: none;
            color: #475569;
            font-size: 18px;
            font-weight: 800;
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
            color: #1e293b;
            font-size: 17px;
            font-weight: 500;
        }
        QLabel#DataSourceDescription,
        QLabel#BoardLengthSourceMeta {
            color: #52678a;
            font-size: 14px;
        }
        QLabel#DataSourceCurrentLabel {
            color: #64748b;
            font-size: 14px;
        }
        QLabel#BoardLengthSourceName {
            color: #1e293b;
            font-size: 15px;
            font-weight: 500;
        }
        QLabel#DataSourceStatusChip {
            border-radius: 4px;
            font-size: 10px;
            font-weight: 700;
            padding: 0 8px;
        }
        QLabel#DataSourceStatusChip[state="default"] {
            background: #f1f5f9;
            color: #64748b;
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
            background: #ffffff;
            border: 1px solid #e2e8f0;
            border-radius: 8px;
            color: #475569;
            padding: 0 12px;
            font-weight: 600;
            font-size: 14px;
        }
        QPushButton#ChangeDataSourceButton:hover,
        QPushButton#DefaultSourceButton:hover {
            background: #f8fafc;
            border-color: #cbd5e1;
        }
    )");

    setStyleSheet(styleSheet() + AntDesignStyle::styleSheet());
    syncGenerationControlHeight();
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
    if (startButton_) {
        startButton_->setEnabled(!running);
    }
    if (cancelButton_) {
        cancelButton_->setEnabled(running);
        cancelButton_->setVisible(running);
    }
    if (openOutputButton_) {
        openOutputButton_->setEnabled(!running);
        if (running) {
            openOutputButton_->hide();
        }
    }
    if (running) {
        setGenerationInfoMode(true);
        progressBar_->setRange(0, 0);
        progressBar_->show();
        statusLabel_->setText("生成任务正在运行...");
        statusLabel_->show();
        setGenerationOutputSummary("正在生成 DXF，警告和错误信息会显示在这里。", "running");
        setGenerationStatus("生成中", "running");
        for (QLabel* label : stepLabels_) {
            setStepLabelState(label, "pending");
        }
        if (!stepLabels_.isEmpty()) {
            setStepLabelState(stepLabels_.front(), "running");
        }
    }
    else {
        if (!generationInfoMode_) {
            updateGenerationPanel();
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
    updateGenerationPanel();
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
    row->setFixedHeight(92);

    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(20, 14, 20, 14);
    rowLayout->setSpacing(18);

    auto* typeLabel = new QLabel("▦", row);
    typeLabel->setObjectName("SelectedFileType");
    typeLabel->setAlignment(Qt::AlignCenter);
    typeLabel->setFixedSize(52, 52);

    auto* categoryLabel = new QLabel(label, row);
    categoryLabel->setObjectName("SelectedFileCategory");

    auto* fileNameLabel = new QLabel(fileName, row);
    fileNameLabel->setObjectName("SelectedFileName");
    fileNameLabel->setToolTip(QDir::toNativeSeparators(path));
    fileNameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto* fileTextLayout = new QVBoxLayout();
    fileTextLayout->setSpacing(4);
    fileTextLayout->addWidget(categoryLabel);
    fileTextLayout->addWidget(fileNameLabel);

    auto* removeButton = new QPushButton("×", row);
    removeButton->setObjectName("RemoveFileButton");
    removeButton->setFixedSize(34, 34);
    removeButton->setCursor(Qt::PointingHandCursor);

    rowLayout->addWidget(typeLabel);
    rowLayout->addLayout(fileTextLayout, 1);
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
        updateGenerationPanel();
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
        updateGenerationPanel();
        return;
    }

    const int workbookCount = countExcelFiles(path);
    if (workbookCount <= 0) {
        boardLengthSourceMetaLabel_->setText("当前目录未识别到 Excel 板长表，请更换数据源目录。");
        boardLengthSourceStatusLabel_->setText("空目录");
        boardLengthSourceStatusLabel_->setProperty("state", "warning");
        polish(boardLengthSourceStatusLabel_);
        updateGenerationPanel();
        return;
    }

    boardLengthSourceMetaLabel_->setText(
        QString("已识别 %1 个 Excel 板长表，生成时将按病害数据表名称自动匹配。")
            .arg(workbookCount));
    boardLengthSourceStatusLabel_->setText(isDefaultSource ? "默认" : "自定义");
    boardLengthSourceStatusLabel_->setProperty("state", isDefaultSource ? "default" : "custom");
    polish(boardLengthSourceStatusLabel_);
    updateGenerationPanel();
}

void AutoCadGui::updateGenerationPanel() {
    if (!generationStatusBadge_) {
        return;
    }

    const bool running = startButton_ && !startButton_->isEnabled() && cancelButton_ && cancelButton_->isEnabled();
    if (generationInfoMode_ && !running) {
        setGenerationInfoMode(false);
    }

    const QString workbookPath = diseaseWorkbookEdit_ ? diseaseWorkbookEdit_->text().trimmed() : QString();
    const QString boardPath = boardLengthSourceEdit_ ? boardLengthSourceEdit_->text().trimmed() : QString();
    const QString outputPath = outputDirEdit_ ? outputDirEdit_->text().trimmed() : QString();
    const QString backendPath = exeEdit_ ? exeEdit_->text().trimmed() : QString();

    const QFileInfo workbookInfo(workbookPath);
    const QFileInfo boardInfo(boardPath);
    const QFileInfo outputInfo(outputPath);
    const QFileInfo backendInfo(backendPath);
    const int boardWorkbookCount = boardInfo.exists() && boardInfo.isDir() ? countExcelFiles(boardPath) : 0;

    const bool workbookOk = !workbookPath.isEmpty() && workbookInfo.exists() && workbookInfo.isFile();
    const bool boardOk = !boardPath.isEmpty() && boardInfo.exists() && boardInfo.isDir() && boardWorkbookCount > 0;
    const bool outputOk = !outputPath.isEmpty() &&
        (QDir(outputPath).exists() || QDir(outputInfo.absolutePath()).exists());
    const bool backendOk = !backendPath.isEmpty() && backendInfo.exists() && backendInfo.isFile();
    const int readyCount = (workbookOk ? 1 : 0) + (boardOk ? 1 : 0) + (outputOk ? 1 : 0) + (backendOk ? 1 : 0);
    const bool ready = readyCount == 4;

    setGenerationCheck(
        checkWorkbookIconLabel_,
        checkWorkbookTextLabel_,
        workbookOk,
        "外观病害 Excel 已选择",
        workbookPath.isEmpty() ? "请选择外观病害 Excel" : "外观病害 Excel 不可访问");
    setGenerationCheck(
        checkBoardSourceIconLabel_,
        checkBoardSourceTextLabel_,
        boardOk,
        QString("板长数据源可读取 · %1 个表").arg(boardWorkbookCount),
        boardPath.isEmpty() ? "请选择板长数据源目录" : "板长数据源缺失或无 Excel");
    setGenerationCheck(
        checkOutputIconLabel_,
        checkOutputTextLabel_,
        outputOk,
        "输出目录可写入",
        outputPath.isEmpty() ? "请选择输出目录" : "输出目录父级不可访问");
    setGenerationCheck(
        checkBackendIconLabel_,
        checkBackendTextLabel_,
        backendOk,
        "后端生成程序可用",
        "后端 draw_cad.exe 不可访问");

    if (!running) {
        setGenerationStatus(ready ? "可生成" : QString("待检查 %1/4").arg(readyCount), ready ? "ready" : "pending");
        if (startButton_) {
            startButton_->setEnabled(ready);
        }
    }
}

void AutoCadGui::syncGenerationControlHeight() {
    auto* leftColumn = findChild<QWidget*>("LeftWorkflowColumn");
    auto* generationCard = findChild<QWidget*>("GenerationControlCard");
    if (!leftColumn || !generationCard) {
        return;
    }

    if (leftColumn->layout()) {
        leftColumn->layout()->activate();
    }
    if (generationCard->layout()) {
        generationCard->layout()->activate();
    }

    generationCard->setFixedHeight(leftColumn->sizeHint().height());
}

void AutoCadGui::setGenerationInfoMode(bool active) {
    generationInfoMode_ = active;
    if (generationTitleLabel_) {
        generationTitleLabel_->setText(active ? "生成信息" : "生成控制");
    }
    for (QWidget* widget : generationPreflightWidgets_) {
        if (widget) {
            widget->setVisible(!active);
        }
    }
    if (startButton_) {
        startButton_->setVisible(!active);
    }
    if (!active) {
        if (statusLabel_) {
            statusLabel_->hide();
        }
        if (progressBar_) {
            progressBar_->hide();
        }
        if (outputSummaryLabel_) {
            outputSummaryLabel_->hide();
        }
        if (cancelButton_) {
            cancelButton_->hide();
        }
        if (openOutputButton_) {
            openOutputButton_->hide();
        }
    }
}

void AutoCadGui::setGenerationOutputSummary(const QString& text, const QString& state) {
    if (!outputSummaryLabel_) {
        return;
    }
    outputSummaryLabel_->setPlainText(text);
    outputSummaryLabel_->setProperty("state", state);
    polish(outputSummaryLabel_);
    outputSummaryLabel_->show();
    outputSummaryLabel_->moveCursor(QTextCursor::End);
}

void AutoCadGui::appendGenerationIssueMessage(const QString& text, const QString& state) {
    const QString message = text.trimmed();
    if (message.isEmpty()) {
        return;
    }

    generationIssueMessages_.append(message);
    const QString currentState = outputSummaryLabel_->property("state").toString();
    const QString displayState = state == "error" || currentState == "error" ? "error" : "warning";
    setGenerationOutputSummary(generationIssueMessages_.join("\n\n"), displayState);
}

void AutoCadGui::appendGenerationCompletionMessage(bool ok, const QString& summary) {
    const QString completion = summary.trimmed().isEmpty()
        ? (ok ? "已完成生成任务。" : "生成失败，请查看运行日志。")
        : summary.trimmed();

    if (generationIssueMessages_.isEmpty()) {
        setGenerationOutputSummary(completion, ok ? "done" : "error");
        return;
    }

    generationIssueMessages_.append(completion);
    const QString currentState = outputSummaryLabel_ ? outputSummaryLabel_->property("state").toString() : QString();
    const QString displayState = !ok || currentState == "error" ? "error" : "warning";
    setGenerationOutputSummary(generationIssueMessages_.join("\n\n"), displayState);
}

void AutoCadGui::setGenerationStatus(const QString& text, const QString& state) {
    if (!generationStatusBadge_) {
        return;
    }
    generationStatusBadge_->setText(text);
    generationStatusBadge_->setProperty("state", state);
    polish(generationStatusBadge_);
}

void AutoCadGui::setGenerationCheck(
    QLabel* iconLabel,
    QLabel* textLabel,
    bool ok,
    const QString& okText,
    const QString& failText) {
    if (iconLabel) {
        iconLabel->setText(ok ? "✓" : "!");
        iconLabel->setProperty("state", ok ? "ok" : "warning");
        polish(iconLabel);
    }
    if (textLabel) {
        textLabel->setText(ok ? okText : failText);
        textLabel->setProperty("state", ok ? "ok" : "warning");
        polish(textLabel);
    }
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
        if (pageTitleIconLabel_) {
            pageTitleIconLabel_->setText("≡");
            pageTitleIconLabel_->setProperty("tone", "cad");
            polish(pageTitleIconLabel_);
        }
        pageTitleLabel_->setText("批量生成衬砌平面图");
        pageSubtitleLabel_->setText("上传外观病害总表，自动匹配板长基础数据，批量生成各个数据表的 DXF 图纸。");
    }
    else if (index == 1) {
        if (pageTitleIconLabel_) {
            pageTitleIconLabel_->setText("▤");
            pageTitleIconLabel_->setProperty("tone", "word");
            polish(pageTitleIconLabel_);
        }
        pageTitleLabel_->setText("标书 Word 框架生成");
        pageSubtitleLabel_->setText("上传招标文件，自动解析章节、评分项与格式要求，生成可编辑的 Word 标书框架。");
    }
    else {
        if (pageTitleIconLabel_) {
            pageTitleIconLabel_->setText("✦");
            pageTitleIconLabel_->setProperty("tone", "ai");
            polish(pageTitleIconLabel_);
        }
        pageTitleLabel_->setText("AI 设置与提示词管理");
        pageSubtitleLabel_->setText("配置 AI API Key、模型参数和自定义提示词，生成内容可应用到标书 Word 框架中的章节。");
    }
}
