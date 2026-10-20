// 应用主体：Win32 无边框外壳 + WebView2 玻璃界面（shell 铺满客户区）。
// 原生仅保留窗口机制（拖动/缩放/贴边/阴影/最小化最大化）与消息循环、定时器、桥接分发；
// 全部 chrome（标题栏/导航/工具栏/状态栏/起始页/设置/说明）由 shell WebView 渲染。
#pragma once
#include <windows.h>
#include <string>
#include "webview.h"
#include "capture.h"
#include "settings.h"
#include "logwriter.h"
#include "shell.h"

class App {
public:
    static App& Get() { static App inst; return inst; }
    bool Init(HINSTANCE hInst, const std::wstring& initialUrl = L"");
    int Run();

    // ---------- 业务 ----------
    void OpenUrlDirect(const std::wstring& url); // 直接打开（起始页/预设/工具栏）
    void CaptureNow(int mask);                   // mask: 1=仅Cookie 2=仅Token
    void RefreshPage();                          // 刷新：重新加载页面并更新界面状态
    void ClearLoginState();                      // 清除登录状态：删除凭证/会话并重置 UI
    void OpenLogFolder();                        // 打开日志输出目录
    void OnCaptureStatus(const std::wstring& text);
    void ScheduleTimer(int id, int ms) { SetTimer(main_, id, ms, nullptr); }
    void OnTimer(int id);
    void KillCaptureTimers() { capture_.KillTimers(); }
    void OnStartPageMessage(const std::wstring& msg); // 起始页按钮回调
    void ShowStartPage();

    // ---------- 界面 ----------
    void SwitchPage(int page);
    void Layout();
    int  Page() const { return page_; }
    void UpdateStatus();                         // 状态变更后推送到 shell 状态栏
    int  S(int v) const { return MulDiv(v, dpi_, 96); }
    int  Dpi() const { return dpi_; }
    void SetDpi(int v) { dpi_ = v; }

    // ---------- WebView 就绪 / 启动过渡 ----------
    bool IsWebViewReady() const { return webview_.IsReady(); }
    bool ShellReady() const { return shellReady_; }   // shell 控制器是否已就绪（决定命中兜底）
    void OnWebViewReady();                       // Content WebView 就绪：结束启动过渡
    void ShowMainWindow();                       // 首次显示主窗口（幂等）

    // ---------- Shell 玻璃外壳桥 ----------
    void InitShell();                            // 创建 shell WebView 并接入消息通道
    void OnShellReady();                         // shell 控制器就绪：生成内容并推送状态
    void ShowShellHtml();                        // 生成并导航到整页玻璃外壳
    void OnShellMessage(const std::string& json);// JS→host 统一分发
    void PushShellState();                       // host→JS 增量推送运行态
    void ApplyContentLayout(double x, double y, double w, double h, double dpr); // 内容视口 rect

    Settings& UiSettings() { return settings_; }
    void ReleaseSingleInstance() {
        if (singleInstance_) { CloseHandle(singleInstance_); singleInstance_ = nullptr; }
    }
    void ShutdownWebView();

    HWND main_ = nullptr;

private:
    shell::State MakeShellState();               // 组装当前运行态快照

    HINSTANCE inst_ = nullptr;
    HWND webHost_ = nullptr;                     // Content WebView 宿主（承载被导航站点）
    HWND shellHost_ = nullptr;                   // Shell WebView 宿主（铺满客户区，全部 chrome）

    int page_ = 0;                               // 0 浏览器 / 1 设置 / 2 说明
    int dpi_ = 96;
    std::wstring caption_, statusText_, hintMsg_;
    std::wstring initialUrl_;
    bool startPageActive_ = true;                // 内容视口当前是否显示起始页
    bool shellReady_ = false;                    // shell 控制器是否已就绪
    int webviewFailChecks_ = 0;                  // WebView2 就绪检测计数
    HANDLE singleInstance_ = nullptr;            // 单实例互斥体

    // 内容视口最近一次上报的物理像素矩形（客户区坐标）
    int contentX_ = 0, contentY_ = 0, contentW_ = 0, contentH_ = 0;

    Settings settings_;
    WebViewHost webview_;                        // 内容浏览 + Token 钩子
    WebViewHost shellWv_;                        // 玻璃外壳 chrome
    CaptureManager capture_;
    LogWriter logger_;
};
