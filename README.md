# CookieGrabber · Cookie / Token 抓取工具

基于 **Win32 + Microsoft Edge WebView2** 的 Windows 桌面抓取工具：在内嵌浏览器中登录任意网站，自动或手动提取该站点的 **Cookie** 与 **Token**（localStorage / sessionStorage / 请求头 / JSON 响应体），以多种格式落盘为日志，并可一键复制到剪贴板。

整个程序编译为**单个 `CookieGrabber.exe`（约 1.7 MB）**，`WebView2Loader.dll` 作为资源内嵌在可执行文件里，拷走一个文件即可运行。

---

## 功能特性

### 浏览器页
- 地址栏（可编辑下拉框）+ 网站预设，回车或点「打开」即可导航；支持命令行传入初始网址
- 工具栏四个按钮：`打开` / `刷新` / `立即抓取Cookie` / `立即抓取Token`，Cookie 与 Token 互不影响
- 内嵌 WebView2 渲染真实网站，底部状态栏常驻显示抓取状态、`Cookie` / `Token` 计数与日志目录

### 抓取与检测
- **自动抓取**：定时轮询（1.5s）比对 Cookie 快照，登录态变化即触发抓取
- **防抖合并**：短时间内的多次请求按位或合并抓取范围（600ms 防抖 + 250ms 存储预扫描），避免后到请求覆盖先到的
- **Token 三通道捕获**：注入 JS Hook 拦截 `fetch` / `XMLHttpRequest` 请求头与 JSON 响应体，并扫描 `localStorage` / `sessionStorage`
- **首次打开即抓取**：导航完成后自动完成一次登录态采集
- **清除登录状态**：删除全部 Cookie 并重建 WebView2 用户数据目录，得到全新会话

### 输出与设置
- 日志格式可选 `字符串` / `JSON` / `两种都写`，对应 `cookie.log`、`cookie.json`、`cookie_detail.json`
- `cookie_history.log` 追加保存全部抓取记录（含 Token 明细与来源）
- 抓取后可自动复制 Cookie 串到剪贴板
- 网站预设：百度网盘、阿里云盘、夸克网盘、天翼云盘、迅雷网盘、115网盘、B站、GitHub
- 设置页可滚动、自适应宽度，「保存设置」立即生效；选中预设并修改登录网址会回写到该预设

### 界面
- **GDI 全自绘 UI**：无边框窗口 + 自绘标题栏、按钮、开关、分段选择、滚动条，不依赖任何 UI 库
- 浅色 / 深色主题，自动跟随系统（读取注册表 `AppsUseLightTheme`）
- Per-Monitor V2 DPI 感知，三档响应式布局（宽屏 / 紧凑 / 窄屏）

---

## 技术栈

| 层面 | 选型 | 说明 |
|---|---|---|
| 语言 | C++17 | 无第三方库，仅 Windows SDK + WebView2 |
| 窗口 | Win32 API | `wWinMain` 入口，`WM_NCCALCSIZE` 实现无边框客户区 |
| 绘制 | GDI | 32 位 top-down DIB 段双缓冲，`RoundRect` / `DrawTextW` 自绘全部控件 |
| 网页 | WebView2 | COM 接口 `ICoreWebView2Environment` → `Controller` → `ICoreWebView2` |
| 字体 | Segoe UI + Segoe Fluent Icons（回退 MDL2） | 图标以字形（glyph）方式绘制 |
| JSON | 自研微型库 `src/json.h`（`mjson`） | 配置读写与消息解析 |
| 构建 | MinGW-w64 GCC + `windres` | 由根目录 `build.bat` 驱动 |
| 链接 | `-static -static-libgcc -static-libstdc++` | 运行时全部静态链接，单文件分发 |

依赖管理上**没有包管理器**：没有 `vcpkg.json` / `CMakeLists.txt` / `package.json`，WebView2 的头文件与加载器 DLL 以「源码树内附带（vendoring）」方式分发，`include/WebView2Loader.dll` 在构建时复制到 `res/` 并由 `windres` 打包进 exe（`RCDATA` 资源 ID 101）。

---

## 环境要求

- Windows 10 及以上（x64）；程序按 `_WIN32_WINNT=0x0A00` 编译
- **Microsoft Edge WebView2 Runtime**（Win11 与新版 Win10 已内置；缺失时程序初始化会弹窗提示）
- MinGW-w64（仅构建时需要）：

