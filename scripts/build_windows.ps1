# Virela Windows 10/11 Build & Portable Packaging Script (MSVC 2022 + Qt 6.9.1 + FFmpeg 9.0.2)
param(
    [string]$Qt6Dir = $env:QT6_DIR,
    [string]$FFmpegRoot = $env:FFMPEG_ROOT,
    [string]$Preset = "windows-msvc-release",
    [switch]$SkipTests,
    [switch]$SyncRelease
)

$ErrorActionPreference = "Stop"

if (-not $Qt6Dir -and (Test-Path "D:\QT\6.9.1\msvc2022_64")) {
    $Qt6Dir = "D:\QT\6.9.1\msvc2022_64"
}
if (-not $FFmpegRoot -and (Test-Path "D:\Setup\ffmpeg-9.0.2-full_build-shared")) {
    $FFmpegRoot = "D:\Setup\ffmpeg-9.0.2-full_build-shared"
}

if (-not $Qt6Dir -or -not (Test-Path $Qt6Dir)) {
    Write-Error "Please set QT6_DIR (e.g. D:\QT\6.9.1\msvc2022_64) or pass -Qt6Dir."
}
if (-not $FFmpegRoot -or -not (Test-Path $FFmpegRoot)) {
    Write-Error "Please set FFMPEG_ROOT (e.g. D:\Setup\ffmpeg-9.0.2-full_build-shared) or pass -FFmpegRoot."
}

# Load Visual Studio 2022 x64 MSVC environment automatically if cl.exe is not in PATH
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $vcvarsCandidates = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    )
    $vcvars = $vcvarsCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $vcvars) {
        Write-Error "MSVC cl.exe not found in PATH and vcvars64.bat could not be located."
    }
    Write-Host "==> Loading MSVC 2022 x64 environment from $vcvars ..." -ForegroundColor Cyan
    $envLines = cmd.exe /c "`"$vcvars`" >nul && set"
    foreach ($line in $envLines) {
        if ($line -match '^([^=]+)=(.*)$') {
            Set-Item -Path "Env:$($matches[1])" -Value $matches[2]
        }
    }
}

$env:QT6_DIR = $Qt6Dir
$env:FFMPEG_ROOT = $FFmpegRoot

Write-Host "==> Configuring Virela ($Preset) with FFmpeg: $FFmpegRoot ..." -ForegroundColor Cyan
cmake --preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "==> Building Virela..." -ForegroundColor Cyan
cmake --build --preset $Preset --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not $SkipTests) {
    Write-Host "==> Running Automated Tests..." -ForegroundColor Cyan
    $env:PATH = "$FFmpegRoot\bin;$Qt6Dir\bin;$env:PATH"
    ctest --preset $Preset --output-on-failure
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildDir = Join-Path $RepoRoot "build\$Preset"
$DistDir = Join-Path $RepoRoot "dist\Virela"
if (Test-Path $DistDir) {
    Remove-Item -Recurse -Force $DistDir
}
New-Item -ItemType Directory -Force -Path $DistDir | Out-Null

Copy-Item (Join-Path $BuildDir "Virela.exe") (Join-Path $DistDir "Virela.exe") -Force
Copy-Item (Join-Path $FFmpegRoot "bin\avcodec*.dll") $DistDir -Force
Copy-Item (Join-Path $FFmpegRoot "bin\avformat*.dll") $DistDir -Force
Copy-Item (Join-Path $FFmpegRoot "bin\avutil*.dll") $DistDir -Force
Copy-Item (Join-Path $FFmpegRoot "bin\swresample*.dll") $DistDir -Force

$WinDeployQt = Join-Path $Qt6Dir "bin\windeployqt.exe"
Write-Host "==> Running windeployqt..." -ForegroundColor Cyan
& $WinDeployQt --release --no-translations --no-opengl-sw --no-system-d3d-compiler --compiler-runtime --qmldir (Join-Path $RepoRoot "qml") (Join-Path $DistDir "Virela.exe")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Ensure qt.conf exists for clean self-contained plugin & QML lookup
Set-Content -Path (Join-Path $DistDir "qt.conf") -Value "[Paths]`nPlugins = .`nQmlImports = qml`n" -Encoding UTF8 -NoNewline

# Remove any unnecessary installer exe if windeployqt placed vc_redist
Get-ChildItem -Path $DistDir -Filter "vc_redist*.exe" -ErrorAction SilentlyContinue | Remove-Item -Force

if (Test-Path (Join-Path $DistDir "Qt6Widgets.dll")) {
    Write-Warning "Qt6Widgets.dll was unexpectedly deployed into $DistDir"
}

Write-Host "==> Portable build ready at: $DistDir" -ForegroundColor Green

if ($SyncRelease) {
    $ReleaseDir = Join-Path $RepoRoot "release\Virela-win64"
    Write-Host "==> Syncing portable build to $ReleaseDir ..." -ForegroundColor Cyan
    if (Test-Path $ReleaseDir) {
        Remove-Item -Recurse -Force $ReleaseDir
    }
    New-Item -ItemType Directory -Force -Path $ReleaseDir | Out-Null
    Copy-Item -Path (Join-Path $DistDir "*") -Destination $ReleaseDir -Recurse -Force
    Write-Host "==> Release portable package updated at: $ReleaseDir" -ForegroundColor Green
}
