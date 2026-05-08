param(
    [string]$SourcePath = "auto_cad_gui/AutoCadGui.cpp"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $SourcePath)) {
    throw "Source file not found: $SourcePath"
}

$source = Get-Content -LiteralPath $SourcePath -Raw -Encoding UTF8
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
