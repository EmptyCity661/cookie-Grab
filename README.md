# cookie抓取工具3.0 · Cookie / Token 抓取工具

基于 **Win32 + Microsoft Edge WebView2** 的 Windows 桌面抓取工具：在内嵌浏览器中登录任意网站，自动或手动提取该站点的 **Cookie** 与 **Token**（`localStorage` / `sessionStorage` / 请求头 / JSON 响应体），以多种格式落盘为日志，并可一键复制到剪贴板。

界面采用 **WebView2 玻璃外壳（Glass Shell）**：整个窗口 chrome（标题栏 / 顶部导航 / 工具栏 / 内容视口 / 设置页 / 说明页 / 状态栏）由一个铺满客户区的 WebView2 以 HTML/CSS 渲染，被访问的网站则由独立的 Content WebView2 承载。程序静态链接为**单个 `cookie抓取工具3.0.exe`（约 3.3 MB）**，`WebView2Loader.dll` 以资源内嵌，拷走一个文件即可运行。

> **许可证** [Apache-2.0](LICENSE)（版权 © 空.城） · **发布版本** [Releases](https://github.com/EmptyCity661/cookie-Grab/releases)（当前 `v1.0.0`） · **贡献指南** [CONTRIBUTING.md](CONTRIBUTING.md) · **变更记录** [CHANGELOG.md](CHANGELOG.md)

---

## 功能特性

### 浏览器页
- 地址栏（可编辑下拉框）+ 网站预设，回车或点「打开」即可导航；支持命令行传入初始网址
- 工具栏按钮：`打开` / `刷新` / `立即抓取Cookie` / `立即抓取Token`，Cookie 与 Token 互不影响
- 内嵌 WebView2 渲染真实网站，底部状态栏常驻显示抓取状态、`Cookie` / `Token` 计数与日志目录
- 标题栏支持拖动、双击「最大化 / 还原」、边缘缩放，以及最小化 / 最大化 / 关闭按钮

### 抓取与检测
- **自动抓取**：定时轮询（约 1.5s）比对 Cookie 快照，登录态变化即触发抓取
- **防抖合并**：短时间内的多次请求按位或合并抓取范围（约 600ms 防抖 + 250ms 存储预扫描），避免后到请求覆盖先到的
- **Token 三通道捕获**：注入 JS Hook 拦截 `fetch` / `XMLHttpRequest` 请求头与 JSON 响应体，并扫描 `localStorage` / `sessionStorage`
- **首次打开即抓取**：导航完成后自动完成一次登录态采集
- **清除登录状态**：删除全部 Cookie 并重建 WebView2 用户数据目录，得到全新会话

### 输出与设置
- 日志格式可选 `字符串` / `JSON` / `两种都写`，对应 `cookie.log`、`cookie.json`、`cookie_detail.json`
- `cookie_history.log` 追加保存全部抓取记录（含 Token 明细与来源）
- 抓取后可自动复制 Cookie 串到剪贴板
- 网站预设：百度网盘、阿里云盘、夸克网盘、天翼云盘、迅雷网盘、115网盘、B站、GitHub
- 设置页可滚动、自适应宽度，「保存设置」立即生效；选中预设并修改登录网址会回写到该预设

### 界面与主题
- **WebView2 玻璃外壳**：无边框窗口 + HTML/CSS 渲染全部界面；原生仅保留窗口机制（拖动 / 缩放 / 贴边 / 阴影 / 最小化 / 最大化）与消息循环
- 浅色 / 深色主题，自动跟随系统（读取注册表 `AppsUseLightTheme`）
- 主题令牌（`src/theme.*`）为唯一配色来源，原生窗口与 WebView2 起始页共用同一份调色板
- Per-Monitor V2 DPI 感知；界面为响应式 CSS 布局

---

## 技术栈

| 层面 | 选型 | 说明 |
|---|---|---|
| 语言 | C++17 | 无第三方库，仅 Windows SDK + WebView2 |
| 窗口 | Win32 API | `wWinMain` 入口，`WM_NCCALCSIZE` 实现无边框客户区 |
| 界面 | WebView2（HTML/CSS） | 玻璃外壳铺满客户区渲染全部 chrome；Content WebView2 承载网站 |
| 原生绘制 | GDI + DWM | 仅保留窗口阴影、就绪前底色等少量原生绘制 |
| 网页 | WebView2 | COM：`ICoreWebView2Environment` → `Controller` → `ICoreWebView2` |
| 字体 | 系统字体栈 + MDL2 字形（Segoe Fluent Icons） | 窗口按钮图标以字形方式绘制 |
| JSON | 自研微型库 `src/json.h`（`mjson`） | 配置读写与消息解析 |
| 构建 | MinGW-w64 GCC + `windres` | 由根目录 `build.bat` 驱动 |
| 链接 | `-static -static-libgcc -static-libstdc++` | 运行时全部静态链接，单文件分发 |

依赖管理上**没有包管理器**：没有 `vcpkg.json` / `CMakeLists.txt` / `package.json`。WebView2 的头文件与加载器 DLL 以「源码树内附带（vendoring）」方式分发，`include/WebView2Loader.dll` 在构建时复制到 `res/` 并由 `windres` 打包进 exe（`RCDATA` 资源 ID 101）。

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
.\build.bat                          # 产出 cookie抓取工具3.0.exe
.\cookie抓取工具3.0.exe                  # 直接运行
.\cookie抓取工具3.0.exe https://pan.baidu.com   # 带初始网址启动
```

构建分三步：`copy` 加载器 DLL 到 `res/` → `windres` 编译资源 → `g++` 编译链接。

> 链接阶段会打印 `ld.exe: .rsrc merge failure: multiple non-default manifests`，**这是无害警告**：`ld` 丢弃 CRT 的空占位清单、保留项目自己的 `app.manifest`，不影响产物。

### 新增源文件

`build.bat` 显式列出所有 `.cpp`，**新增文件必须同时加进 g++ 参数那一行**，否则不会被编译。

---

## 文件与目录位置

| 内容 | 路径 |
|---|---|
| 用户设置 | `%LOCALAPPDATA%\cookie抓取工具3.0\settings.json` |
| WebView2 用户数据 | `%LOCALAPPDATA%\cookie抓取工具3.0\userdata` |
| 日志输出 | 优先 exe 同目录；不可写时回退 `%LOCALAPPDATA%\cookie抓取工具3.0\logs` |
| 释放的内嵌加载器 | `%TEMP%\cookie抓取工具3.0\WebView2Loader.dll`（仅当 exe 同目录没有 DLL 时） |

> `settings.json` 中的 `presets` 会**整体覆盖**代码里的内置默认预设。修改 `DefaultSettings()` 后，若本机已有该文件，需一并更新或删除它才能看到变化。

---

## 配置说明

首次运行会在 `%LOCALAPPDATA%\cookie抓取工具3.0\settings.json` 生成配置，「保存设置」时写回。字段如下：

| 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `autoCapture` | bool | `true` | 检测到登录状态变化时自动抓取 |
| `firstOpenCapture` | bool | `true` | 导航完成后首次打开即抓取一次 |
| `clipboard` | bool | `true` | 抓取后自动复制 Cookie 串到剪贴板 |
| `history` | bool | `true` | 追加写入 `cookie_history.log` |
| `logFormat` | string | `"string"` | 日志格式：`string` / `json` / `both` |
| `presets` | array | 内置 8 项 | 网站预设，元素为 `{"name":"...","url":"..."}` |

> `logFormat` 决定落盘格式：`string`→`cookie.log`，`json`→`cookie.json`，`both`→两者都写；此外固定输出 `cookie_detail.json` 与 `cookie_history.log`。

---

## 目录结构

```
.
├── .qoder/repowiki/        # 中文知识卡片（随仓库分发，工具生成；分层 Wiki 可重新生成）
├── build.bat               # 唯一构建入口（windres + g++，静态链接）
├── finalize.ps1            # 构建辅助：GBK 转码 + 重命名为中文 exe 名
├── make_icon.py            # 纯 Python 生成 32×32 应用图标（非构建步骤）
├── README.md               # 本说明
├── CONTRIBUTING.md         # 贡献指南
├── CHANGELOG.md            # 版本变更记录
├── LICENSE                 # Apache-2.0 许可证
├── include/
│   ├── WebView2.h          # vendored 的 WebView2 SDK 头文件
│   └── WebView2Loader.dll  # vendored 的加载器，构建时复制到 res/
├── res/
│   ├── app.rc              # 资源脚本：图标 + 清单 + 内嵌 WebView2Loader.dll
│   ├── app.manifest        # comctl v6 / PerMonitorV2 DPI / UTF-8 代码页
│   ├── app.ico
│   └── app.res             # windres 生成（已由 .gitignore 忽略）
├── src/
│   ├── main.cpp            # wWinMain：解析命令行网址 → App::Init → Run
│   ├── app.h/.cpp          # 应用主体：无边框窗口、消息循环、定时器、桥接分发
│   ├── shell.h/.cpp        # 玻璃外壳：生成整页 HTML/CSS 与增量 state 推送
│   ├── theme.h/.cpp        # 设计令牌：调色板 + 外壳/起始页 CSS（唯一配色来源）
│   ├── webview.h/.cpp      # WebView2 封装：环境/控制器生命周期、导航、JS 注入、Cookie 读取
│   ├── capture.h/.cpp      # 抓取编排：掩码/防抖/轮询、Token 汇总、登录态变更检测
│   ├── settings.h/.cpp     # 配置持久化：JSON 读写、默认值、内置网站预设
│   ├── logwriter.h/.cpp    # 日志输出：多格式、原子写入、追加历史、进程内互斥
│   ├── util.h/.cpp         # 编码转换、时间格式、原子文件写、剪贴板、路径解析
│   ├── json.h              # 微型 JSON 解析/序列化（mjson）
│   └── ids.h               # 控件与资源 ID 集中定义（app.rc 与 C++ 共用）
└── test/
    ├── serve.py            # 本地测试服务器：静态页 + /api/login 返回带 token 的 JSON
    └── login.html          # 端到端验证用登录页
```

> 早期基于原生 GDI 自绘的 `src/ui.*` 已被玻璃外壳取代并删除，`build.bat` 不再编译它。

### 模块依赖方向

```
main → App ─┬→ WebViewHost ────→ WebView2 COM（Content：承载被访问网站）
            ├→ Shell ──────────→ WebView2 COM（外壳：渲染全部 chrome）
            ├→ CaptureManager ┬→ WebViewHost
            │                 └→ LogWriter / Settings
            ├→ Settings / LogWriter
            └→ theme（视觉令牌：原生与外壳 CSS 共用）
```

UI 与业务解耦：WebView2 只通过 `std::function` 回调（`MessageFn` / `NavFn` / `CookiesFn` / `ReadyFn`）向外暴露事件；外壳与宿主之间经 `window.chrome.webview.postMessage`（JS → host）与 `ExecuteScript("window.CG.state(...)")`（host → JS）通信，**Cookie / Token 值绝不进入外壳**。

---

## 文档

除本 README 外，仓库内 `.qoder/repowiki/knowledge/zh/` 随代码一同维护一套中文知识卡片：编码规范、业务术语表、错误处理模式、依赖管理、日志系统、构建脚本与资源内嵌、WebView2 运行时，以及按模块组织的项目文档（如「单进程桌面抓取工具」）。

> 早期的 `.qoder/repowiki/zh/content/` 分层 Wiki 为迁移前生成、描述的是已删除的 GDI 自绘层，现已移除；如需结构化 Wiki，可用 repowiki 工具按当前代码重新生成。

改动外壳 / 主题 / 抓取相关代码时，同步更新对应的编码规范与业务术语表卡片；新增或调整 `settings.json` 字段时，同步更新设置持久化卡片。

---

## 关键设计说明

**单文件分发。** C/C++ 运行时静态链接，`WebView2Loader.dll` 以 `RCDATA`（ID 101）内嵌。加载策略：优先 exe 同目录的 DLL，找不到才从资源释放到 `%TEMP%` 再加载——因此放一个 exe 在任意目录都能跑。

**玻璃外壳双视图。** 外壳 WebView 铺满客户区，绘制环境光斑与各视图；被导航的第三方站点由独立的 Content WebView（`App::webview_`）承载，按外壳上报的 `#webhole` 矩形定位并覆盖其上（二者是主窗口的兄弟子窗口，Content 在孔区 Z 序更高）。双向桥：JS → host 用 `window.chrome.webview.postMessage(JSON)`，统一信封 `{t:"...",...}`；host → JS 用 `ExecuteScript("window.CG.state({...})")`。**Cookie / Token 值绝不进入外壳**，只把计数与状态文字推给外壳。

**主题单一来源。** 所有颜色取 `theme::Current()` 的语义色，外壳 CSS 与原生窗口共用同一份令牌（`theme::StartPageCss()`），从根上消除「两套配色」导致的风格割裂。绘制代码里禁止出现裸 `RGB()` 字面量。

**原生窗口机制。** `WM_NCCALCSIZE` 去掉非客户区实现无边框；最大化时按 `SM_CXSIZEFRAME + SM_CXPADDEDBORDER` 内缩客户区，避免内容溢出屏幕。拖动 / 缩放在外壳就绪后由 JS 桥接实现（`ReleaseCapture` + `WM_NCLBUTTONDOWN`，命中 `HTCAPTION` 或各边缘 `HT*`）；标题栏双击由外壳侧自计时判定（第二击直接切换最大化 / 还原，且不进入移动模态循环——否则会吞掉配套 `mouseup`，令浏览器永不派发 `dblclick`）。

**错误处理。** 无统一异常框架：WebView2 异步操作以 `HRESULT` 透传并在回调内消化；同步 API（`App::Init`、`WebViewHost::Create`）返回 `bool`；日志写入采取「部分失败可接受」策略，逐个格式尽力写出并把错误文本累积到 `errOut`。弹窗只用于初始化失败、单实例重复启动提示和破坏性操作确认三处；其余运行期错误（非 http 页、写日志失败、剪贴板复制失败等）统一写到状态行。WebView2 初始化失败不向上抛，所以靠启动兜底定时器保证界面仍会出现，并用就绪计数给出提示。

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

## 贡献指引

欢迎提交 Issue 与 Pull Request。为保持架构与风格一致，请遵循：

- **架构边界**：界面一律走 WebView2 玻璃外壳（`src/shell.*` 以 HTML/CSS 渲染），不要重新引入原生 GDI 自绘控件；被访问站点由 Content WebView2 承载，二者经 `postMessage` / `ExecuteScript` 通信，**Cookie / Token 值绝不进入外壳 DOM、`state` 或页面日志**。
- **配色与尺寸**：颜色统一取 `theme::Current()` 语义色，禁止裸 `RGB()` 字面量；原生绘制尺寸以 96dpi 逻辑像素书写并经 `App::S()` 缩放。
- **新增源文件**：`build.bat` 显式列出全部 `.cpp`，新文件必须同时加入 g++ 参数行，否则不参与编译。
- **构建与验证**：提交前运行 `.\build.bat` 确认可编译；抓取相关改动用 `python test\serve.py` + `login.html` 做端到端验证。
- **提交信息**：使用清晰的行为描述，建议 `feat:` / `fix:` / `docs:` / `refactor:` 前缀，并说明动机与影响范围。
- **文档同步**：改动外壳 / 主题 / 抓取逻辑时，同步更新本 README 与 `.qoder/repowiki/knowledge` 下对应知识卡片。

本项目采用 [Apache-2.0](LICENSE) 许可证（版权 © 空.城）；贡献即表示你同意以其条款并入。完整的开发规范、提交信息与 PR 流程见 [CONTRIBUTING.md](CONTRIBUTING.md)。

---

## 已知限制

- 仅支持 x64 Windows，无交叉编译或其他架构目标
- 需要系统已安装 WebView2 Runtime；离线环境下无法加载网页
- 无版本化 / 签名 / 安装包流水线，`build.bat` 只产出裸 exe
- 依赖升级需手动替换 `include/WebView2.h` 与 `WebView2Loader.dll` 并重新提交
- 未指定许可证，代码版权归仓库所有者
