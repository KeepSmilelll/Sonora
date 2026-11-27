# Virela

*A refined, fluid-spatial local audiophile music player for Windows 10/11 — Qt 6.9.1 (C++20 / QML + D3D11 QRhi), FFmpeg 9.0.2, `miniaudio`, SQLite3 FTS5.*

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
- **GPU 流体极光与原生桌面材质**：内置自定义 GPU 流体极光着色器（`FluidAuroraItem`），自动提取当前专辑封面主/次色调实现柔和氛围流动；支持 **Windows 11 原生 DWM Mica / Acrylic 磨砂**、轻量封面氛围渐变及极简纯色，支持深色 / 浅色 / 跟随系统主题与简体中文 / 英文实时热切换。
- **连续三态空间舞台与 Hero 转场**：支持「折叠 / 右侧栏 / 全窗口沉浸页」无缝切换，搭配专辑封面到旋转黑胶唱片的 Hero 飞入缩放动画。
- **沉浸页一体化黑胶控制台**：沉浸视图左栏集成无边框旋转黑胶、实时 GPU 极光频谱与完整播控、进度条、10 段 EQ、技术信息、音量控制；右栏歌词流贯通至窗口底部。
- **悬停变形桌面迷你胶囊窗**：按需懒加载的 `MiniPlayerWindow`，紧凑态实时显示当前单行歌词与迷你频谱，悬停丝滑展开完整操作区。

### 2. 发烧级音频引擎
- **全格式流式解码**：基于 **FFmpeg 9.0.2** 现代流式解码，支持 FLAC、ALAC、WAV、AIFF、APE、WavPack、TTA、MP3、AAC、Vorbis、Opus、WMA，以及 **DSD (`.dsf` / `.dff`)** 高精度转 PCM（支持 `88.2 kHz` / `176.4 kHz` / `352.8 kHz`）。
- **低抖动零锁输出管线**：无锁 SPSC 环形缓冲 + `miniaudio` WASAPI 共享模式与 **WASAPI 独占模式（Bit-Perfect 旁路）**，支持硬件原生采样率自动切换、0 ms Gapless 无缝衔接。
- **专业调音与响度平衡**：内置 10 段双精度 Direct Form II 参数均衡器（EQ，含 7 套预设及实时频响曲线）与 ReplayGain 响度平衡（关闭 / 按曲目 / 按专辑）。

### 3. 区分模式的精准歌词引擎
- **逐字卡拉 OK 与逐行歌词自动区分**：
  - 逐字歌词（网易云 `.yrc`、QQ 音乐 `.qrc`、KRC 风格及增强型 LRC `<mm:ss.xx>`）采用 **60fps 本地音频时钟插值** 实现零延迟逐字推进扫光，自动为长间奏前的未闭合尾词限制合理时长上限。
  - 普通逐行 `.lrc` 呈现柔和整行聚焦高亮，不强行伪造匀速扫光。
- **生态与编码全兼容**：支持外挂优先级 `.yrc` → `.qrc` → `.lrc` → 内嵌 `USLT / LYRICS`，支持双语翻译配对，自动识别并纠错 UTF-8 / UTF-16 / GB18030 / Shift-JIS / Big5 编码。

### 4. 本地曲库与中文音乐生态
- **高速本地检索**：嵌入式 **SQLite3 WAL + FTS5 trigram** 模糊检索（4 MB 页缓存硬上限），中英文任意子串毫秒级响应；歌手/专辑视图按 `年份 DESC · 专辑 · 碟号 · 音轨号` 有序排列。
- **整轨分轨与缓存**：多目录异步增量扫描、`.cue` 整轨分轨解析与相邻分轨零重启无缝续播、8 MB RAM LRU 封面缓存、自建歌单与 M3U/M3U8 导入导出、文件与文件夹拖拽即播。

### 5. Windows 10/11 原生系统集成
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

- `src/` — 核心 C++20 源码（音频引擎、DSP/EQ/FFT、SQLite 曲库、歌词解析、D3D11 SceneGraph 组件、Win32 平台集成）
- `qml/` — QML 界面模块 `Virela.UI`（主题令牌、基础控件、沉浸舞台、视图与悬浮迷你窗）
- `translations/` — 简体中文 / 英文国际化翻译源文件（`virela_zh_CN.ts` / `virela_en_US.ts`）
- `tests/` — 5 套自动化单元测试（环形缓冲区、编码纠错、FFmpeg 解码、曲库与歌词解析、DSP/FFT）
- `scripts/` — Windows MSVC 便携打包脚本与跨平台打包脚本
- `release/Virela-win64/` — 开箱即用的 Windows x64 便携运行目录
- 更多设计细节：[架构与实现计划](docs/implementation_plan.md) · [实现总结与验证报告](docs/walkthrough.md)
