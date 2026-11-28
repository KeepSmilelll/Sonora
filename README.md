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

### 1. 流体空间界面与响应式高 DPI 适配
- **在线为主、本地融合的首屏布局**：启动默认进入 **发现首页（`OnlineMusicView`）** 并默认折叠右侧舞台，聚焦「继续收听」混合卡片、精选推荐歌单与新歌速递（已精简多余顶栏按钮与热搜干扰）；未配置在线源时自动降级展示本地专辑墙与引导卡片；全局 `Ctrl+K` 命令搜索浮窗（`SearchSuggestionsPopover`）实时联动本地 FTS5 检索与网易云歌单链接直达。
- **紧凑导航与 4 标签设置中心**：可折叠侧边栏（`NavigationRail`）支持自建歌单与 **固定在线歌单（Pinned Online Playlists）** 一键直达，并在窄屏/矮屏下自动紧凑化密度；4 标签设置中心分为 **在线与缓存 / 音频与输出 / 本地曲库 / 外观与桌面**（宽屏自动居中约束在 `960px` 舒适阅读列宽，长文本自动换行）。
- **纯净背景与精致微交互动效**：内置自定义 GPU 流体极光着色器（`FluidAuroraItem`），支持「轻量封面氛围渐变」与「极简纯色」双背景风格（已全面移除 Windows 11 透明磨砂，保留原生圆角、阴影与沉浸深浅色边框）；支持按需加载并在结束后自动卸载的径向扩散主题切换动画（`ThemeTransitionOverlay`）、方向性平滑切页过渡、浮动音量百分比数字提示、缓存文案防塌缩的弹性气泡提示，以及高 DPI `IconImageProvider` 矢量图标。
- **1080p / 1440p 宽度与 125% / 150% 缩放自适应**：启用 Qt `PassThrough` 小数 DPI 策略与按当前显示器工作区居中启动（[`WindowLayout.h`](src/core/WindowLayout.h)，最小窗口 `840×560`，在 1080p@150% `1280×672` 逻辑工作区下完整贴合任务栏不越界；在 1440p 宽屏下自动扩展为 1–5 列卡片网格、动态歌手/专辑列宽与居中双栏沉浸舞台），配合 [`Win32WindowHelper.cpp`](src/platform/win32/Win32WindowHelper.cpp) 多倍缩放命中测试与对称三区底部播控栏（`PlaybackBar`）。
- **连续三态空间舞台与 Hero 转场**：支持「折叠 / 右侧栏 / 全窗口沉浸页」无缝切换，搭配专辑封面到旋转黑胶唱片的 Hero 飞入缩放动画；无封面状态采用深石墨金属唱片中心标，有封面时按需加载圆形遮罩（`VinylCoverMask`）；按需加载悬停变形 `MiniPlayerWindow`。

### 2. 在线流媒体、离线下载与跨源统一
- **双音源流式播放与内置兜底**：支持网易云 API 与 Meting API 双源配置、标准 / 极高 / 无损（Lossless）/ Hi-Res 音质选择与直链解析优先级；**跨源自动回退与试听/无版权解锁兜底（`/song/url/match` + Meting 兜底）始终开启**；内置 [`LocalBackendManager`](src/online/LocalBackendManager.cpp) 可安全托管本地 Node.js 后端进程并随主程序自动启停。
- **分段流式缓存与下载中心**：通过 `StreamCacheManager` 进行可配置上限的分段边缘缓存与缓冲进度指示；`DownloadsView` 管理下载队列，完成后由 `AudioMetadataWriter` 自动写入曲目标签、高清封面与歌词并入库。
- **跨来源数据层统一**：本地查询与在线影子记录通过 `source` 严格隔离、互不污染本地曲库计数；「我喜欢的音乐」「最近播放」与自建歌单原生支持本地与在线曲目混排，且播控栏/舞台统一开放红心收藏；在线曲目下载完成后调用 `upgradeOnlineTrackToLocal()` 原位升级为本地曲目并完整保留播放次数、收藏状态与歌单关联。

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
- **全链路按需加载与内存治理**：主视图切换与 `AppDialogs` / `FocusStage` / `EqDrawer` / `SettingsView` / `ThemeTransitionOverlay` / `VinylCoverMask` / `MiniPlayerWindow` 全部采用按需 `Loader` 并在失活时卸载释放；列表封面禁用 Qt 隐式图像缓存并按 DPR 封顶尺寸；配合 `CoverArtProvider`（8 MB RAM LRU）、`QPixmapCache`（4 MB 上限）与切页 `trimTransientMemory()` 回收，实测标准不透明 D3D11 管线下冷启动工作集约 **~173 MB RSS**，最小化至托盘后自动清理瞬态缓存并裁剪 Win32 工作集。

