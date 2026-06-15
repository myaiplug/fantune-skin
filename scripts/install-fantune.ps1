#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Installs FanTune VST3, CLAP, Standalone, and factory presets to the
    correct system locations on Windows.
.DESCRIPTION
    Copies built binaries from the default build output directory to:
      - $env:ProgramFiles\Common Files\VST3\
      - $env:ProgramFiles\Common Files\CLAP\
      - $env:ProgramFiles\FanTune\ (Standalone)
      - $env:USERPROFILE\Documents\FanTune\Presets\ (Factory presets)
.PARAMETER BuildDir
    Path to the build artefacts (Release). Default: ..\build\FanTune_artefacts\Release
.PARAMETER PresetDir
    Path to the factory presets. Default: ..\presets
.PARAMETER SkipStandalone
    Skip installing the Standalone executable.
.EXAMPLE
    .\install-fantune.ps1
    .\install-fantune.ps1 -BuildDir "D:\builds\fan\Release"
#>
param(
    [string]$BuildDir   = "$PSScriptRoot\..\build\FanTune_artefacts\Release",
    [string]$PresetDir  = "$PSScriptRoot\..\presets",
    [switch]$SkipStandalone
)

$ErrorActionPreference = "Stop"

# ── Paths ────────────────────────────────────────────────────────────────────
$vst3Src   = Join-Path $BuildDir "VST3\FanTune.vst3"
$clapSrc   = Join-Path $BuildDir "CLAP\FanTune.clap"
$exeSrc    = Join-Path $BuildDir "Standalone\FanTune.exe"

$vst3Dst   = Join-Path $env:ProgramFiles "Common Files\VST3"
$clapDst   = Join-Path $env:ProgramFiles "Common Files\CLAP"
$exeDst    = "$env:ProgramFiles\FanTune"
$presetDst = Join-Path $env:USERPROFILE "Documents\FanTune\Presets"

# ── Validate ─────────────────────────────────────────────────────────────────
Write-Host "FanTune Installer [Windows]" -ForegroundColor Cyan
Write-Host "  Build artefacts: $BuildDir" -ForegroundColor DarkGray

if (-not (Test-Path $vst3Src)) { throw "Missing VST3: $vst3Src" }
if (-not (Test-Path $clapSrc)) { throw "Missing CLAP: $clapSrc" }

# ── Create target dirs ───────────────────────────────────────────────────────
New-Item -ItemType Directory -Force -Path $vst3Dst   | Out-Null
New-Item -ItemType Directory -Force -Path $clapDst   | Out-Null
New-Item -ItemType Directory -Force -Path $presetDst | Out-Null

if (-not $SkipStandalone) {
    New-Item -ItemType Directory -Force -Path $exeDst   | Out-Null
}

# ── Copy VST3 ────────────────────────────────────────────────────────────────
$vst3Target = Join-Path $vst3Dst "FanTune.vst3"
if (Test-Path $vst3Target) { Remove-Item -Recurse -Force $vst3Target }
Copy-Item -Recurse -Force $vst3Src $vst3Dst
Write-Host "  VST3 → $vst3Target" -ForegroundColor Green

# ── Copy CLAP ────────────────────────────────────────────────────────────────
$clapTarget = Join-Path $clapDst "FanTune.clap"
Copy-Item -Force $clapSrc $clapTarget
Write-Host "  CLAP → $clapTarget" -ForegroundColor Green

# ── Copy Standalone ──────────────────────────────────────────────────────────
if (-not $SkipStandalone) {
    if (Test-Path $exeSrc) {
        $exeTarget = Join-Path $exeDst "FanTune.exe"
        Copy-Item -Force $exeSrc $exeTarget
        Write-Host "  APP  → $exeTarget" -ForegroundColor Green
    } else {
        Write-Host "  Standalone not built (optional, skipping)" -ForegroundColor Yellow
    }
}

# ── Copy factory presets ─────────────────────────────────────────────────────
if (Test-Path $PresetDir) {
    $count = 0
    Get-ChildItem "$PresetDir\*.fantune" | ForEach-Object {
        Copy-Item -Force $_.FullName $presetDst
        $count++
    }
    Write-Host "  Presets ($count) → $presetDst" -ForegroundColor Green
} else {
    Write-Host "  Presets dir not found: $PresetDir (run generate_factory_presets.py first)" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "FanTune installed. Restart your DAW to scan the new plugin." -ForegroundColor Cyan
