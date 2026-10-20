// 应用主体：无边框 Win32 宿主 + 全量 WebView2 外壳（标题栏/导航/工具栏/内容占位/
// 设置页/说明页/状态栏均由一个 shell WebView 渲染），中间浏览区为独立 WebView2。
#pragma once
#include <windows.h>
#include <string>
#include "webview.h"
#include "capture.h"
#include "settings.h"
#include "logwriter.h"

class App {
public:
    static App& Get() { static App inst; return inst; }
    bool Init(HINSTANCE hInst, const std::wstring& initialUrl = L"");
    int Run();
    bool IsWebViewReady() const;
    void OnWebViewReady();
    void ShowMainWindow();

    // ---------- 业务 ----------
    void OpenUrlDirect(const std::wstring& url);
    void CaptureNow(int mask);          // mask: 1=仅Cookie 2=仅Token
    void RefreshPage();
    void ClearLoginState();
    void SaveSettingsClicked();
    void OpenLogFolder();
    void SaveUiSettings();
    void OnCaptureStatus(const std::wstring& text);
    void ScheduleTimer(int id, int ms) { SetTimer(main_, id, ms, nullptr); }
    void OnTimer(int id);
    void KillCaptureTimers();
    void OnStartPageMessage(const std::wstring& msg);
    void ShowStartPage();
    void TitleBarButton(int i);         // 0=min 1=max/restore 2=close

    // ---------- 外壳 WebView ----------
    void InitShell();                    // 创建 shell WebView 并接入消息通道
    void OnShellReady();                 // shell 控制器就绪：导航到外壳 HTML
    void OnShellMessage(const std::string& json); // 外壳动作回调
    void PushShellState(bool includeAddress = false, bool includeForm = false); // C++ → shell；默认只推非输入字段，避免覆盖用户正在编辑的 address/loginUrl

    // ---------- 界面 ----------
    void SwitchPage(int page);
    void Layout();
    void ApplyWebSlot();                 // 按缓存的 slot 矩形定位/显隐 webHost_（切页/缩放/导航后重用）
    void CreateControls();
    void BuildBrushes();
    void ReleaseUiResources();
    void ShutdownWebView();

    int  S(int v) const;                 // DPI 缩放
    int  Dpi() const { return dpi_; }
    void SetDpi(int v) { dpi_ = v; }
    int  Page() const { return page_; }
    bool IsActive() const { return active_; }
    void SetActive(bool v) { active_ = v; }
    HANDLE SingleInstance() const { return singleInstance_; }
    void ReleaseSingleInstance() {
        if (singleInstance_) { CloseHandle(singleInstance_); singleInstance_ = nullptr; }
    }

    HWND main_ = nullptr;
    HBRUSH brBg_ = nullptr;              // 最外缩放边带的底色画刷（主窗口 WM_PAINT 用）

private:
    HINSTANCE inst_ = nullptr;
    HWND webHost_ = nullptr;             // 浏览区宿主（z-order 高于 shell）
    HWND shellHost_ = nullptr;           // 外壳宿主（铺满内缩边带）

    int page_ = 0;
    int dpi_ = 96;
    bool active_ = true;

    // shell 回报的 #webSlot 矩形（已换算为主窗口客户区物理像素，含 shellHost 内缩偏移）
    int slotX_ = 0, slotY_ = 0, slotW_ = 0, slotH_ = 0;
    bool slotValid_ = false;

    std::wstring caption_, statusText_, hintMsg_;
    std::wstring initialUrl_;

    // 设置页表单模型（HTML 采集回填的权威状态）
    bool toggleOn_[4] = { true, true, true, true }; // auto/clip/hist/first
    int  segSel_ = 0;                                // 0=字符串 1=JSON 2=两种都写
    int  pendingPresetIdx_ = 0;                      // 0=自定义
    std::wstring pendingLoginUrl_;

    bool startPageActive_ = true;
    bool shellReady_ = false;            // shell WebView 控制器已就绪
    int webviewFailChecks_ = 0;

    HANDLE singleInstance_ = nullptr;

    Settings settings_;
    WebViewHost webview_;                // 浏览区：起始页 / 目标网站
    WebViewHost shellWv_;                // 外壳：整个 UI chrome
    CaptureManager capture_;
    LogWriter logger_;
};