### 6. Windows 10/11 原生系统集成
- 无边框原生窗口（完整保留 Aero Snap 分屏、最大化防裁剪与窗口圆角）。
- 单实例守护与二次启动文件路径转发。
- Windows SMTC 锁屏 / 通知中心媒体卡片、物理媒体键响应、原生 `Shell_NotifyIconW` 系统托盘、全局媒体快捷键（`Ctrl+Alt+Space / 方向键 / M`）。

---

## 在线音源 API 部署指南（双后端配置）

Virela **不内置任何公共第三方接口地址**，完全由用户连接自托管的 **NeteaseCloudMusicApi**（主后端，默认 `http://127.0.0.1:3000`）与 **Meting API**（可选直链与兜底后端，默认 `http://127.0.0.1:3001/api`）。你可以只部署其中之一，或同时部署两者实现高可用自动回退。

### 环境准备
- **本地 Node.js 托管模式（推荐）**：安装 **Node.js `>= 18`（推荐 20 / 22 LTS）**，确保 `node` 在系统 `PATH` 中（Windows 下 Virela 也会自动探测 `C:\Program Files\nodejs\node.exe`、`D:\nodejs\node.exe`）。
- **Docker / 远程服务器模式**：任何能从本机访问的局域网 / 云端 HTTP(S) 服务均可直接填写 URL 使用。

### 1. 部署 NeteaseCloudMusicApi（主后端，默认端口 `3000`）

提供发现页推荐歌单/新歌、搜索、专辑/歌手/歌单详情、歌词（含逐字 `.yrc`）、音质解析以及无版权解锁接口（`/song/url/match`）。推荐使用包含 `/song/url/match` 兜底能力的增强版仓库：

**方式 A：本地 Node.js 部署（可交由 Virela 自动启停）**

```powershell
git clone https://github.com/NeteaseCloudMusicApiEnhanced/api-enhanced.git netease-api
cd netease-api
npm install --omit=dev

# 可选：手动验证启动（默认监听 127.0.0.1:3000）
$env:HOST = "127.0.0.1"
$env:PORT = "3000"
$env:ENABLE_FLAC = "true"
node app.js
```

**方式 B：Docker 独立部署**

```bash
docker run -d --name virela-netease-api \
  -p 127.0.0.1:3000:3000 \
  -e ENABLE_FLAC=true \
  ghcr.io/neteasecloudmusicapienhanced/api-enhanced:latest
```

- **连通性自检**：浏览器或终端访问 `http://127.0.0.1:3000/search/hot`，返回 `{"code":200,...}` 即表示就绪。

### 2. 部署 Meting API（可选直链与兜底后端，默认端口 `3001`）

当主后端遇到版权受限或超时时，Virela 会按直链优先级自动通过 Meting API 获取播放直链与基础元信息/歌词。

**方式 A：本地 Node.js 部署（可交由 Virela 自动启停）**

```powershell
git clone https://github.com/xizeyoupan/Meting-API.git meting-api
cd meting-api
npm install --omit=dev

# 可选：手动验证启动（默认端口 3001）
$env:HOST = "127.0.0.1"
$env:PORT = "3001"
node index.js
```

> 若你的 Meting 服务入口文件为 `server.js` / `server.mjs` / `app.js` / `index.js` / `index.mjs` 中的任意一个，Virela 均会自动识别（部分自建共宿主脚本亦可通过环境变量 `VIRELA_METING_DIR` 与主服务协同拉起）。

**方式 B：Docker 独立部署**

```bash
docker run -d --name virela-meting-api \
  -p 127.0.0.1:3001:3000 \
  -e METING_URL=http://127.0.0.1:3001 \
  xizeyoupan/meting-api:latest
```

- **连通性自检**：访问 `http://127.0.0.1:3001/api?server=netease&type=song&id=186016`，能返回 JSON 曲目数据或 302 音频跳转即表示就绪。

### 3. 在 Virela 中配置与开启随程序自动托管

打开 Virela 左下角 **设置 → 在线与缓存**：

1. **填写接口地址**：
   - **网易云 API 地址（主后端）**：例如 `http://127.0.0.1:3000`
   - **Meting API 地址（可选直链与兜底）**：例如 `http://127.0.0.1:3001/api`
   - 点击 **测试连接** 验证状态显示为正常。
