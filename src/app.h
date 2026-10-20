// 应用主体：Win32 GUI（现代化无边框窗口 / 响应式布局：浏览器页 + 设置页）、模块编排
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "ids.h"
#include "webview.h"
#include "capture.h"
#include "settings.h"
#include "logwriter.h"

// 响应式布局档位（按客户区宽度划分，DPI 已换算）
enum LayoutMode { LayoutExpanded = 0, LayoutCompact = 1, LayoutNarrow = 2 };

class App {
public:
    static App& Get() { static App inst; return inst; }
    bool Init(HINSTANCE hInst, const std::wstring& initialUrl = L"");
    int Run();
    bool IsWebViewReady() const;                   // WebView2 控制器是否已就绪（决定过渡绘制）
    void OnWebViewReady();                         // WebView2 就绪：结束启动过渡
    void ShowMainWindow();                         // 首次显示主窗口（幂等）

    // ---------- 业务 ----------
    void OpenUrl();                              // 从地址框取 URL 并打开
    void OpenUrlDirect(const std::wstring& url); // 直接打开（起始页/预设）
    void CaptureNow(int mask);                   // mask: 1=仅Cookie 2=仅Token
    void RefreshPage();                          // 刷新：重新加载页面并更新界面状态
    void ClearLoginState();                      // 清除登录状态：删除凭证/会话并重置 UI
    void RefreshPresetList();
    void OnPresetComboSel();                     // 预设下拉切换：回填登录网址
    void SaveSettingsClicked();                  // 「保存设置」按钮
    void OpenLogFolder();                        // 「打开日志文件夹」按钮
    void ClearHint() { hintMsg_.clear(); }
    void SaveUiSettings();
    void OnCaptureStatus(const std::wstring& text);
    void ScheduleTimer(int id, int ms) { SetTimer(main_, id, ms, nullptr); }
    void OnTimer(int id);
    void KillCaptureTimers();
    void OnStartPageMessage(const std::wstring& msg); // 起始页按钮回调
    void ShowStartPage();

    // ---------- 界面 ----------
    void SwitchPage(int page);
    void Layout();
    void CreateControls();
    void BuildFonts();
    void BuildBrushes();
    void ReleaseUiResources();
    void ApplyFonts();
    void EnsureMemBuffer(HDC ref, int w, int h); // 双缓冲缓存（尺寸变化才重建）
    void FreeMemBuffer();
    void ShutdownWebView();
    HDC  MemDC() const { return memDC_; }
    HBITMAP MemBmp() const { return memBmp_; }
    HANDLE SingleInstance() const { return singleInstance_; }
    void ReleaseSingleInstance() {
        if (singleInstance_) { CloseHandle(singleInstance_); singleInstance_ = nullptr; }
    }
    void UpdateStatus();
    int  S(int v) const;                          // DPI 缩放
    int  Dpi() const { return dpi_; }
    void SetDpi(int v) { dpi_ = v; }
    LayoutMode Mode() const { return mode_; }
    int  Page() const { return page_; }

    // 绘制资源访问（供窗口过程与绘制辅助使用）
    HFONT FontTitle() const { return fTitle_; }
    HFONT FontNav() const { return fNav_; }
    HFONT FontNavB() const { return fNavB_; }
    HFONT FontBody() const { return fBody_; }
    HFONT FontCaption() const { return fCaption_; }
    HFONT FontBtn() const { return fBtn_; }
    HFONT FontCard() const { return fCard_; }
    HFONT FontMdl2() const { return fMdl2_; }
    HBRUSH BrushAppBg() const { return brAppBg_; }
    HBRUSH BrushSurface() const { return brSurface_; }
    HBRUSH BrushSidebar() const { return brSidebar_; }
    HBRUSH BrushBorder() const { return brBorder_; }
    HBRUSH BrushTitleInact() const { return brTitleInact_; }
    HBRUSH BrushAccentTint() const { return brAccentTint_; }
    HBRUSH BrushHover() const { return brHover_; }
    HBRUSH BrushDangerTint() const { return brDangerTint_; }
    HPEN   PenBorder() const { return penBorder_; }

    // 标题栏按钮
    int   TitleBtnHot() const { return tbHot_; }
    int   TitleBtnPress() const { return tbPress_; }
    RECT  TitleBtnRect(int i) const { return tbRects_[i]; }
    void  SetTitleBtnHot(int i) { tbHot_ = i; }
    void  SetTitleBtnPress(int i) { tbPress_ = i; }
    void  TitleBarButton(int i);                  // 0=min 1=max/restore 2=close
    bool  IsActive() const { return active_; }
    void  SetActive(bool v) { active_ = v; }
    int   TitleH() const { return titleH_; }
    int   SideW() const { return sideW_; }
    int   TopNavH() const { return topNavH_; }

    // 悬停跟踪（自绘控件）
    HWND  HoverCtl() const { return hoverCtl_; }
    void  SetHoverCtl(HWND h) { hoverCtl_ = h; }
    bool  Tracking() const { return tracking_; }
    void  SetTracking(bool v) { tracking_ = v; }

