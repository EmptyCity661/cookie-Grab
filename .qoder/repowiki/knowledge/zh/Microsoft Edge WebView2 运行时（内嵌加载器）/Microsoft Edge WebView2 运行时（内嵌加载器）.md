---
kind: external_dependency
name: Microsoft Edge WebView2 运行时（内嵌加载器）
slug: microsoft-edge-webview2
category: external_dependency
category_hints:
    - framework_behavior
    - sdk_real_api
scope:
    - '**'
---

### 身份与角色
- 本工具通过 Microsoft Edge WebView2 在 Win32 窗口中嵌入 Chromium 内核，用于抓取目标网站的 Cookie 与 Token。
- 头文件 `include/WebView2.h` 与动态库 `res/WebView2Loader.dll` 随项目分发；构建脚本 `build.bat` 将 Loader 复制到 `res/` 并参与资源编译。

### 集成方式
- 使用 COM 接口：`ICoreWebView2Environment` → `CreateCoreWebView2Controller` → `ICoreWebView2` / `ICoreWebView2_2`，并通过 `get_CookieManager` 访问 Cookie。
- 所有异步回调（环境创建、控制器创建、导航完成、WebMessage、GetCookies、ExecuteScript）均实现为独立的 COM handler 类（`EnvHandlerImpl`、`ControllerHandlerImpl`、`NavCompletedHandlerImpl`、`WebMessageHandlerImpl`、`GetCookiesHandlerImpl`、`ExecScriptHandlerImpl`），手动 `AddRef`/`Release` 管理生命周期。
- 注入一段 JS Hook（`TokenHookScript`）拦截 `fetch`、`XMLHttpRequest`、`localStorage`/`sessionStorage`，通过 `window.chrome.webview.postMessage` 回传 token 片段。

### 关键约束
- WebView2 运行时必须已在目标机器上安装（Edge/Chromium WebView2 Runtime）；Loader 只是进程内桥接，不替代运行时。
- `Recreate` 会删除整个 `userDataDir` 并重建环境，期间对已加载的 Loader DLL 不可覆盖写（注释明确说明需复用已加载句柄）。
- Cookie 读取走 `ICoreWebView2CookieList` 异步枚举，不是直接读文件系统。

### 验证提示
- 若 WebView2 未安装或版本过旧，`CreateCoreWebView2EnvironmentWithOptions` 会失败；确认系统 WebView2 Runtime 可用。