```powershell
winget install BrechtSanders.WinLibs.POSIX.UCRT
```

> `build.bat` 默认从 `%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_*\mingw64` 定位工具链；若装在别处，请把 `g++` 加入 `PATH`。

---

## 构建与运行

```powershell
cd d:\itemi\cookie
.\build.bat                    # 产出 CookieGrabber.exe
.\CookieGrabber.exe            # 直接运行
.\CookieGrabber.exe https://pan.baidu.com   # 带初始网址启动
```

构建分三步：`copy` 加载器 DLL 到 `res/` → `windres` 编译资源 → `g++` 编译链接。

> 链接阶段会打印 `ld.exe: .rsrc merge failure: multiple non-default manifests`，**这是无害警告**：`ld` 丢弃 CRT 的空占位清单、保留项目自己的 `app.manifest`，不影响产物。

### 新增源文件

`build.bat` 显式列出所有 `.cpp`，**新增文件必须同时加进第 28 行的 g++ 参数列表**，否则不会被编译。

---

## 文件与目录位置

| 内容 | 路径 |
|---|---|
| 用户设置 | `%LOCALAPPDATA%\CookieGrabber\settings.json` |
| WebView2 用户数据 | `%LOCALAPPDATA%\CookieGrabber\userdata` |
| 日志输出 | 优先 exe 同目录；不可写时回退 `%LOCALAPPDATA%\CookieGrabber\logs` |
| 释放的内嵌加载器 | `%TEMP%\CookieGrabber\WebView2Loader.dll`（仅当 exe 同目录没有 DLL 时） |

**注意**：`settings.json` 中的 `presets` 会**整体覆盖**代码里的内置默认预设。修改 `DefaultSettings()` 后，若本机已有该文件，需一并更新或删除它才能看到变化。

---

## 目录结构

```
.
├── .qoder/repowiki/       # 中文 Wiki 与知识卡片（随仓库分发）
├── build.bat              # 唯一构建入口（windres + g++，静态链接）
├── make_icon.py           # 纯 Python 生成 32×32 应用图标（非构建步骤）
├── include/
│   ├── WebView2.h         # vendored 的 WebView2 SDK 头文件
│   └── WebView2Loader.dll # vendored 的加载器，构建时复制到 res/
├── res/
│   ├── app.rc             # 资源脚本：图标 + 清单 + 内嵌 WebView2Loader.dll
│   ├── app.manifest       # comctl v6 / PerMonitorV2 DPI / UTF-8 代码页
│   └── app.ico
├── src/
│   ├── main.cpp           # wWinMain：解析命令行网址 → App::Init → Run
│   ├── app.h/.cpp         # 应用编排：窗口、无边框框架、布局、双缓冲、消息路由
│   ├── theme.h/.cpp       # 设计令牌：Palette 调色板 + scale 常量 + 起始页 CSS 同源
│   ├── ui.h/.cpp          # 绘制层：Painter 后备缓冲、Fonts 字体集、draw:: 原语与组件
│   ├── webview.h/.cpp     # WebView2 封装：环境/控制器生命周期、导航、JS 注入、Cookie 读取
│   ├── capture.h/.cpp     # 抓取编排：掩码/防抖/轮询、Token 汇总、登录态变更检测
│   ├── settings.h/.cpp    # 配置持久化：JSON 读写、默认值、内置网站预设
│   ├── logwriter.h/.cpp   # 日志输出：多格式、原子写入、追加历史、进程内互斥
│   ├── json.h             # 微型 JSON 解析/序列化（mjson）
│   ├── util.h/.cpp        # 编码转换、时间格式、原子文件写、剪贴板、路径解析
│   └── ids.h              # 控件与资源 ID 集中定义（app.rc 与 C++ 共用）
└── test/
    ├── serve.py           # 本地测试服务器：静态页 + /api/login 返回带 token 的 JSON
    └── login.html         # 端到端验证用登录页
```

### 模块依赖方向

```
main → App ─→ WebViewHost → WebView2 COM
            ├→ CaptureManager ─→ { WebViewHost, LogWriter, Settings }
            ├→ Settings / LogWriter
            └→ UI 层 { theme, ui }   ← 仅被绘制代码引用，不反向依赖业务
```

