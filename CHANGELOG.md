# 更新日志

记一下这个项目比较大的改动。格式大致按 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)，版本号参考[语义化版本](https://semver.org/lang/zh-CN/)。

> 顺带说一句：GitHub 上的发布标签（比如 `v1.0.0`）和 exe 文件属性里那个版本号（`3.0.0.0`）是两回事，别混了。应用显示名是 `cookie抓取工具3.0`。

## [1.0.0] - 2026-10-21

第一个正式版。

### 新增
- 基于 Win32 + WebView2 的 Cookie / Token 抓取工具：在程序里打开任意网站登录，把 Cookie 和 Token 抓下来。
- 玻璃外壳界面：标题栏、顶部导航、工具栏、内容区、设置页、说明页、状态栏全是用 WebView2 画出来的；深浅主题、Per-Monitor V2 的 DPI、无边框窗口、标题栏双击最大/还原都在。
- 抓取能力：
  - 自动抓取（检测登录态变化）和首次打开即抓取；
  - 防抖合并，避免短时间多个请求互相覆盖；
  - Token 三通道：`localStorage` / `sessionStorage`、请求头、JSON 响应体；
  - "立即抓取 Cookie" 和 "立即抓取 Token" 是独立的两个按钮，各抓各的；
  - 清除登录状态（删 Cookie + 重建用户数据目录）。
- 输出：日志格式可选字符串 / JSON / 两种都写；`cookie_history.log` 追加历史；抓完能复制到剪贴板。
- 8 个网站预设：百度网盘、阿里云盘、夸克网盘、天翼云盘、迅雷网盘、115网盘、B站、GitHub。
- 单文件分发：静态链接，`WebView2Loader.dll` 内嵌，拷一个 exe 就能跑。
- 工程化：`build.bat` 一键构建、`finalize.ps1` 处理中文资源编码和产物命名、Apache-2.0 许可证、贡献指南，以及这份更新日志。

### 已知限制
- 只支持 x64 Windows，没有交叉编译，也没有别的目标架构。
- 必须装 WebView2 Runtime；离线环境加载不了网页。
- 没有代码签名、也没有安装包流水线，`build.bat` 只产出一个裸 exe（SmartScreen 可能对未签名程序拦一下）。
- WebView2 依赖升级得手动替换 `include/WebView2.h` 和 `WebView2Loader.dll`。
- `.qoder/repowiki` 下的是工具生成的，可能跟不上最新代码。

[1.0.0]: https://github.com/EmptyCity661/cookie-Grab/releases/tag/v1.0.0
