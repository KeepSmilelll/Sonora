# Sonora Windows 10/11 Build & Portable Packaging Script (MSVC 2022 + Qt 6.9.1 + FFmpeg 8.1)
param(
    [string]$Qt6Dir = $env:QT6_DIR,
    [string]$FFmpegRoot = $env:FFMPEG_ROOT,
    [string]$Preset = "windows-msvc-release",
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"

if (-not $Qt6Dir) {
    Write-Error "Please set QT6_DIR (e.g. C:\Qt\6.9.1\msvc2022_64) or pass -Qt6Dir."
}
if (-not $FFmpegRoot) {
    Write-Error "Please set FFMPEG_ROOT (e.g. C:\ffmpeg-8.1-full_build-shared) or pass -FFmpegRoot."
}

$env:QT6_DIR = $Qt6Dir
$env:FFMPEG_ROOT = $FFmpegRoot

Write-Host "==> Configuring Sonora ($Preset)..." -ForegroundColor Cyan
cmake --preset $Preset

Write-Host "==> Building Sonora..." -ForegroundColor Cyan
cmake --build --preset $Preset --parallel

if (-not $SkipTests) {
    Write-Host "==> Running Automated Tests..." -ForegroundColor Cyan
    $env:PATH = "$Qt6Dir\bin;$FFmpegRoot\bin;$env:PATH"
    ctest --preset $Preset --output-on-failure
}

$BuildDir = Join-Path $PSScriptRoot "..\build\$Preset"
$DistDir = Join-Path $PSScriptRoot "..\dist\Sonora"
New-Item -ItemType Directory -Force -Path $DistDir | Out-Null

Copy-Item (Join-Path $BuildDir "Sonora.exe") (Join-Path $DistDir "Sonora.exe") -Force
Copy-Item (Join-Path $FFmpegRoot "bin\avcodec*.dll") $DistDir -Force
Copy-Item (Join-Path $FFmpegRoot "bin\avformat*.dll") $DistDir -Force
Copy-Item (Join-Path $FFmpegRoot "bin\avutil*.dll") $DistDir -Force
Copy-Item (Join-Path $FFmpegRoot "bin\swresample*.dll") $DistDir -Force

$WinDeployQt = Join-Path $Qt6Dir "bin\windeployqt.exe"
Write-Host "==> Running windeployqt..." -ForegroundColor Cyan
& $WinDeployQt --release --no-translations --no-opengl-sw --qmldir (Join-Path $PSScriptRoot "..\qml") (Join-Path $DistDir "Sonora.exe")

Write-Host "==> Portable build ready at: $DistDir" -ForegroundColor Green