UI 与业务解耦：WebView2 只通过 `std::function` 回调（`MessageFn` / `NavFn` / `CookiesFn` / `ReadyFn`）向外暴露事件。

---

## 文档

除本 README 外，仓库内 `.qoder/repowiki/` 随代码一同维护一套中文文档：

- `.qoder/repowiki/zh/content/` —— 分层 Wiki：项目概述、安装与构建、架构设计（系统概览 / 事件系统 / 数据流 / 各核心模块）、核心模块详解、用户界面系统（绘制层 / 主题系统）、工具函数库、配置与部署、测试与调试
- `.qoder/repowiki/knowledge/zh/` —— 知识卡片：编码规范、业务术语表、错误处理模式、依赖管理、日志系统、构建脚本与资源内嵌、WebView2 运行时

改动绘制/主题/抓取相关代码时，同步更新对应的编码规范与业务术语表卡片；新增或调整 `settings.json` 字段时，同步更新设置持久化卡片与「配置与部署」章节。

---

## 关键设计说明

**单文件分发。** C/C++ 运行时静态链接，`WebView2Loader.dll` 以 `RCDATA` 内嵌。加载策略：优先 exe 同目录的 DLL，找不到才从资源释放到 `%TEMP%` 再加载——因此放一个 exe 在任意目录都能跑。

**GDI 自绘 UI。** 所有像素都经由 `src/ui.h` 的同一套原语产生（`Rect/RoundRect/Card/FocusRing/Text/Button/Toggle/Chip/Scrollbar/Dot`），杜绝"同一个界面多种画法"造成的风格漂移。卡片投影用多层递淡描边模拟（GDI 无高斯模糊，分层描边在缩放下无锯齿）。

**双缓冲与残影。** 窗口类背景刷为空、`WM_ERASEBKGND` 直接返回 1，一帧画完再 `BitBlt` 上屏。代价是：子控件被 `MoveWindow` 移动后腾出的旧像素不会被系统擦除，因此**布局/滚动变更后必须让整个客户区失效**（`InvalidateRect(main_, nullptr, FALSE)`），只失效局部会累积成多重残影。

**焦点提示。** 自绘圆角按钮不使用 `DrawFocusRect`——它画的是 XOR 点状**直角**框，四角落在圆角之外，看起来像被裁掉的影子边。改为内缩的圆角焦点环，且只在键盘导航时显示，鼠标点击一律不显示。

**颜色与尺寸。** 绘制代码里禁止出现裸 `RGB()` 字面量，一律取 `theme::Current()` 的语义色；所有尺寸以 96dpi 逻辑像素书写，绘制前经 `App::S()` 缩放。

**错误处理。** 无统一异常框架：WebView2 异步操作以 `HRESULT` 透传并在回调内消化；同步 API（`App::Init`、`WebViewHost::Create`）返回 `bool`；日志写入采取"部分失败可接受"策略，逐个格式尽力写出并把错误文本累积到 `errOut`。弹窗只用于初始化失败、单实例重复启动提示和破坏性操作确认三处；其余运行期错误（非 http 页、写日志失败、剪贴板复制失败等）统一写到状态行。WebView2 初始化失败不向上抛，所以靠启动兜底定时器保证界面仍会出现，并用就绪计数给出提示。

---

## 本地测试

```powershell
python test\serve.py            # 监听 http://127.0.0.1:8899
# 在应用地址栏打开 http://127.0.0.1:8899/ 并点击页面上的「登 录」
```

`login.html` 会一次写入三类目标数据，用于验证三条捕获链路是否同时生效：

- **Cookie**：`session_id`、`csrf_token`
- **Web 存储**：`localStorage` 的 `access_token` / `refresh_token`、`sessionStorage` 的 `jwt`
- **请求头与响应体**：`POST /api/login` 携带 `Authorization` / `X-Auth-Token`，响应体返回 `access_token`、`refresh_token` 与嵌套的 `user.token`

---

## 已知限制

- 仅支持 x64 Windows，无交叉编译或其他架构目标
- 需要系统已安装 WebView2 Runtime；离线环境下无法加载网页
- 无版本化/签名/安装包流水线，`build.bat` 只产出裸 exe
- 依赖升级需手动替换 `include/WebView2.h` 与 `WebView2Loader.dll` 并重新提交
- 未指定许可证，代码版权归仓库所有者
