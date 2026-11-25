# Sonora 实现总结与验证报告 (Walkthrough)

**Sonora**（面向 Windows 10/11 的现代极简磨砂、极致低内存本地音乐播放器）已按批准的 [implementation_plan.md](./implementation_plan.md) 完整实现 **Phase 1 → Phase 2 → Phase 3** 全部模块，工程位于 `/home/antigravity/sonora`。

---

## 一、已完成的核心模块与文件

### 1. 构建系统与轻量化依赖裁剪
- [CMakeLists.txt](../CMakeLists.txt)、[CMakePresets.json](../CMakePresets.json)、[FindFFmpeg.cmake](../cmake/FindFFmpeg.cmake)、[build_windows.ps1](../scripts/build_windows.ps1)
- 采用 `Qt 6.9.1` 现代 CMake 范式（`qt_standard_project_setup(REQUIRES 6.9)` + `qt_add_qml_module` + `qmlcachegen` AOT 编译 + `qt_add_translations`）。
- 使用 `QGuiApplication`，完全剔除 `Qt6::Widgets`、`Qt6::Multimedia`、`Qt6::Sql`、`Qt6::Svg`。
- 静态集成裁剪后的 [miniaudio_impl.c](../third_party/miniaudio/miniaudio_impl.c)（宏禁用未使用的 `engine/decoding/encoding/node_graph/resource_manager`）与开启 FTS5 的 [sqlite3.c](../third_party/sqlite3/sqlite3.c)。

### 2. 高音质流式音频引擎与发烧 DSP (`src/audio/`)
- **无锁并发环形缓冲**：[AudioRingBuffer.h](../src/audio/AudioRingBuffer.h)（`AudioRingBuffer` SPSC 缓存行对齐 + `LockFreeMarkerQueue` 采样级曲目边界队列）。
- **FFmpeg 8.1 流式解码器**：[FFmpegDecoder.h](../src/audio/FFmpegDecoder.h) / [FFmpegDecoder.cpp](../src/audio/FFmpegDecoder.cpp)
  - 全面采用 FFmpeg 8.1 现代 API（`AVChannelLayout`、`swr_alloc_set_opts2`、`avcodec_send_packet`/`avcodec_receive_frame`、`AV_PKT_DATA_REPLAYGAIN` 侧数据与 R128/标签回退）。
  - 支持 FLAC / ALAC / WAV / AIFF / APE / WavPack / TTA / MP3 / AAC / Vorbis / Opus / WMA，以及 **DSD (`.dsf`/`.dff`) 高精度转 PCM**（64-tap Kaiser 窗 + 三角高通抖动）。
  - 原生支持 `.cue` 虚拟区间 `[startMs, endMs]` 精确裁剪及相邻分轨零重启续播（`advanceContiguousCueWindow`）。
- **`miniaudio` WASAPI 输出**：[MiniAudioOutput.h](../src/audio/MiniAudioOutput.h) / [MiniAudioOutput.cpp](../src/audio/MiniAudioOutput.cpp)
  - 封装低级 `ma_device`，支持 WASAPI 共享与 **WASAPI 独占模式**（硬件格式精度降序协商 `f32 → s32 → s24 → s16` + TPDF 抖动、独占失败自动无感降级共享并提示）。
- **实时 DSP 与 Gapless 调度**：[PlaybackEngine.cpp](../src/audio/PlaybackEngine.cpp)、[BiquadEq.cpp](../src/audio/dsp/BiquadEq.cpp)、[ReplayGainProcessor.h](../src/audio/dsp/ReplayGainProcessor.h)、[FftAnalyzer.cpp](../src/audio/dsp/FftAnalyzer.cpp)
  - 实时回调零锁零堆分配；支持 10 段双精度 Direct Form II Transposed 参数均衡器（无锁双缓冲热切换系数 + 7 套预设）、ReplayGain 曲目/专辑平衡与 `-0.009 dBFS` 柔膝防削波限幅器、Bit-Perfect 旁路、5 秒提前 Gapless 预加载以及 WASAPI 独占自动采样率切换。