    // 焦点提示：仅键盘导航时显示（系统 DrawFocusRect 的点状直角框在圆角按钮上会露出直角，
    // 看起来像“被裁掉的影子边”，鼠标点击后不应出现）
    bool  FocusCueOn() const { return focusCue_; }
    void  SetFocusCue(bool on);

    // 设置页滚动
    int   SettingsScroll() const { return settingsScroll_; }
    void  SetSettingsScroll(int v);
    int   SettingsContentH() const { return settingsContentH_; }
    int   SettingsViewH() const { return settingsViewH_; }
    RECT  SettingsViewRect() const { return settingsView_; }
    bool  ScrollBarHot() const { return sbHot_; }
    void  SetScrollBarHot(bool v) { sbHot_ = v; }
    bool  ScrollBarDragging() const { return sbDrag_; }
    void  SetScrollBarDragging(bool v) { sbDrag_ = v; }
    int   SbDragOffset() const { return sbDragOffset_; }
    void  SetSbDragOffset(int v) { sbDragOffset_ = v; }

    // 状态信息（状态行绘制）
    const std::wstring& CaptionText() const { return caption_; }
    const std::wstring& StatusText() const { return statusText_; }
    const std::wstring& HintText() const { return hintMsg_; }
    // 开关/分段状态（自管理，不依赖 button check 行为）
    bool ToggleOn(int i) const { return toggleOn_[i]; }
    void SetToggleOn(int i, bool v) { toggleOn_[i] = v; }
    int  SegSel() const { return segSel_; }
    void SetSegSel(int v) { segSel_ = v; }
    int   CookieCount() const { return capture_.CookieCount(); }
    int   TokenCount() const { return capture_.TokenCount(); }
    const std::wstring& LogDir() const { return logger_.Dir(); }

    Settings& UiSettings() { return settings_; }

    // 布局矩形（PaintMain 使用）
    RECT toolbarCard_, webCard_, statusLine_;
    std::vector<RECT> cardRects_;

    HWND main_ = nullptr;

private:
    HINSTANCE inst_ = nullptr;
    // 控件
    HWND navBrowser_ = nullptr, navSettings_ = nullptr, navHelp_ = nullptr;
    HWND urlCombo_ = nullptr, btnOpen_ = nullptr, btnRefresh_ = nullptr, btnCapture_ = nullptr;
    HWND btnCaptureTkn_ = nullptr, webHost_ = nullptr;
    HWND presetCombo_ = nullptr, loginUrlEdit_ = nullptr, logFileEdit_ = nullptr;
    HWND btnSave_ = nullptr, btnOpenLog_ = nullptr;
    HWND chkAuto_ = nullptr, chkClip_ = nullptr, chkHist_ = nullptr, chkFirst_ = nullptr;
    HWND radStr_ = nullptr, radJson_ = nullptr, radBoth_ = nullptr;
    HWND btnClear_ = nullptr;
    // 字体/画刷
    HFONT fTitle_ = nullptr, fNav_ = nullptr, fNavB_ = nullptr, fBody_ = nullptr, fCaption_ = nullptr;
    HFONT fBtn_ = nullptr, fCard_ = nullptr, fMdl2_ = nullptr;
    HBRUSH brAppBg_ = nullptr, brSurface_ = nullptr, brSidebar_ = nullptr, brBorder_ = nullptr;
    HBRUSH brTitleInact_ = nullptr, brAccentTint_ = nullptr, brHover_ = nullptr, brDangerTint_ = nullptr;
    HPEN penBorder_ = nullptr;
    // 布局
    LayoutMode mode_ = LayoutExpanded;
    int page_ = 0;
    int sideW_ = 0, topNavH_ = 0, titleH_ = 0, dpi_ = 96;
    RECT tbRects_[3] = {};     // 标题栏按钮 0=min 1=max 2=close
    int tbHot_ = -1, tbPress_ = -1;
    bool active_ = true;
    bool tracking_ = false;
    HWND hoverCtl_ = nullptr;
    bool focusCue_ = false;      // 焦点提示可见：仅键盘 Tab/方向键导航后为 true
    // 设置页
    int settingsScroll_ = 0, settingsContentH_ = 0, settingsViewH_ = 0;
    RECT settingsView_ = {};
    bool sbHot_ = false, sbDrag_ = false;
    int sbDragOffset_ = 0;
    // 状态
    std::wstring caption_, statusText_, hintMsg_;
    std::wstring initialUrl_;
    bool toggleOn_[4] = { true, true, true, true }; // 自动抓取/剪贴板/历史/首次打开
    int segSel_ = 0;                                // 日志格式 0=字符串 1=JSON 2=两种都写
    bool startPageActive_ = true;                   // 当前是否显示起始页
    int webviewFailChecks_ = 0;                     // WebView2 就绪检测计数
    HANDLE singleInstance_ = nullptr;               // 单实例互斥体
    // 双缓冲缓存
    HDC memDC_ = nullptr;
    HBITMAP memBmp_ = nullptr;
    int memW_ = 0, memH_ = 0;
    Settings settings_;
    WebViewHost webview_;
    CaptureManager capture_;
    LogWriter logger_;
};