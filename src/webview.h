// WebView2 宿主封装：环境/控制器创建、导航、脚本注入、消息、Cookie 读取
// 加载器策略：优先 exe 同目录 WebView2Loader.dll；否则释放内嵌资源到临时目录后加载
#pragma once
#include <windows.h>
#include <functional>
#include <string>
#include <vector>
#include "WebView2.h"
#include "logwriter.h"

class WebViewHost {
public:
    using MessageFn = std::function<void(const std::string& jsonUtf8)>;
    using NavFn = std::function<void(bool success, const std::wstring& url)>;
    using CookiesFn = std::function<void(std::vector<CookieItem>)>;

    ~WebViewHost() { Destroy(); }

    bool Create(HWND parent, const std::wstring& userDataDir);
    void Destroy();
    void Navigate(const std::wstring& url); // 控制器未就绪时挂起，就绪后自动执行
    void NavigateToString(const std::wstring& html); // 同上，HTML 内容导航（起始页）
    void Reload();                             // 刷新当前页面
    void DeleteAllCookies();                   // 删除当前配置所有 Cookie
    bool Recreate(const std::wstring& userDataDir); // 关闭并重建（删除数据目录后全新会话）
    void ExecuteScript(const std::wstring& js);
    void GetCookies(const std::wstring& uri, CookiesFn cb);
    void Resize(int x, int y, int w, int h);
    bool IsReady() const { return webview_ != nullptr; }
    std::wstring CurrentUrl();
    void Focus();

    void SetMessageHandler(MessageFn fn) { onMessage_ = std::move(fn); }
    void SetNavHandler(NavFn fn) { onNav_ = std::move(fn); }

    // 注入的 Token 捕获 JS（页面文档创建时执行）
    static const wchar_t* TokenHookScript();

private:
    void OnEnvCreated(HRESULT hr, ICoreWebView2Environment* env);
    void OnControllerCreated(HRESULT hr, ICoreWebView2Controller* controller);
    void OnNavCompleted(bool success);
    void OnWebMessage(const std::string& json);
    void OnCookiesReady(HRESULT hr, void* cookieList);

    HWND parent_ = nullptr;
    ICoreWebView2Environment* env_ = nullptr;
    ICoreWebView2Controller* controller_ = nullptr;
    ICoreWebView2* webview_ = nullptr;
    ICoreWebView2_2* webview2_ = nullptr;
    ICoreWebView2CookieManager* cookieManager_ = nullptr; // 异步 GetCookies 期间保持存活
    HMODULE loader_ = nullptr;
    std::wstring loaderPath_;      // 释放出的加载器路径（需在退出时清理引用）
    bool loaderFromTemp_ = false;
    std::wstring userDataDir_;
    MessageFn onMessage_;
    NavFn onNav_;
    CookiesFn cookiesCb_;
    std::wstring pendingNav_;
    std::wstring pendingNavToString_;
    EventRegistrationToken navToken_ = {};
    EventRegistrationToken msgToken_ = {};

    friend class EnvHandlerImpl;
    friend class ControllerHandlerImpl;
    friend class NavCompletedHandlerImpl;
    friend class WebMessageHandlerImpl;
    friend class GetCookiesHandlerImpl;
    friend class ExecScriptHandlerImpl;
};