### 3. 本地曲库、中文编码纠错、封面缓存与歌词系统
- **中文乱码根治**：[TextEncodingHelper.cpp](../src/core/TextEncodingHelper.cpp) — Windows 下直调 Win32 `MultiByteToWideChar(54936)`、Linux 下直调 glibc `iconv`，自动识别 UTF-8 / UTF-16 BOM / GB18030 / Shift-JIS / Big5，并智能修复 FFmpeg 按 Latin-1 读取老式 GBK ID3 标签导致的乱码，同时保留法/冰岛语等拉丁重音字符。
- **SQLite3 WAL + FTS5 曲库**：[LibraryDatabase.cpp](../src/library/LibraryDatabase.cpp) — 硬限制 `PRAGMA cache_size=-4000`（4 MB 封顶）与 `PRAGMA mmap_size=0`；3 字符及以上使用 FTS5 `trigram` 索引，1–2 字符中文查询（如“晴天”）自动走 `LIKE` 精确子串匹配；支持全部歌曲、专辑、歌手、文件夹、收藏、最近播放及自定义歌单。
- **异步增量扫描与 `.cue` / M3U8**：[LibraryScanner.cpp](../src/library/LibraryScanner.cpp)、[CueParser.cpp](../src/library/CueParser.cpp)、[M3uPlaylistIO.cpp](../src/library/M3uPlaylistIO.cpp) — 依据 `mtime + size` 跳过未变更文件，自动折叠 `.cue` 指向的整轨大文件（含 `.wav`→`.flac` 同名文件救援）并按目录共享封面哈希。
- **8 MB RAM LRU 封面缓存**：[CoverArtProvider.cpp](../src/library/CoverArtProvider.cpp) — 异步 `QQuickAsyncImageProvider`，内嵌封面优先于目录 `cover.jpg/folder.png`，SHA-1 去重生成 420px JPEG 磁盘缓存并严格执行 8 MB RAM LRU 上限。
- **双语/增强歌词引擎**：[LyricsParser.cpp](../src/lyrics/LyricsParser.cpp) / [LyricsController.cpp](../src/lyrics/LyricsController.cpp) — 自动读取同名 `.lrc` 或内嵌歌词，支持多时间戳展开、同时间戳双语翻译合并、增强型逐字标签解析、`[offset]` 与手动偏移微调、O(log N) 二分同步。

### 4. Apple Music 现代极简磨砂 QML UI 与 Windows 10/11 原生集成
- **自定义单 Draw Call GPU 组件**：[SpectrumVisualizerItem.cpp](../src/ui/SpectrumVisualizerItem.cpp)（48 柱实时频谱 + 重力峰值帽，单次 `QSGGeometryNode` Draw Call，隐藏/暂停时自动停表零 CPU 占用）与 [EqCurveItem.cpp](../src/ui/EqCurveItem.cpp)（实时 10 段频率响应曲线）。
- **QML 界面体系**：[Main.qml](../qml/Main.qml)、[DesignTokens.qml](../qml/theme/DesignTokens.qml)、[IconGlyph.qml](../qml/components/IconGlyph.qml)（Qt 6.9 `Shape.CurveRenderer` 矢量图标）、[AmbientBackdrop.qml](../qml/components/AmbientBackdrop.qml)、[TitleBar.qml](../qml/components/TitleBar.qml)、[Sidebar.qml](../qml/components/Sidebar.qml)、[PlaybackBar.qml](../qml/components/PlaybackBar.qml)、[QueueDrawer.qml](../qml/components/QueueDrawer.qml)、[TracksView.qml](../qml/views/TracksView.qml)、[AlbumsView.qml](../qml/views/AlbumsView.qml)、[ArtistsView.qml](../qml/views/ArtistsView.qml)、[FoldersView.qml](../qml/views/FoldersView.qml)、[NowPlayingOverlay.qml](../qml/views/NowPlayingOverlay.qml)、[EqDrawer.qml](../qml/views/EqDrawer.qml)、[SettingsView.qml](../qml/views/SettingsView.qml)、[MiniPlayerWindow.qml](../qml/windows/MiniPlayerWindow.qml)、[TrackInfoDialog.qml](../qml/components/TrackInfoDialog.qml)，所有重型视图与弹窗均经 `Loader` 延迟加载。
- **Windows 10/11 原生平台层**：
  - [Win32WindowHelper.cpp](../src/platform/win32/Win32WindowHelper.cpp)（无边框 `WM_NCCALCSIZE` / `WM_NCHITTEST` 保留 Aero Snap 与阴影 + Win11 原生 DWM Mica / Acrylic / 圆角 / 暗黑标题栏）
  - [Win32SmtcController.cpp](../src/platform/win32/Win32SmtcController.cpp)（纯 Windows SDK WRL/COM 互操作接入锁屏与媒体键，仅链接 `runtimeobject.lib`）
  - [Win32TrayIcon.cpp](../src/platform/win32/Win32TrayIcon.cpp)（纯 `Shell_NotifyIconW` 托盘，零 `Qt6::Widgets` 依赖）
  - [Win32GlobalHotkey.cpp](../src/platform/win32/Win32GlobalHotkey.cpp)（`Ctrl+Alt+Space/方向键/M` 全局快捷键）
