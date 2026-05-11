param(
    [string]$ExePath = "x64/Debug/draw_cad.exe"
)

$ErrorActionPreference = "Stop"

function Write-Utf8NoBomFile {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Content
    )

    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    $parent = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    [System.IO.File]::WriteAllText($Path, $Content, $utf8NoBom)
}

function ConvertTo-Double {
    param([Parameter(Mandatory = $true)][string]$Text)

    return [double]::Parse($Text, [System.Globalization.CultureInfo]::InvariantCulture)
}

function Get-DxfLineEntities {
    param([Parameter(Mandatory = $true)][string]$Path)

    $rawLines = Get-Content -LiteralPath $Path -Encoding UTF8
    $pairs = New-Object System.Collections.Generic.List[object]
    for ($i = 0; $i + 1 -lt $rawLines.Count; $i += 2) {
        $pairs.Add([pscustomobject]@{
            Code = $rawLines[$i].Trim()
            Value = $rawLines[$i + 1].Trim()
        })
    }

    $entities = New-Object System.Collections.Generic.List[object]
    for ($i = 0; $i -lt $pairs.Count; ++$i) {
        if ($pairs[$i].Code -ne "0" -or $pairs[$i].Value -ne "LINE") {
            continue
        }

        $entity = @{
            Layer = ""
            X1 = $null
            Y1 = $null
            X2 = $null
            Y2 = $null
        }

        for ($j = $i + 1; $j -lt $pairs.Count; ++$j) {
            if ($pairs[$j].Code -eq "0") {
                break
            }

            switch ($pairs[$j].Code) {
                "8" { $entity.Layer = $pairs[$j].Value }
                "10" { $entity.X1 = ConvertTo-Double $pairs[$j].Value }
                "20" { $entity.Y1 = ConvertTo-Double $pairs[$j].Value }
                "11" { $entity.X2 = ConvertTo-Double $pairs[$j].Value }
                "21" { $entity.Y2 = ConvertTo-Double $pairs[$j].Value }
            }
        }

        if ($null -ne $entity.X1 -and $null -ne $entity.Y1 -and $null -ne $entity.X2 -and $null -ne $entity.Y2) {
            $entities.Add([pscustomobject]$entity)
        }
    }

    return $entities
}

if (-not (Test-Path -LiteralPath $ExePath)) {
    throw "draw_cad executable not found: $ExePath"
}

$workDir = $null
try {
$runId = [guid]::NewGuid().ToString("N")
$workDir = Join-Path $PSScriptRoot "_tmp_longitudinal_cross_slab_$runId"
New-Item -ItemType Directory -Path $workDir -Force | Out-Null

$diseaseJson = Join-Path $workDir "disease_report.json"
$boardJson = Join-Path $workDir "board_lengths.json"
$symbolsJson = Join-Path $workDir "symbols.json"
$outputDxf = Join-Path $workDir "cross_slab.dxf"

$diseaseContent = @'
{
  "source": {
    "file": "cross-slab-test.xlsx",
    "sheet": "unit",
    "header_row": 1
  },
  "summary": {
    "exported_count": 1
  },
  "records": [
    {
      "source_row": 2,
      "slab_no": "022",
      "project_name": "\u886c\u780c",
      "check_item": "\u7eb5\u5411\u88c2\u7f1d",
      "defect_location": "\u53f3\u62f1\u8170",
      "defect_desc": "cross slab longitudinal crack",
      "judgement": "",
      "judgement_is_zero": false,
      "parsed": {
        "distance_from_slab_end": "6m",
        "length": "5m",
        "width": "0.2mm",
        "area": ""
      }
    }
  ]
}
'@

$boardContent = @'
{
  "source": {
    "file": "board-length-test.xlsx",
    "sheet": "unit",
    "header_row": 2
  },
  "summary": {
    "exported_count": 3
  },
  "records": [
    {
      "source_row": 3,
      "group": "A:B",
      "slab_no": "022",
      "length": "8m",
      "length_raw": "8",
      "length_m": 8.0
    },
    {
      "source_row": 4,
      "group": "A:B",
      "slab_no": "023",
      "length": "8m",
      "length_raw": "8",
      "length_m": 8.0
    },
    {
      "source_row": 5,
      "group": "A:B",
      "slab_no": "024",
      "length": "8m",
      "length_raw": "8",
      "length_m": 8.0
    }
  ]
}
'@

$symbolsContent = @'
{
  "meta": {
    "unit": "m",
    "version": 1
  },
  "symbols": [
    {
      "id": "crack_long",
      "block": "TEST_CRACK_LONG",
      "kind": "LongitudinalCrack",
      "anchor": [0, 0],
      "bbox": {
        "min": [0, -0.05],
        "max": [1, 0.05]
      },
      "style": {
        "layer": "0",
        "linetype": "BYLAYER",
        "lineweight_mm": 0.29,
        "mixed": false
      },
      "params": {
        "scale_mode": "stretch_x",
        "base_length_m": 1
      },
      "primitives": [
        {
          "type": "LINE",
          "a": [0, 0],
          "b": [1, 0]
        }
      ]
    }
  ]
}
'@

Write-Utf8NoBomFile -Path $diseaseJson -Content $diseaseContent
Write-Utf8NoBomFile -Path $boardJson -Content $boardContent
Write-Utf8NoBomFile -Path $symbolsJson -Content $symbolsContent

$output = & $ExePath --lining-plan $diseaseJson $boardJson $outputDxf $symbolsJson
if ($LASTEXITCODE -ne 0) {
    throw "draw_cad failed with exit code $LASTEXITCODE`n$output"
}

if (-not (Test-Path -LiteralPath $outputDxf)) {
    throw "Expected DXF output was not created: $outputDxf"
}

$dxfText = Get-Content -LiteralPath $outputDxf -Raw -Encoding UTF8
if (-not $dxfText.Contains("022")) {
    throw "Expected output DXF to contain slab label 022."
}
if (-not $dxfText.Contains("023")) {
    throw "Expected output DXF to contain auto-added slab label 023 for the cross-slab crack."
}

$diseaseLines = @(Get-DxfLineEntities -Path $outputDxf | Where-Object { $_.Layer -eq "DISEASE" })
if ($diseaseLines.Count -ne 1) {
    throw "Expected 1 continuous DISEASE line for the cross-slab crack, found $($diseaseLines.Count)."
}

$length = [math]::Round([math]::Abs($diseaseLines[0].X2 - $diseaseLines[0].X1), 6)
if ($length -ne 5.0) {
    throw "Expected one continuous 5m disease line across slabs 022 and 023, found $length."
}

Write-Host "Longitudinal cross-slab regression check passed."
}
finally {
    if (-not [string]::IsNullOrWhiteSpace($workDir) -and (Test-Path -LiteralPath $workDir)) {
        $resolvedWorkDir = [System.IO.Path]::GetFullPath($workDir)
        $allowedPrefix = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "_tmp_longitudinal_cross_slab_"))
        if (-not $resolvedWorkDir.StartsWith($allowedPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to clean unexpected path: $resolvedWorkDir"
        }

        Remove-Item -LiteralPath $resolvedWorkDir -Recurse -Force
    }
}
