param(
    [string]$SourcePath = "auto_cad_gui/AutoCadGui.cpp"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $SourcePath)) {
    throw "Source file not found: $SourcePath"
}

$source = Get-Content -LiteralPath $SourcePath -Raw -Encoding UTF8

$requiredTokens = @(
    "BoardLengthDataSourceCard",
    "BoardLengthSourceName",
    "BoardLengthSourceMeta",
    "ChangeDataSourceButton",
    "DefaultSourceButton",
    "refreshBoardLengthSourceStatus",
    "chooseBoardLengthSource",
    "resetBoardLengthSource"
)

foreach ($token in $requiredTokens) {
    if (-not $source.Contains($token)) {
        throw "Missing expected CAD data source UI token: $token"
    }
}

Write-Host "CAD data source UI static checks passed."
