# Virela

*A refined, fluid-spatial online & local audiophile music player for Windows 10/11 — Qt 6.9.1 (C++20 / QML + D3D11 QRhi), FFmpeg 9.0.2, `miniaudio`, SQLite3 FTS5.*

![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11%20x64-0078D4?style=flat-square)
![Stack](https://img.shields.io/badge/Qt%206.9.1-C%2B%2B20%20%2F%20QML%20D3D11-41CD52?style=flat-square)
![Audio](https://img.shields.io/badge/FFmpeg%209.0.2-WASAPI%20Exclusive%20Bit--Perfect-E67E22?style=flat-square)
![Footprint](https://img.shields.io/badge/Zero%20Qt6Widgets-Lightweight%20Native-6C5CE7?style=flat-square)

---

## 开箱即用（Windows x64 便携版）

仓库内已集成自包含 64 位 Windows 便携运行包（MSVC 2022 + Qt 6.9.1 + FFmpeg 9.0.2 运行时 DLL，**零 `Qt6Widgets.dll` 依赖**，完整支持 Windows 10/11 原生 SMTC 媒体控制）：

- 运行入口：[`release/Virela-win64/Virela.exe`](release/Virela-win64/Virela.exe)

可直接克隆仓库或下载 Release 后双击 `release/Virela-win64/Virela.exe` 启动：

```bash
git clone https://github.com/KeepSmilelll/Virela.git
```

---

## 核心特性

### 1. 流体空间界面（Fluid Spatial UI）
- **在线为主、本地融合的首屏布局**：启动默认进入 **发现首页（`OnlineMusicView`）** 并默认折叠右侧舞台，集成热门搜索词、精选推荐歌单、新歌速递与本地/在线混合「继续收听」卡片；未配置在线源时自动降级展示本地专辑墙与引导卡片；全局顶部 `Ctrl+K` 命令搜索浮窗（`SearchSuggestionsPopover`）实时联动本地 FTS5 检索与网易云歌单链接直达识别。
- **紧凑导航与 4 标签设置中心**：精简侧边栏（`NavigationRail`）支持自建歌单与 **固定在线歌单（Pinned Online Playlists）** 一键直达；4 标签分页设置中心（播放与输出 / 曲库与在线音源 / 外观与歌词 / 快捷键与语言）支持即时切换与状态反馈。
- **GPU 流体极光与矢量图标**：内置自定义 GPU 流体极光着色器（`FluidAuroraItem`），自动提取当前专辑封面主/次色调实现柔和氛围流动；支持 **Windows 11 原生 DWM Mica / Acrylic 磨砂**、轻量封面氛围渐变及极简纯色；全应用统一使用高 DPI `IconImageProvider` 矢量渲染图标。
- **连续三态空间舞台与 Hero 转场**：支持「折叠 / 右侧栏 / 全窗口沉浸页」无缝切换，搭配专辑封面到旋转黑胶唱片的 Hero 飞入缩放动画；沉浸页左栏集成无边框旋转黑胶、实时 GPU 极光频谱与完整播控，右栏歌词流贯通至窗口底部；按需加载悬停变形 `MiniPlayerWindow`。

### 2. 在线流媒体、离线下载与跨源统一
- **双音源流式播放与自动回退**：支持网易云 API 与 Meting API 双源配置、标准 / 极高 / 无损（Lossless）/ Hi-Res 音质选择、直链优先级与自动回退机制；内置 `LocalBackendManager` 可托管本地 Node 后端进程并随主程序安全启停。
- **分段流式缓存与下载中心**：通过 `StreamCacheManager` 进行可配置上限的分段边缘缓存与缓冲进度指示；`DownloadsView` 管理下载队列，完成后由 `AudioMetadataWriter` 自动写入曲目标签、高清封面与歌词并入库。
- **跨来源数据层统一（阶段三）**：本地查询与在线影子记录通过 `source` 严格隔离、互不污染本地曲库计数；「我喜欢的音乐」「最近播放」与自建歌单原生支持本地与在线曲目混排，且播控栏/舞台统一开放红心收藏；在线曲目下载完成后调用 `upgradeOnlineTrackToLocal()` 原位升级为本地曲目并完整保留播放次数、收藏状态与歌单关联。

### 3. 发烧级音频引擎
- **全格式流式解码**：基于 **FFmpeg 9.0.2** 现代流式解码，支持 FLAC、ALAC、WAV、AIFF、APE、WavPack、TTA、MP3、AAC、Vorbis、Opus、WMA，以及 **DSD (`.dsf` / `.dff`)** 高精度转 PCM（支持 `88.2 kHz` / `176.4 kHz` / `352.8 kHz`）。
- **低抖动零锁输出管线**：无锁 SPSC 环形缓冲 + `miniaudio` WASAPI 共享模式与 **WASAPI 独占模式（Bit-Perfect 旁路）**，支持硬件原生采样率自动切换、0 ms Gapless 无缝衔接。
- **专业调音与响度平衡**：内置 10 段双精度 Direct Form II 参数均衡器（EQ，含 7 套预设及实时频响曲线）与 ReplayGain 响度平衡（关闭 / 按曲目 / 按专辑）。

### 4. 区分模式的精准歌词引擎
- **逐字卡拉 OK 与逐行歌词自动区分**：
  - 逐字歌词（网易云 `.yrc`、QQ 音乐 `.qrc`、KRC 风格及增强型 LRC `<mm:ss.xx>`）采用 **60fps 本地音频时钟插值** 实现零延迟逐字推进扫光，自动为长间奏前的未闭合尾词限制合理时长上限。
  - 普通逐行 `.lrc` 呈现柔和整行聚焦高亮，支持在线曲目自动拉取云端歌词及双语翻译。
- **生态与编码全兼容**：支持外挂优先级 `.yrc` → `.qrc` → `.lrc` → 内嵌 `USLT / LYRICS`，支持双语翻译配对，自动识别并纠错 UTF-8 / UTF-16 / GB18030 / Shift-JIS / Big5 编码。

### 5. 本地曲库中心与低内存架构
- **高速本地检索与本地中心（`LocalMusicHubView`）**：嵌入式 **SQLite3 WAL + FTS5 trigram** 模糊检索（4 MB 页缓存硬上限），中英文任意子串毫秒级响应；统一整合全部曲目 / 专辑 / 歌手 / 文件夹视图与 `.cue` 整轨分轨解析、M3U/M3U8 导入导出、文件与文件夹拖拽即播。
- **全链路按需加载与内存治理**：全视图与 `AppDialogs` / `FocusStage` / `EqDrawer` / `SettingsView` / `MiniPlayerWindow` 全部采用基于 URL 的动态 `Loader` 并在失活时卸载；`OnlineMusicService` 网络客户端与 `StreamCacheManager` 工作线程惰性初始化；配合 `CoverArtProvider`（8 MB RAM LRU）、`QPixmapCache`（4 MB 上限）与切页 `trimTransientMemory()` 回收，实测完整启动常驻内存维持在 **~132 MB RSS**。

### 6. Windows 10/11 原生系统集成
- 无边框原生窗口（完整保留 Aero Snap 分屏、最大化防裁剪与窗口圆角）。
- 单实例守护与二次启动文件路径转发。
- Windows SMTC 锁屏 / 通知中心媒体卡片、物理媒体键响应、原生 `Shell_NotifyIconW` 系统托盘、全局媒体快捷键（`Ctrl+Alt+Space / 方向键 / M`）。

---

## 音频引擎推荐设置速查

在 **设置 → 音频引擎与 WASAPI 输出** 中可按使用场景快速选择：

| 设置项 | 专心听歌 / 外接 DAC 或有线耳机 | 日常办公多任务 / 蓝牙耳机 |
| :--- | :--- | :--- |
| **音频输出设备** | 手动选中你的 DAC / 有线耳机 | 常用设备或默认 |
| **WASAPI 独占模式** | **开启**（绕过系统混音；播放时其他软件静音） | **关闭**（不抢占浏览器/通知提示音） |
| **自动切换硬件原生采样率** | **开启** | 自动灰置 |
| **Bit-Perfect 旁路** | **开启**（EQ/RG 关闭且软件音量 100% 时源码直通） | **开启**（调音量或开 EQ 时自动让位） |
| **DSD 转 PCM 目标采样率** | **`176.4 kHz`**（高端 384k DAC 可选 `352.8 kHz`） | **`88.2 kHz`** 或 **`176.4 kHz`** |
| **ReplayGain 响度平衡** | **关闭**（保持源码直出） | **关闭**（若曲目已写入 RG 标签，可按需选按曲目/按专辑） |

---

## 构建与测试

### Windows 10/11 (MSVC 2022)
**环境依赖**：Visual Studio 2022（C++20 桌面开发）、CMake ≥ 3.28、Ninja、Qt 6.9.1 (`msvc2022_64`)、FFmpeg 9.0.2 shared dev 包（如 `ffmpeg-9.0.2-full_build-shared`）。

一键编译、运行全量单元测试并同步更新便携发行包：

```powershell
$env:QT6_DIR = "D:\QT\6.9.1\msvc2022_64"
$env:FFMPEG_ROOT = "D:\Setup\ffmpeg-9.0.2-full_build-shared"
.\scripts\build_windows.ps1 -SyncRelease
```

或使用 CMake Presets 手动构建、校验 QML 与执行烟雾测试：

```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release --parallel
ctest --preset windows-msvc-release --output-on-failure
cmake --build --preset windows-msvc-release --target virela_qmllint
.\release\Virela-win64\Virela.exe --smoke-test-exit
```

### Linux (开发 / CI 测试)

```bash
cmake --preset linux-dev-test
cmake --build --preset linux-dev-test --parallel
ctest --preset linux-dev-test --output-on-failure
QT_QPA_PLATFORM=offscreen ./build/linux-dev-test/virela --smoke-test-exit
```

---

## 目录结构与技术文档

- `src/` — 核心 C++20 源码（音频引擎、DSP/EQ/FFT、SQLite 跨源曲库、在线流媒体服务与分段缓存、元数据写入、本地后端托管、歌词解析、D3D11 SceneGraph & `IconImageProvider`、Win32 平台集成）
- `qml/` — QML 界面模块 `Virela.UI`（主题令牌、基础控件、发现页 `OnlineMusicView`、本地中心 `LocalMusicHubView`、下载管理 `DownloadsView`、命令搜索浮窗 `SearchSuggestionsPopover`、按需弹窗 `AppDialogs`、沉浸舞台与悬浮迷你窗）
- `translations/` — 简体中文 / 英文国际化翻译源文件（`virela_zh_CN.ts` / `virela_en_US.ts`）
- `tests/` — 6 套自动化单元测试（环形缓冲区、编码纠错、FFmpeg 解码、曲库与跨源数据及歌词解析、DSP/FFT、在线流媒体与缓存管理）
- `scripts/` — Windows MSVC 便携打包脚本与跨平台打包脚本
- `release/Virela-win64/` — 开箱即用的 Windows x64 便携运行目录
- 更多设计细节：[架构与实现计划](docs/implementation_plan.md) · [实现总结与验证报告](docs/walkthrough.md)