- **中/英双语**：[sonora_zh_CN.ts](../translations/sonora_zh_CN.ts) / [sonora_en_US.ts](../translations/sonora_en_US.ts)，支持设置页热切换。

---

## 二、自动化验证结果（Qt 6.9.1 + FFmpeg 8.1.3）

| 验证项 | 命令 / 配置 | 结果 |
|---|---|---|
| RelWithDebInfo 全量编译 | `cmake --build --preset linux-dev-test -j2` | **通过**（零错误、零编译警告） |
| 单元测试套件（5 套） | `ctest --preset linux-dev-test --output-on-failure` | **5/5 通过**（耗时 0.83 s） |
| 内存安全与 UB 检测 | `cmake --preset linux-asan` + `ASAN_OPTIONS=detect_leaks=0 ctest --preset linux-asan` | **5/5 通过**（零越界、零 UAF、零未定义行为） |
| QML 静态检查 | `qmllint` 扫描全部 17 个 QML 文件 | **0 错误、0 `Quick.layout-positioning` 布局警告** |
| 全功能无头烟雾测试与内存采样 | `QT_QPA_PLATFORM=offscreen ./build/linux-dev-test/sonora --smoke-test-exit` | **零运行时警告，实测 RSS = 59.9 MB / Private = 17.5 MB** |
| Windows x64 交叉编译与便携打包 | `cmake --preset win64-cross-release` + `scripts/package_win64_cross.py` | **PE32+ x86-64 `Sonora.exe` (4.0 MB)，完整便携目录 55.14 MB（零 `Qt6Widgets.dll`、零缺失 DLL）** |

### 便携运行包（Windows 10/11 开箱即用）
- 路径：[`release/Sonora-win64/Sonora.exe`](../release/Sonora-win64/Sonora.exe)
- 内含 Qt 6.9.1 (`win64_llvm_mingw`) 精简运行时、FFmpeg 8.1.3 音频精简 4 个 DLL（合计 3.2 MB）、LLVM-MinGW UCRT 运行库、自定义多分辨率图标及 `qt.conf`，经 `x86_64-w64-mingw32-objdump` 递归导入表校验 0 缺失依赖。

### 单元测试覆盖明细
1. [test_ring_buffer.cpp](../tests/test_ring_buffer.cpp)：回绕读写、20 万帧并发 SPSC 无锁完整性、`LockFreeMarkerQueue` 边界序验证。
2. [test_text_encoding.cpp](../tests/test_text_encoding.cpp)：UTF-8 / BOM、GB18030 自动识别、假 Latin-1 GBK ID3 乱码修复、欧洲重音字母防误伤。
3. [test_ffmpeg_decoder.cpp](../tests/test_ffmpeg_decoder.cpp)：真实 FFmpeg 8.1.3 解码 96 kHz Hi-Res FLAC、ReplayGain/内嵌歌词提取、CUE 虚拟区间切片、相邻 CUE 零重启续播、双曲目确定性 Gapless 切换。
4. [test_library_and_parsers.cpp](../tests/test_library_and_parsers.cpp)：SQLite3 FTS5 trigram 英文搜索 + 2 字中文（“晴天”）搜索、专辑/歌手聚合、歌单 CRUD、GB18030 `.cue` 解析与文件名救援、`LibraryScanner` 整轨折叠、双语/增强 `.lrc` 解析、M3U8 读写往返、`CoverArtProvider` 严格 RAM LRU 淘汰上限验证。
5. [test_dsp_and_fft.cpp](../tests/test_dsp_and_fft.cpp)：10 段 `BiquadEq` 在 1 kHz 处 +6 dB / -6 dB 正弦波实测 RMS 精度误差 < 0.25 dB、`ReplayGainProcessor` 防削波限幅与柔膝限制、`FftAnalyzer` 1 kHz 峰值频点与对数频段能量验证。

---

## 三、Windows 10/11 本机构建指南

在安装了 **Visual Studio 2022**、**Qt 6.9.1 (`msvc2022_64`)** 与 **FFmpeg 8.1 shared dev 包**的 Windows 10/11 环境下，打开 *x64 Native Tools Command Prompt / PowerShell* 执行：

```powershell
$env:QT6_DIR = "C:\Qt\6.9.1\msvc2022_64"
$env:FFMPEG_ROOT = "C:\ffmpeg-8.1-full_build-shared"
.\scripts\build_windows.ps1
```

脚本将自动完成配置、MSVC 2022 + Ninja 编译、运行全部 5 套单元测试，并通过 `windeployqt` 与 FFmpeg DLL 拷贝在 `dist\Sonora\` 下生成可直接运行的便携版 `Sonora.exe`。完整说明见 [README.md](../README.md)。
