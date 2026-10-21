# 更新日志

本文件记录本项目的 notable 变更，遵循 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/) 与[语义化版本](https://semver.org/lang/zh-CN/)。

> 说明：仓库的**发布标签**（如 `v1.0.0`）与**应用内部版本号**（当前 `3.0.0.0`，见文件属性）相互独立；应用显示名为 `cookie抓取工具3.0`。

## [1.0.0] - 2026-10-21

首个正式发布版本。

### 新增
- 基于 Win32 + WebView2 的 Cookie / Token 抓取工具：内嵌浏览器登录任意网站，提取该站点的 Cookie 与 Token。
- WebView2 玻璃外壳界面：标题栏 / 顶部导航 / 工具栏 / 内容视口 / 设置页 / 说明页 / 状态栏全部由 HTML/CSS 渲染；深浅主题、Per-Monitor V2 DPI、无边框窗口与标题栏双击最大化/还原。
- 抓取能力：
  - 自动抓取（登录态变化检测）与首次打开即抓取；
  - 防抖合并，避免短时间内多次请求互相覆盖；
  - Token 三通道：`localStorage` / `sessionStorage`、请求头、JSON 响应体；
  - 「立即抓取 Cookie」与「立即抓取 Token」独立按钮，互不影响；
  - 清除登录状态（删除 Cookie 并重建用户数据目录）。
- 输出：日志格式可选 `字符串` / `JSON` / `两种都写`；`cookie_history.log` 追加历史；抓取后可复制到剪贴板。
- 网站预设：百度网盘、阿里云盘、夸克网盘、天翼云盘、迅雷网盘、115网盘、B站、GitHub。
- 单文件分发：静态链接，`WebView2Loader.dll` 内嵌，拷贝一个 exe 即可运行。
- 工程化：`build.bat` 一键构建、`finalize.ps1` 处理中文资源编码与产物命名、Apache-2.0 许可证、贡献指南与本更新日志。

### 已知限制
- 仅支持 x64 Windows，无交叉编译或其他架构目标。
- 需要系统已安装 Microsoft Edge WebView2 Runtime；离线环境无法加载网页。
- 无代码签名 / 安装包流水线，`build.bat` 只产出裸 exe（SmartScreen 可能对未签名程序提示）。
- WebView2 依赖升级需手动替换 `include/WebView2.h` 与 `WebView2Loader.dll`。
- `.qoder/repowiki` 的分层 Wiki 为工具生成物，可能与最新代码存在滞后。

[1.0.0]: https://github.com/wudicyg/cookie-Grab/releases/tag/v1.0.0