2. **（可选）开启本地后端自动托管**：
   - 打开 **随 Virela 自动启动本地后端** 开关；
   - 分别选择你本机保存的 **Netease 服务目录**（如 `D:\services\netease-api`）和 **Meting 服务目录**（如 `D:\services\meting-api`）。
   - Virela 启动后端时的内部行为（由 [`LocalBackendManager`](src/online/LocalBackendManager.cpp) 实现）：
     - 按 `server.js → server.mjs → app.js → index.js → index.mjs` 自动查找入口脚本；
     - 以 `node --max-old-space-size=96 <entry>` 无黑框模式（`CREATE_NO_WINDOW`）启动，限制单进程内存上限；
     - 自动从配置 URL 解析端口并注入 `HOST=127.0.0.1`、`PORT`、`NODE_ENV=production`、`VIRELA_MANAGED=1`，并依据当前默认音质为网易云服务注入 `ENABLE_FLAC=true/false`；
     - 若目标端口已提前被外部手动启动（例如你在后台先跑了 Docker 或终端），自动标记为 `External` 复用，不抢占端口；
     - 通过 Windows **Job Object（`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`）** 绑定生命周期，关闭或退出 Virela 时自动清理托管子进程，不在后台残留。
3. **音质与优先级**：可按需选择「直链解析优先级（网易云 API 优先 / Meting API 优先）」与「默认在线试听与下载音质（标准 / 极高 / 无损 FLAC / Hi-Res）」。

### 4. 接口协议约定参考（供自定义反向代理或兼容层使用）

| 后端 | 用途 | Virela 实际调用的路由 / 参数 |
| --- | --- | --- |
| **Netease API** | 发现与搜索 | `/search/hot`、`/cloudsearch`、`/search`、`/personalized`、`/top/playlist`、`/personalized/newsong`、`/top/song` |
| **Netease API** | 详情与歌词 | `/playlist/detail`、`/playlist/track/all`、`/album`、`/artists`、`/artist/top/song`、`/lyric/new`、`/lyric` |
| **Netease API** | 播放直链与解锁 | `/song/url/v1`、`/song/url`、`/song/url/match`（支持 `source=unm` 与通用匹配回退） |
| **Meting API** | 直链、详情与歌词 | `<base>?server=netease&type=<url\|song\|playlist\|album\|lrc\|search>&id=<id>&br=<128000\|320000\|999000>`（`type=url` 支持 `302 Location`、`{"url":"..."}`、`[{"url":"..."}]` 或纯文本 URL） |

---

## 音频引擎推荐设置速查

在 **设置 → 音频与输出 → 音频引擎与 WASAPI 输出** 中可按使用场景快速选择：

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

或使用 CMake Presets 手动构建、校验与执行多缩放烟雾测试：

```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release --parallel
ctest --preset windows-msvc-release --output-on-failure
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

- `src/` — 核心 C++20 源码（音频引擎、DSP/EQ/FFT、SQLite 跨源曲库、在线流媒体服务与分段缓存、元数据写入、本地后端托管、歌词解析、`WindowLayout.h` 屏幕/DPI 布局、D3D11 SceneGraph & `IconImageProvider`、Win32 平台集成）
- `qml/` — QML 界面模块 `Virela.UI`（主题令牌 `DesignTokens`、按需动效与遮罩组件 `ThemeTransitionOverlay` / `VinylCoverMask`、发现页 `OnlineMusicView`、本地中心 `LocalMusicHubView`、下载管理 `DownloadsView`、命令搜索浮窗 `SearchSuggestionsPopover`、按需弹窗 `AppDialogs`、沉浸舞台 `FocusStage` 与悬浮迷你窗 `MiniPlayerWindow`）
- `translations/` — 简体中文 / 英文国际化翻译源文件（`virela_zh_CN.ts` / `virela_en_US.ts`）
- `tests/` — 6 套自动化单元测试（环形缓冲区、编码纠错、FFmpeg 解码、曲库与跨源数据及歌词解析、DSP/FFT、在线流媒体/配置迁移与 1080p/1440p 多缩放启动几何）
- `scripts/` — Windows MSVC 便携打包脚本与跨平台打包脚本
- `release/Virela-win64/` — 开箱即用的 Windows x64 便携运行目录
- 更多设计细节：[架构与实现计划](docs/implementation_plan.md) · [实现总结与验证报告](docs/walkthrough.md)
