---
kind: error_handling
name: Win32/WebView2 桌面应用的错误处理模式
category: error_handling
scope:
    - '**'
source_files:
    - src/webview.cpp
    - src/main.cpp
    - src/logwriter.cpp
    - src/util.h
---

## 1. 总体方式

本仓库是一个 Win32 + WebView2 C++ 桌面应用，没有统一的异常框架、错误码枚举或中间件层。错误处理以三种互补的模式散落在各模块中：
- **HRESULT 回调**：所有 WebView2 COM 异步操作通过 `ICoreWebView2*CompletedHandler` 接口返回 `HRESULT errorCode`，调用方在 `Invoke` 中检查 `SUCCEEDED(hr)` / `FAILED(hr)`。
- **布尔返回值**：C++ 侧的同步 API（如 `WebViewHost::Create`）直接返回 `bool`，失败路径 `return false`。
- **Windows API 错误**：文件/目录/注册表等系统调用通过 `SetLastError`/`GetLastError` 间接使用（见 `util.cpp` 中的 `WriteFileAtomic`、`DeleteDirRecursive` 等），上层通过 `errOut` 字符串累积用户可读的错误信息。

没有发现全局的 `try/catch` 包裹整个程序；唯一的 C++ `try/catch` 出现在 `webview.cpp` 的内嵌 JS 钩子脚本中，用于让浏览器端 JavaScript 的异常静默吞掉，避免影响抓取流程。

## 2. 关键文件与位置

| 文件 | 作用 |
|---|---|
| `src/webview.cpp` | WebView2 COM 回调处理器（`EnvHandlerImpl`、`ControllerHandlerImpl`、`GetCookiesHandlerImpl` 等），全部以 `HRESULT errorCode` 参数传递错误 |
| `src/main.cpp` | 入口 `wWinMain`，初始化失败时弹出 `MessageBoxW` 并返回 1 |
| `src/logwriter.cpp` | 日志写入失败时通过 `fail()` lambda 将错误文本追加到 `errOut` 参数 |
| `src/util.h` / `src/util.cpp` | 提供 `WriteFileAtomic`、`DeleteDirRecursive` 等封装，内部使用 Windows API 错误码 |
| `src/app.h` / `src/app.cpp` | `App::Init` 返回 `bool`，由 `main.cpp` 消费 |

## 3. 架构与约定

### 3.1 WebView2 COM 回调的错误传播
每个 `Invoke(HRESULT errorCode, ...)` 实现都会把 `errorCode` 透传给 `WebViewHost` 的对应方法，例如：
```cpp
// webview.cpp:32
HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Environment* env) override {
    h_->OnEnvCreated(errorCode, env);
    return S_OK;
}
```
`OnEnvCreated` / `OnControllerCreated` / `OnCookiesReady` 等方法统一以 `if (FAILED(hr) || !ptr) return;` 的形式丢弃失败分支，不向上抛异常。

### 3.2 同步 API 用 `bool` 表示成功/失败
`WebViewHost::Create` 返回 `bool`，内部对 `CoInitializeEx`、`LoadLoaderDll`、`GetProcAddress` 等失败路径全部 `return false`。调用方 `main.cpp` 据此弹出错误对话框：
```cpp
if (!App::Get().Init(hInstance, initial)) {
    MessageBoxW(nullptr, L"程序初始化失败（WebView2 运行时可能未安装）", L"Cookie 抓取工具", MB_ICONERROR);
    return 1;
}
```

### 3.3 日志写入失败的“软失败”策略
`LogWriter::Write` 使用一个局部 `fail` lambda 把每个文件的写入失败记录到 `errOut` 字符串中，同时继续尝试后续输出格式（cookie.log → cookie.json → cookie_detail.json → cookie_history.log）。最终返回 `ok` 标志位给调用者。这是一种“尽力而为”的错误收集模式，而不是遇到第一个错误就中断。

### 3.4 WebView2 资源加载失败的处理
`LoadLoaderDll` 先尝试从 exe 同目录加载 `WebView2Loader.dll`，失败则从内嵌资源释放到 `%TEMP%\CookieGrabber\WebView2Loader.dll` 再加载。任一阶段失败都返回 `nullptr`，由 `Create` 转为 `false`。

### 3.5 浏览器端 JS 钩子的异常隔离
`TokenHookScript()` 返回的内嵌 JS 代码大量使用 `try { ... } catch(e){}` 包裹 `postMessage`、`localStorage/sessionStorage` 访问、`fetch` 拦截、`XMLHttpRequest` 重写等逻辑，确保单个站点或某个请求的异常不会污染其他站点的抓取。

## 4. 观察到的约定与约束

- **COM 回调不抛 C++ 异常**：所有 `ICoreWebView2*CompletedHandler::Invoke` 均返回 `S_OK`，错误通过 `HRESULT` 参数传递。
- **同步 API 用 `bool` 而非异常**：`WebViewHost::Create`、`App::Init` 等对外暴露的 C++ 函数以 `bool` 表达失败。
- **用户可见错误集中在入口**：只有 `main.cpp` 中调用 `MessageBoxW` 展示错误；UI 层的其他错误通常通过回调或日志回传。
- **日志写入采用“部分失败可接受”**：`LogWriter::Write` 允许部分文件格式写入失败，仅累积错误消息并返回 `ok=false`。
- **WebView2 对象生命周期由 Release 引用计数管理**：`Destroy` / `Recreate` 中按顺序 `Close()` → `Release()` → 置空指针，避免悬垂引用。
- **无全局 panic/recover**：未发现 `std::terminate`、`abort` 或跨模块的错误恢复机制。
- **无中间件层**：错误处理是内联在每个回调和函数中的，不存在统一的 middleware/error handler 抽象。

## 5. 缺失项

仓库中没有定义自定义异常类、错误码枚举、错误类型层次结构或错误上下文包装器。错误信息对用户呈现的方式非常有限——主要是 `MessageBoxW` 弹窗和可选的文件日志，没有结构化错误报告、重试策略或错误聚合器。