# cookie抓取工具3.0

用内嵌的 WebView2 浏览器登录网站，然后把 Cookie 和 Token 抓下来存好——主要给自己用的一个小工具。

在程序里打开任意网页、登录账号，它会自动（也可以手动）把当前站点的登录态提取出来，写成日志、或者丢进剪贴板。Token 的抓取覆盖三种来源：`localStorage` / `sessionStorage`、请求头、以及 JSON 响应体，常见网站的鉴权方式基本都能拿到。

界面是整个窗口都用 WebView2 当"玻璃外壳"画的：标题栏、工具栏、设置页、状态栏这些都是一个铺满窗口的网页（HTML/CSS），真正被访问的站点由另一个独立的 WebView2 承载。整个程序静态链接，打出来就是一个 `cookie抓取工具3.0.exe`（大概 3.3 MB），`WebView2Loader.dll` 也塞进 exe 里了，所以拷一个文件就能跑。

许可证是 Apache-2.0（版权 © 空.城）。版本发布记录在 [Releases](https://github.com/EmptyCity661/cookie-Grab/releases)，想一起开发看 [CONTRIBUTING.md](CONTRIBUTING.md)，改动历史见 [CHANGELOG.md](CHANGELOG.md)。

## 功能

### 浏览器页
- 地址栏是可编辑的下拉框，带网站预设，回车或点"打开"就跳过去；启动时可以命令行带一个初始网址。
- 工具栏四个按钮："打开"、"刷新"、"立即抓取 Cookie"、"立即抓取 Token"。Cookie 和 Token 走两套逻辑，互不干扰，各抓各的。
- 底下状态栏一直显示当前抓取状态、Cookie / Token 计数，以及日志写在哪。
- 标题栏可以拖动、双击在最大化和还原之间切、拖到屏幕边缘也能缩放；最小化 / 最大化 / 关闭按钮都正常。

### 抓取
- 自动抓取：每 1.5 秒左右比对一次 Cookie 快照，发现登录态变了就抓。
- 防抖合并：短时间内来好几个请求，会把抓取范围按位或合并（约 600ms 防抖 + 250ms 预扫描），避免后面的请求把前面的覆盖掉。
- Token 三通道：往页面里注入 JS，Hook 掉 `fetch` / `XMLHttpRequest` 的请求头和 JSON 响应体，同时扫一遍 `localStorage` / `sessionStorage`。
- 首次打开即抓取：导航一完成就先做一轮采集。
- 清除登录状态：把所有 Cookie 删掉并重建 WebView2 的用户数据目录，等于换一个全新会话。

### 输出与设置
- 日志格式三选一：字符串 / JSON / 两种都写，分别对应 `cookie.log`、`cookie.json`、`cookie_detail.json`。
- 另外固定会写 `cookie_history.log`，把每次抓取（含 Token 明细和来源）都追加进去。
- 抓完可以自动把 Cookie 串复制到剪贴板。
- 内置 8 个网站预设：百度网盘、阿里云盘、夸克网盘、天翼云盘、迅雷网盘、115网盘、B站、GitHub。
- 设置页能滚动、宽度自适应，"保存设置"即时生效；选了某个预设再改登录网址，会写回那个预设里。

### 界面与主题
- 玻璃外壳就是无边框窗口 + 一个铺满的网页画全部界面；原生这边只留窗口机制（拖动、缩放、贴边、阴影、最小化、最大化）和消息循环。
- 浅色 / 深色主题自动跟随系统（读注册表 `AppsUseLightTheme`）。
- 配色集中在 `src/theme.*` 这一份令牌里，原生窗口和 WebView2 起始页用的是同一套，不会出现两套颜色对不上的情况。
- 支持 Per-Monitor V2 的 DPI 缩放，界面是响应式的 CSS 布局。

## 技术栈

| 层面 | 选型 | 说明 |
|---|---|---|
| 语言 | C++17 | 不依赖第三方库，只用 Windows SDK + WebView2 |
| 窗口 | Win32 API | `wWinMain` 入口，靠 `WM_NCCALCSIZE` 去掉边框做无边框客户区 |
| 界面 | WebView2（HTML/CSS） | 玻璃外壳铺满客户区画全部界面；网站由另一个 Content WebView2 承载 |
| 原生绘制 | GDI + DWM | 只保留窗口阴影、启动前的底色这类少量原生绘制 |
| 网页 | WebView2 COM | `ICoreWebView2Environment` → `Controller` → `ICoreWebView2` |
| 字体 | 系统字体 + MDL2 字形（Segoe Fluent Icons） | 窗口按钮的图标是字形画的 |
| JSON | 自研微型库 `src/json.h`（mjson） | 负责配置读写和消息解析 |
| 构建 | MinGW-w64 GCC + `windres` | 全部由根目录的 `build.bat` 驱动 |
| 链接 | `-static -static-libgcc -static-libstdc++` | 运行时全静态链接，所以能单文件分发 |

这个项目没有用任何包管理器，既没有 `vcpkg.json`、`CMakeLists.txt`，也没有 `package.json`。WebView2 的头文件和加载器 DLL 是直接放进源码树里分发的（`include/WebView2Loader.dll` 在构建时复制到 `res/`，再由 `windres` 打进 exe，资源 ID 是 101 的 RCDATA）。

## 环境要求
- Windows 10 及以上（x64），程序按 `_WIN32_WINNT=0x0A00` 编译。
- 系统要装 **Microsoft Edge WebView2 Runtime**（Win11 和新版 Win10 自带；没有的话程序启动会弹窗提示）。
- 构建才需要 MinGW-w64：

```powershell
winget install BrechtSanders.WinLibs.POSIX.UCRT
```

`build.bat` 默认去 `%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_*\mingw64` 找工具链；要是装在别处，把 `g++` 加进 `PATH` 就行。

## 构建与运行

```powershell
cd d:\itemi\cookie
.\build.bat                          # 生成 cookie抓取工具3.0.exe
.\cookie抓取工具3.0.exe                  # 直接双击运行
.\cookie抓取工具3.0.exe https://pan.baidu.com   # 启动时就打开某个网址
```

构建就三步：先把加载器 DLL 复制到 `res/`，再用 `windres` 编译资源，最后 `g++` 编译链接。

> 链接时可能会打印一句 `ld.exe: .rsrc merge failure: multiple non-default manifests`，别慌，没影响——`ld` 只是扔掉 CRT 那个空的占位清单、留着我们自己的 `app.manifest`，产物完全正常。

### 加了新源文件要注意
`build.bat` 是把所有 `.cpp` 一个个列出来的，**新加的文件必须同时写进 g++ 那一行参数里**，不然它不会被编译进去。

## 文件和目录放在哪

| 内容 | 路径 |
|---|---|
| 用户设置 | `%LOCALAPPDATA%\cookie抓取工具3.0\settings.json` |
| WebView2 用户数据 | `%LOCALAPPDATA%\cookie抓取工具3.0\userdata` |
| 日志输出 | 优先写在 exe 同目录；写不进去就回退到 `%LOCALAPPDATA%\cookie抓取工具3.0\logs` |
| 释放出来的内嵌加载器 | `%TEMP%\cookie抓取工具3.0\WebView2Loader.dll`（仅当 exe 同目录没有 DLL 时） |

有一点要注意：`settings.json` 里的 `presets` 是**整体覆盖**代码里那套默认预设的。所以如果你改过 `DefaultSettings()`，本机又已经有了这个文件，得手动更新或删掉它才能看到变化。

## 配置说明

第一次运行会在 `%LOCALAPPDATA%\cookie抓取工具3.0\settings.json` 生成配置，之后每次"保存设置"都会写回。字段就这几个：

| 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `autoCapture` | bool | `true` | 检测到登录态变化就自动抓 |
| `firstOpenCapture` | bool | `true` | 导航完成后第一次打开就抓一轮 |
| `clipboard` | bool | `true` | 抓完自动把 Cookie 串复制到剪贴板 |
| `history` | bool | `true` | 是否往 `cookie_history.log` 追加 |
| `logFormat` | string | `"string"` | 日志格式：`string` / `json` / `both` |
| `presets` | array | 内置 8 项 | 网站预设，元素是 `{"name":"...","url":"..."}` |

`logFormat` 决定写什么文件：`string` 写 `cookie.log`，`json` 写 `cookie.json`，`both` 两个都写；另外 `cookie_detail.json` 和 `cookie_history.log` 是无论如何都会出的。

## 目录结构

```
.
├── .qoder/repowiki/        # 中文知识卡片，工具生成的，跟着仓库走（分层 Wiki 可重生成）
├── build.bat               # 唯一的构建入口（windres + g++，静态链接）
├── finalize.ps1            # 构建辅助：GBK 转码 + 把产物重命名成中文 exe 名
├── make_icon.py            # 纯 Python 生成 32×32 图标（不是构建必需）
├── README.md               # 就是这份
├── CONTRIBUTING.md         # 贡献指南
├── CHANGELOG.md            # 版本改动记录
├── LICENSE                 # Apache-2.0 全文
├── include/
│   ├── WebView2.h          # 随源码分发的 WebView2 SDK 头文件
│   └── WebView2Loader.dll  # 随源码分发的加载器，构建时复制到 res/
├── res/
│   ├── app.rc              # 资源脚本：图标 + 清单 + 内嵌的 WebView2Loader.dll
│   ├── app.manifest        # comctl v6 / PerMonitorV2 DPI / UTF-8 代码页
│   ├── app.ico
│   └── app.res             # windres 生成（已进 .gitignore）
├── src/
│   ├── main.cpp            # wWinMain：解析命令行网址 → App::Init → Run
│   ├── app.h/.cpp          # 应用主体：无边框窗口、消息循环、定时器、桥接分发
│   ├── shell.h/.cpp        # 玻璃外壳：生成整页 HTML/CSS，增量推送 state
│   ├── theme.h/.cpp        # 设计令牌：调色板 + 外壳/起始页 CSS（配色唯一来源）
│   ├── webview.h/.cpp      # WebView2 封装：环境/控制器生命周期、导航、JS 注入、读 Cookie
│   ├── capture.h/.cpp      # 抓取编排：掩码/防抖/轮询、Token 汇总、登录态变更检测
│   ├── settings.h/.cpp     # 配置持久化：JSON 读写、默认值、内置网站预设
│   ├── logwriter.h/.cpp    # 日志：多格式、原子写、追加历史、进程内互斥
│   ├── util.h/.cpp         # 编码转换、时间格式、原子文件写、剪贴板、路径解析
│   ├── json.h              # 微型 JSON 解析/序列化（mjson）
│   └── ids.h               # 控件与资源 ID 集中定义（app.rc 和 C++ 共用）
└── test/
    ├── serve.py            # 本地测试服务器：静态页 + /api/login 返回带 token 的 JSON
    └── login.html          # 端到端验证用的登录页
```

> 早期那套基于原生 GDI 自绘的 `src/ui.*` 已经被玻璃外壳取代并删掉了，`build.bat` 也不再编它。

### 模块之间怎么依赖

```
main → App ─┬→ WebViewHost ────→ WebView2 COM（Content：承载被访问的网站）
            ├→ Shell ──────────→ WebView2 COM（外壳：画全部界面）
            ├→ CaptureManager ┬→ WebViewHost
            │                 └→ LogWriter / Settings
            ├→ Settings / LogWriter
            └→ theme（视觉令牌：原生窗口和外壳 CSS 共用）
```

WebView2 对外只通过几个 `std::function` 回调（`MessageFn` / `NavFn` / `CookiesFn` / `ReadyFn`）暴露事件，壳和宿主之间靠 `window.chrome.webview.postMessage`（JS → 宿主）和 `ExecuteScript("window.CG.state(...)")`（宿主 → JS）通信。**Cookie 和 Token 的值从不会进到外壳里**——外壳只拿到计数和状态文字。

## 几个关键设计

**单文件分发。** C/C++ 运行时静态链接，`WebView2Loader.dll` 以 RCDATA（ID 101）嵌进 exe。加载顺序是这样的：优先用 exe 同目录的 DLL，找不到才从资源里释放到 `%TEMP%` 再加载。所以随便扔个目录放一个 exe 就能跑。

**玻璃外壳双视图。** 外壳那个 WebView 铺满客户区，负责画环境光斑和各种视图；被导航的第三方站点由独立的 Content WebView（`App::webview_`）承载，位置按外壳上报的 `#webhole` 矩形来定位、盖在上面（两者是主窗口的兄弟子窗口，Content 在孔区那块 Z 序更高）。双向桥：JS 到宿主用 `window.chrome.webview.postMessage(JSON)`，统一信封 `{t:"...",...}`；宿主到 JS 用 `ExecuteScript("window.CG.state({...})")`。**Cookie / Token 不进外壳**，只把计数和状态推过去。

**配色只有一份。** 所有颜色都从 `theme::Current()` 的语义色取，外壳 CSS 和原生窗口共用同一套令牌（`theme::StartPageCss()`），从根上避免"两套配色对不上"的问题。绘制代码里不允许出现裸的 `RGB()` 字面量。

**原生窗口机制。** 用 `WM_NCCALCSIZE` 去掉非客户区实现无边框；最大化时按 `SM_CXSIZEFRAME + SM_CXPADDEDBORDER` 把客户区往里缩，免得内容溢出屏幕。拖动和缩放在外壳就绪后由 JS 桥接完成（`ReleaseCapture` + `WM_NCLBUTTONDOWN`，命中 `HTCAPTION` 或各边缘 `HT*`）；标题栏双击是外壳侧自己计时判断的——第二下直接切最大/还原，而且不进移动模态循环，否则会吃掉配对的 `mouseup`，浏览器就永远不派发 `dblclick` 了。

**错误处理。** 没有统一的异常框架：WebView2 的异步操作直接把 `HRESULT` 透传、在回调里消化；同步 API（`App::Init`、`WebViewHost::Create`）返回 `bool`；写日志走"部分失败可接受"策略，每种格式尽力写、错误文本累积到 `errOut`。弹窗只在三处出现：初始化失败、重复启动（单实例）、破坏性操作确认。其他运行期错误（不是 http 页、写日志失败、剪贴板复制失败等）都归到状态栏。WebView2 初始化失败不会往上抛，所以靠一个兜底定时器保证界面迟早会出来，并用就绪计数给个提示。

## 本地测试

```powershell
python test\serve.py            # 监听 http://127.0.0.1:8899
# 在应用地址栏打开 http://127.0.0.1:8899/ 并点页面上的「登 录」
```

`login.html` 一次会写入三类目标数据，正好用来验证三条捕获链路是不是都通了：

- **Cookie**：`session_id`、`csrf_token`
- **Web 存储**：`localStorage` 的 `access_token` / `refresh_token`、`sessionStorage` 的 `jwt`
- **请求头与响应体**：`POST /api/login` 带 `Authorization` / `X-Auth-Token`，响应体回 `access_token`、`refresh_token` 和嵌套的 `user.token`

## 想改点东西 / 提 PR

欢迎提 Issue 和 PR。为了保持架构和风格不乱，有几点最好照着来：

- **界面边界**：界面一律走 WebView2 玻璃外壳（`src/shell.*` 用 HTML/CSS 画），别又搞回原生 GDI 自绘控件；被访问的站点由 Content WebView2 承载，两边用 `postMessage` / `ExecuteScript` 通信。**Cookie / Token 值别进外壳的 DOM、`state` 或页面日志。**
- **配色和尺寸**：颜色统一取 `theme::Current()` 的语义色，别写裸 `RGB()`；原生绘制尺寸按 96dpi 逻辑像素写，再用 `App::S()` 缩放。
- **加文件**：新 `.cpp` 记得加进 `build.bat` 的 g++ 参数行。
- **验证**：提交前跑一遍 `.\build.bat` 确认编得过；动到抓取逻辑就用 `python test\serve.py` 加 `login.html` 做端到端验证。
- **提交信息**：写清楚干了啥，建议用 `feat:` / `fix:` / `docs:` / `refactor:` 这类前缀，顺带说下动机和影响面。
- **同步文档**：动了外壳 / 主题 / 抓取逻辑，记得同步更新 README 和 `.qoder/repowiki/knowledge` 里对应的知识卡片。

代码以 Apache-2.0 许可证（版权 © 空.城）发布，提 PR 即表示你同意按这个许可证并入。更完整的开发规范、提交和 PR 流程看 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 已知的限制

- 只支持 x64 Windows，没有交叉编译，也没有别的目标架构。
- 必须装 WebView2 Runtime；离线环境加载不了网页。
- 没有做代码签名，也没有安装包流水线，`build.bat` 只产出一个裸 exe（未签名程序 SmartScreen 可能会拦一下）。
- 依赖升级得手动替换 `include/WebView2.h` 和 `WebView2Loader.dll` 再重新提交。
- `.qoder/repowiki` 下的是工具生成的，可能跟不上最新代码。
