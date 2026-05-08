param(
    [string]$SourcePath = "auto_cad_gui/AutoCadGui.cpp"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $SourcePath)) {
    throw "Source file not found: $SourcePath"
}

$source = Get-Content -LiteralPath $SourcePath -Raw -Encoding UTF8
$headerPath = Join-Path (Split-Path -Parent $SourcePath) "AutoCadGui.h"
if (Test-Path -LiteralPath $headerPath) {
    $source += Get-Content -LiteralPath $headerPath -Raw -Encoding UTF8
}
$stylePath = Join-Path (Split-Path -Parent $SourcePath) "AntDesignStyle.cpp"
if (Test-Path -LiteralPath $stylePath) {
    $source += Get-Content -LiteralPath $stylePath -Raw -Encoding UTF8
}

$requiredTokens = @(
    "BoardLengthDataSourceCard",
    "BoardLengthSourceName",
    "BoardLengthSourceMeta",
    "ChangeDataSourceButton",
    "DefaultSourceButton",
    "AppShell",
    "TopBar",
    "PageTitleIcon",
    "CadDashboard",
    "LeftWorkflowColumn",
    "DataSourcePathBox",
    "MiniCardIcon",
    "TaskFilePanel",
    "uploadBox->setFixedHeight(112)",
    "GenerationControlCard",
    "GenerationStatusBadge",
    "GenerationCompactNote",
    "GenerationCheckRow",
    "generationPreflightWidgets_",
    "setGenerationInfoMode",
    "appendGenerationIssueMessage",
    "appendGenerationCompletionMessage",
    "generationIssueMessages_",
    "generationIssueMessages_.clear()",
    "generationIssueMessages_.append(completion)",
    "generationIssueMessages_.join(""\n\n"")",
    "generationTitleLabel_->setText",
    "startButton_->setVisible(!active)",
    "outputSummaryLabel_->setReadOnly(true)",
    "outputSummaryLabel_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded)",
    "outputSummaryLabel_->setPlainText(text)",
    "outputSummaryLabel_->setProperty(""state"", state)",
    "QTextEdit* outputSummaryLabel_",
    "QTextEdit#OutputSummary[state=""error""]",
    "cadPageLayout->addWidget(cadDashboard)",
    "mainLayout->setContentsMargins(0, 0, 0, 12)",
    "leftColumnLayout->setSpacing(22)",
    "card->setFixedHeight(286)",
    "syncGenerationControlHeight",
    'generationCard->setFixedHeight(leftColumn->sizeHint().height())',
    "updateGenerationPanel",
    "QueuePrimaryButton",
    "AiSettingsCard",
    "AiComboBox",
    "PromptTabActive",
    "PromptEditorBox",
    "AiFlowStep",
    "AiCandidateBox",
    "AntDesignStyle",
    "AntPrimaryButton",
    "#1677ff",
    "OutputPathCard",
    "createOutputDirectoryCard",
    "refreshBoardLengthSourceStatus",
    "chooseBoardLengthSource",
    "resetBoardLengthSource"
)

foreach ($token in $requiredTokens) {
    if (-not $source.Contains($token)) {
        throw "Missing expected CAD data source UI token: $token"
    }
}

$forbiddenTokens = @(
    "GenerationSummaryRow",
    "createGenerationSummaryRow",
    "QScrollArea#ContentScroll",
    "ContentScroll",
    "ScrollBarAlwaysOff",
    "cadScroll"
)

foreach ($token in $forbiddenTokens) {
    if ($source.Contains($token)) {
        throw "Unexpected obsolete generation panel token: $token"
    }
}

Write-Host "CAD data source UI static checks passed."
