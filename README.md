# Sonora

A refined, ultra-low-memory (**40–80 MB** target) local music player for **Windows 10/11**, built with **Qt 6.9.1 (C++20 / QML)**, **FFmpeg 9.0.2**, **`miniaudio`**, and **SQLite3 FTS5**.

## Ready-to-Run Windows x64 Portable Release

A pre-built, self-contained 64-bit Windows portable build (**zero `Qt6Widgets`**, MSVC 2022 + Qt 6.9.1 + FFmpeg 9.0.2 runtime DLLs included, with full native Windows 10/11 SMTC support) is available directly in:

- [`release/Sonora-win64/Sonora.exe`](release/Sonora-win64/Sonora.exe)

Simply download/clone the repository and double-click `release/Sonora-win64/Sonora.exe` on Windows 10/11.

## Documentation

- [Architecture & Implementation Plan](docs/implementation_plan.md)
- [Verification Walkthrough & Memory Report](docs/walkthrough.md)

## Key Highlights

- **Apple Music–Inspired Frosted UI**: Native Windows 11 DWM Mica / Acrylic backdrop (zero offscreen FBO blur overhead), Windows 10 dynamic cover ambient glow fallback, Dark / Light / System themes, and Simplified Chinese / English live switching.
- **Audiophile Audio Engine**: FFmpeg 9.0.2 streaming decoder (FLAC, ALAC, WAV, AIFF, APE, WavPack, TTA, MP3, AAC, Vorbis, Opus, WMA, and DSD `.dsf`/`.dff` to high-precision PCM), lock-free SPSC ring buffer, `miniaudio` WASAPI Shared & **WASAPI Exclusive (Bit-Perfect)** output with automatic hardware sample-rate switching, Gapless playback, ReplayGain, and 10-band double-precision parametric EQ.
- **Local Library & Chinese Music Ecosystem**: SQLite3 WAL + FTS5 trigram search (with 4 MB page cache hard cap), `.cue` whole-disc sheet parsing with contiguous zero-reopen gapless handover, GB18030/GBK/Shift-JIS metadata & `.lrc` auto-detection, dual-language & Enhanced LRC lyrics, 8 MB RAM LRU cover cache, custom playlists & M3U/M3U8 import/export, and drag-and-drop playback.
- **Windows 10/11 Desktop Integration**: Custom frameless window with full Aero Snap & rounded corners, Windows SMTC lock-screen/media-key controls (native WRL on MSVC 2022; `WM_APPCOMMAND` media keys on MinGW), zero-`Qt6Widgets` native `Shell_NotifyIconW` system tray, global hotkeys, and lazy-loaded Mini Floating Player.

## Building on Windows 10/11 (MSVC 2022)

### Prerequisites
- Visual Studio 2022 (C++20 Desktop Workload) + Ninja + CMake ≥ 3.28
- Qt 6.9.1 (`msvc2022_64`)
- FFmpeg 9.0.2 shared dev build (e.g. gyan.dev `ffmpeg-9.0.2-full_build-shared`)

### One-Command Build & Portable Package

Open **x64 Native Tools Command Prompt for VS 2022** (or any PowerShell window — the script auto-loads VS 2022 `vcvars64.bat`):

```powershell
$env:QT6_DIR = "D:\QT\6.9.1\msvc2022_64"
$env:FFMPEG_ROOT = "D:\Setup\ffmpeg-9.0.2-full_build-shared"
.\scripts\build_windows.ps1 -SyncRelease
```

Or configure & build directly with CMake Presets:

```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release --parallel
ctest --preset windows-msvc-release --output-on-failure
```

## Building & Running Tests on Linux (Dev / CI)

```bash
cmake --preset linux-dev-test
cmake --build --preset linux-dev-test --parallel
ctest --preset linux-dev-test --output-on-failure
QT_QPA_PLATFORM=offscreen ./build/linux-dev-test/sonora --smoke-test-exit
```
