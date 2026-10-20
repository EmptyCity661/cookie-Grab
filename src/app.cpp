// 应用主体：无边框 Win32 宿主 + 全量 WebView2 外壳。
// UI chrome（标题栏/导航/工具栏/内容占位/设置/说明/状态栏）由一个 shell WebView 渲染；
// 中间浏览区为独立的 webview WebView，二者共用一个 userdata 目录。
// 消息通道：shell ⇄ C++（{sh:...}/{tb:...} ↔ __cgUpdate）；起始页 ⇄ C++（{cg:'nav'}）。
#include "app.h"
#include "util.h"
#include "theme.h"
#include "shellhtml.h"
#include "json.h"
#include <commctrl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <cstdio>
#include <vector>
#include <string>

#pragma comment(lib, "dwmapi.lib")

static const wchar_t kMainClass[] = L"CGMainWndClass";
static const wchar_t kHostClass[] = L"CGWebHostClass";
static App* g_app = nullptr;

// 定时器 / 自定义消息
static const int TIMER_LOGIN_POLL = 1;
static const int TIMER_SHOW_FALLBACK = 4;
static const UINT WM_APP_WEBVIEW_READY = WM_APP + 1;
static const UINT WM_APP_SHELL_READY = WM_APP + 2;
static const UINT WM_APP_DRAG = WM_APP + 3;

static inline int PtX(LPARAM lp) { return (int)(short)LOWORD(lp); }
static inline int PtY(LPARAM lp) { return (int)(short)HIWORD(lp); }

static LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK HostWndProc(HWND, UINT, WPARAM, LPARAM);

// ==================== App ====================
bool App::Init(HINSTANCE hInst, const std::wstring& initialUrl) {
    inst_ = hInst;
    initialUrl_ = initialUrl;
    g_app = this;
    // 单实例：两个进程共享 WebView2 userdata 目录会冲突
    singleInstance_ = CreateMutexW(nullptr, TRUE, L"Local\\CookieGrabberSingleInstance");
    if (singleInstance_ && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND exist = FindWindowW(kMainClass, nullptr);
        if (exist) {
            ShowWindow(exist, SW_RESTORE);
            SetForegroundWindow(exist);
        } else {
            MessageBoxW(nullptr, L"Cookie 抓取工具已在运行中。", L"提示", MB_ICONINFORMATION);
        }
        CloseHandle(singleInstance_);
        singleInstance_ = nullptr;
        return false;
    }
    // COM 必须先初始化：SHGetKnownFolderPath / WebView2 均依赖 STA
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hrCo) && hrCo != RPC_E_CHANGED_MODE) return false;
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = kMainClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(100));
    if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    RegisterClassExW(&wc);

    wc.lpfnWndProc = HostWndProc;
    wc.lpszClassName = kHostClass;
    wc.hbrBackground = nullptr;
    wc.hIcon = nullptr;
    wc.hIconSm = nullptr;
    RegisterClassExW(&wc);

    // 主题解析必须早于窗口创建：WM_CREATE 即用调色板建画刷；shell HTML 也依赖同源变量
    theme::Resolve(theme::DetectDarkMode());

    settings_ = LoadSettings();
    logger_.Init();

    // 表单模型初值（shell 就绪时随首次 PushShellState 一起回填）
    toggleOn_[0] = settings_.autoCapture;
    toggleOn_[1] = settings_.clipboard;
    toggleOn_[2] = settings_.history;
    toggleOn_[3] = settings_.firstOpenCapture;
    segSel_ = settings_.logFormat == LogFormat::Json ? 1
            : (settings_.logFormat == LogFormat::Both ? 2 : 0);
    pendingPresetIdx_ = 0;
    pendingLoginUrl_.clear();

    main_ = CreateWindowExW(0, kMainClass, L"Cookie 抓取工具",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 1180, 780,
        nullptr, nullptr, hInst, nullptr);
    if (!main_) return false;

    // Win11 圆角 + 边框色（旧系统静默失败）
    DWMWINDOWATTRIBUTE attrCorner = (DWMWINDOWATTRIBUTE)33; // DWMWA_WINDOW_CORNER_PREFERENCE
    int cornerPref = 2;                                     // DWMWCP_ROUND
    DwmSetWindowAttribute(main_, attrCorner, &cornerPref, sizeof(cornerPref));
    DWMWINDOWATTRIBUTE attrBorder = (DWMWINDOWATTRIBUTE)34; // DWMWA_BORDER_COLOR
    COLORREF borderClr = theme::Current().border;
    DwmSetWindowAttribute(main_, attrBorder, &borderClr, sizeof(borderClr));
    // 向 DWM 声明一层极薄的玻璃框架以恢复窗口外围投影（drop shadow）：
    // 本窗口把非客户区压成 0（见 WM_NCCALCSIZE），DWM 会按 borderless 处理而不画阴影；
    // bottom=1 足以触发投影，且整客户区被 shell/host 底色覆盖，不会露出透明/白边
    MARGINS glass = { 0, 0, 0, 1 };
    DwmExtendFrameIntoClientArea(main_, &glass);

    // 窗口居中
    RECT wa{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    UINT dpi = GetDpiForWindow(main_);
    int w = MulDiv(1180, dpi, 96), h = MulDiv(780, dpi, 96);
    int x = wa.left + ((wa.right - wa.left) - w) / 2;
    int y = wa.top + ((wa.bottom - wa.top) - h) / 2;
    SetWindowPos(main_, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    SetTimer(main_, TIMER_LOGIN_POLL, 1500, nullptr); // 登录态轮询

    // 浏览区：起始页 cg 消息优先，其余交 capture_ 处理（Token 上报等）
    webview_.SetMessageHandler([this](const std::string& json) {
        std::wstring wj = util::Utf8ToWide(json);
        if (wj.find(L"\"cg\"") != std::wstring::npos) { OnStartPageMessage(wj); return; }
        capture_.OnWebMessage(json);
    });
    webview_.SetNavHandler([this](bool ok, const std::wstring& url) {
        capture_.OnNavigationCompleted(ok);
        bool realPage = !url.empty() &&
            (util::StartsWithI(url, L"http://") || util::StartsWithI(url, L"https://") || util::StartsWithI(url, L"file://"));
        if (realPage) caption_ = CaptureManager::HostFromUrl(url);
        else caption_.clear();
        // 导航完成后同步标题与地址（用户输入已提交，可以安全回填）
        PushShellState(true);
    });
    webview_.SetReadyHandler([this]() {
        if (main_) PostMessageW(main_, WM_APP_WEBVIEW_READY, 0, 0);
    });
    std::wstring userData = util::LocalAppDir() + L"\\userdata";
    webview_.Create(webHost_, userData);

    // 外壳 WebView：与浏览区共用 userdata 目录，WebView2Loader 内部按引用计数共享环境
    InitShell();

    capture_.Init(&webview_, &settings_, &logger_,
        [this](const std::wstring& t) { OnCaptureStatus(t); },
        [this](int id, int ms) { ScheduleTimer(id, ms); });

    if (!initialUrl_.empty()) webview_.Navigate(initialUrl_);
    else ShowStartPage();

    // 启动不立即显示窗口：WebView2 异步初始化，先显示会露出未绘制区域（黑/空白）。
    // 由浏览区就绪回调显示；shell 若尚未就绪，其宿主自绘 canvas 底色与 HTML body 同色，
    // 视觉无缝；超时兜底保证运行时缺失时界面仍会出现。
    SetTimer(main_, TIMER_SHOW_FALLBACK, 500, nullptr);
    return true;
}

int App::Run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

int App::S(int v) const { return MulDiv(v, dpi_, 96); }

void App::BuildBrushes() {
    brBg_ = CreateSolidBrush(theme::Current().canvas);
}

void App::ReleaseUiResources() {
    if (brBg_) { DeleteObject(brBg_); brBg_ = nullptr; }
}

void App::ShutdownWebView() {
    // 先销毁 shell，再销毁浏览区；加载器按引用计数归还
    shellWv_.Destroy();
    webview_.Destroy();
}

bool App::IsWebViewReady() const { return webview_.IsReady(); }

void App::ShowMainWindow() {
    if (!main_ || IsWindowVisible(main_)) return;
    ShowWindow(main_, SW_SHOW);
    // 显示后立即完成首帧（含子窗口）绘制：否则窗口出现瞬间会先闪一帧空白
    RedrawWindow(main_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

void App::OnWebViewReady() {
    KillTimer(main_, TIMER_SHOW_FALLBACK);
    ShowMainWindow();
}

// ==================== 控件与外壳 ====================
void App::CreateControls() {
    // z-order：shellHost 先创建 → 位置在兄弟链底部；webHost 后创建 → 顶部，
    // 覆盖 shell 上的 #webSlot 视口（切页隐藏 webHost 时，shell 完整可见）
    // shellHost 从一开始就应可见（它就是整个 UI chrome）；webHost 默认隐藏，
    // 等 shell 报上 slot 矩形后再 ShowWindow。
    shellHost_ = CreateWindowExW(0, kHostClass, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 10, 10, main_, nullptr, inst_, nullptr);
    webHost_ = CreateWindowExW(0, kHostClass, L"", WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 10, 10, main_, nullptr, inst_, nullptr);
    // 显式把 webHost_ 置于 shellHost_ 之上（创建顺序已如此，再确一次防 WebView2 初始化重排）
    SetWindowPos(webHost_, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    page_ = 0;
}

void App::InitShell() {
    if (!shellHost_) return;
    shellWv_.SetMessageHandler([this](const std::string& json) { OnShellMessage(json); });
    shellWv_.SetReadyHandler([this]() {
        if (main_) PostMessageW(main_, WM_APP_SHELL_READY, 0, 0);
    });
    std::wstring userData = util::LocalAppDir() + L"\\userdata";
    shellWv_.Create(shellHost_, userData);
}

void App::OnShellReady() {
    // 控制器就绪：注入外壳 HTML；HTML 加载完成后会 post {sh:'ready'} → 触发首次完整推送
    std::string html = shellhtml::Build();
    shellWv_.NavigateToString(util::Utf8ToWide(html));
}

// ==================== 布局 ====================
// shellHost 内缩 S(5)：最外环带由主窗口 WM_PAINT 用 canvas 画刷填充，
// WM_NCHITTEST 在该环带上返回 HTLEFT/HTRIGHT/HTTOP/HTBOTTOM/四角 → 系统缩放
void App::Layout() {
    if (!main_) return;
    RECT rc;
    GetClientRect(main_, &rc);
    int edge = S(5);
    int w = rc.right - edge * 2;
    int h = rc.bottom - edge * 2;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    MoveWindow(shellHost_, edge, edge, w, h, TRUE);
    if (shellWv_.IsReady()) shellWv_.Resize(0, 0, w, h);
    // webHost 的精确位置由 shell 的 #webSlot ResizeObserver 回报驱动；这里用缓存矩形
    // 重新确认一次（窗口缩放后 HTML 会重排并再报 slot，此处保证不短暂错位/失序）。
    ApplyWebSlot();
}

// 按缓存的 slot 矩形定位/显隐 webHost_：浏览页且矩形有效时置顶显示，否则隐藏。
void App::ApplyWebSlot() {
    if (!webHost_) return;
    if (page_ == 0 && slotValid_ && slotW_ > 0 && slotH_ > 0) {
        SetWindowPos(webHost_, HWND_TOP, slotX_, slotY_, slotW_, slotH_,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
        if (webview_.IsReady()) webview_.Resize(0, 0, slotW_, slotH_);
    } else {
        ShowWindow(webHost_, SW_HIDE);
    }
}

void App::SwitchPage(int page) {
    page_ = page;
    // 立即按页码显隐 webHost_（缓存矩形有效才显示），避免等 slot 往返期间残留覆盖设置/说明页；
    // shell 侧 data-page 切换后 #webSlot 尺寸变化会再报 slot 精确校正。
    ApplyWebSlot();
    PushShellState();
}

// ==================== C++ → shell 状态推送 ====================
// includeAddress=true：同步地址栏（只在导航完成/回到起始页/首次就绪时使用，
//   平时不推送，避免与用户正在编辑的 #address 打架）；
// includeForm=true：重推 presets + 设置表单初值（shell 首次就绪、预设变更、
//   保存设置、清除登录状态等"程序性回填"时机使用）；loginUrl 也属于文本输入，
//   只在 includeForm=true 时才写入，以免覆盖用户当下输入。
void App::PushShellState(bool includeAddress, bool includeForm) {
    if (!shellReady_ || !shellWv_.IsReady()) return;
    mjson::Writer w;
    w.BeginObject();
    w.Key("page"); w.Int(page_);
    w.Key("active"); w.Bool(active_);
    w.Key("maximized"); w.Bool(main_ && IsZoomed(main_) ? true : false);
    std::wstring title = L"Cookie 抓取工具";
    if (!caption_.empty()) title += L"   ·   " + caption_;
    w.Key("title"); w.Str(util::WideToUtf8(title));
    std::wstring st = statusText_.empty() ? L"就绪" : statusText_;
    w.Key("status"); w.Str(util::WideToUtf8(st));
    w.Key("cookieCount"); w.Int(capture_.CookieCount());
    w.Key("tokenCount"); w.Int(capture_.TokenCount());
    w.Key("logDir"); w.Str(util::WideToUtf8(logger_.Dir()));
    if (includeAddress) {
        std::wstring addr = startPageActive_ ? std::wstring() : webview_.CurrentUrl();
        w.Key("address"); w.Str(util::WideToUtf8(addr));
    }
    if (includeForm) {
        w.Key("presets");
        w.BeginArray();
        for (auto& p : settings_.presets) {
            w.BeginObject();
            w.Key("name"); w.Str(util::WideToUtf8(p.name));
            w.Key("url"); w.Str(util::WideToUtf8(p.url));
            w.EndObject();
        }
        w.EndArray();
        w.Key("form");
        w.BeginObject();
        w.Key("presetIdx"); w.Int(pendingPresetIdx_);
        w.Key("loginUrl"); w.Str(util::WideToUtf8(pendingLoginUrl_));
        w.Key("auto"); w.Bool(toggleOn_[0]);
        w.Key("clip"); w.Bool(toggleOn_[1]);
        w.Key("hist"); w.Bool(toggleOn_[2]);
        w.Key("first"); w.Bool(toggleOn_[3]);
        w.Key("seg"); w.Int(segSel_);
        w.Key("dataDir"); w.Str(util::WideToUtf8(logger_.Dir()));
        w.Key("logFile"); w.Str(util::WideToUtf8(L"cookie.log"));
        w.Key("hint"); w.Str(util::WideToUtf8(hintMsg_));
        w.EndObject();
    }
    w.EndObject();
    std::wstring js = L"if(window.__cgUpdate)__cgUpdate(" + util::Utf8ToWide(w.out) + L");";
    shellWv_.ExecuteScript(js);
}

// ==================== shell → C++ 消息 ====================
// {sh:'ready'|'page'|'drag'|'togglemax'|'min'|'max'|'close'|'save'|'openlog'|
//    'clearlogin'|'preset'|'loginurl'|'toggle'|'seg'|'slot', ...}
// {tb:'open'|'refresh'|'capcookie'|'captoken'|'regex', ...}
void App::OnShellMessage(const std::string& json) {
    auto v = mjson::Parse(json);
    if (!v || v->type != mjson::Value::Object) return;

    // 工具栏族：任何携带 re 字段的消息都先同步正则筛选到抓取后端（写盘时应用）
    if (v->Has("re")) {
        capture_.SetCookieFilter(util::Utf8ToWide(v->GetStr("re")));
    }
    std::string tb = v->GetStr("tb");
    if (!tb.empty()) {
        if (tb == "open") OpenUrlDirect(util::Utf8ToWide(v->GetStr("url")));
        else if (tb == "refresh") RefreshPage();
        else if (tb == "capcookie") CaptureNow(1);
        else if (tb == "captoken") CaptureNow(2);
        // tb == "regex"：仅更新筛选，无副作用
        return;
    }

    std::string sh = v->GetStr("sh");
    auto numAt = [&](const char* k, int def = 0) -> int {
        auto it = v->obj.find(k);
        if (it == v->obj.end() || it->second->type != mjson::Value::Number) return def;
        return (int)it->second->num;
    };

    if (sh == "ready") {
        shellReady_ = true;
        PushShellState(true, true);
        Layout();
    } else if (sh == "page") {
        SwitchPage(numAt("idx", 0));
    } else if (sh == "drag") {
        // 拖入系统移动模态：必须在主窗口过程里做，不能在 WebView2 回调栈内直接 SendMessage
        PostMessageW(main_, WM_APP_DRAG, 0, 0);
    } else if (sh == "togglemax") {
        TitleBarButton(1);
    } else if (sh == "min") {
        TitleBarButton(0);
    } else if (sh == "max") {
        TitleBarButton(1);
    } else if (sh == "close") {
        TitleBarButton(2);
    } else if (sh == "save") {
        SaveSettingsClicked();
    } else if (sh == "openlog") {
        OpenLogFolder();
    } else if (sh == "clearlogin") {
        ClearLoginState();
    } else if (sh == "preset") {
        int idx = numAt("idx", 0);
        pendingPresetIdx_ = idx;
        if (idx >= 1 && idx <= (int)settings_.presets.size())
            pendingLoginUrl_ = settings_.presets[idx - 1].url;
        else
            pendingLoginUrl_.clear();
        hintMsg_.clear();
        PushShellState(false, true);
    } else if (sh == "loginurl") {
        pendingLoginUrl_ = util::Utf8ToWide(v->GetStr("text"));
        hintMsg_.clear();
        // 不推送：避免覆盖用户正在编辑的文本
    } else if (sh == "toggle") {
        std::string field = v->GetStr("field");
        bool on = v->GetBool("on", false);
        int idx = field == "auto" ? 0 : field == "clip" ? 1 : field == "hist" ? 2 : field == "first" ? 3 : -1;
        if (idx >= 0) {
            toggleOn_[idx] = on;
            SaveUiSettings();
            hintMsg_.clear();
        }
    } else if (sh == "seg") {
        int idx = numAt("idx", 0);
        if (idx < 0 || idx > 2) idx = 0;
        segSel_ = idx;
        SaveUiSettings();
        hintMsg_.clear();
    } else if (sh == "slot") {
        int inset = S(5);
        slotW_ = numAt("w");
        slotH_ = numAt("h");
        slotX_ = inset + numAt("x");
        slotY_ = inset + numAt("y");
        slotValid_ = (slotW_ > 0 && slotH_ > 0);
        ApplyWebSlot();
    }
}

// ==================== 业务方法 ====================
void App::OpenUrlDirect(const std::wstring& urlIn) {
    std::wstring url = util::Trim(urlIn);
    if (url.empty()) return;
    if (!util::StartsWithI(url, L"http://") && !util::StartsWithI(url, L"https://") && !util::StartsWithI(url, L"file://"))
        url = L"https://" + url;
    startPageActive_ = false;
    caption_ = CaptureManager::HostFromUrl(url);
    SwitchPage(0);
    // 不在此处推 address：等导航完成回调里再推，避免中间态覆盖用户当下输入
    webview_.Navigate(url);
}

void App::CaptureNow(int mask) {
    // mask: 1=仅 Cookie 2=仅 Token；两个按钮互不影响（采集路径独立）
    capture_.RequestCapture(true, mask);
}

void App::RefreshPage() {
    SwitchPage(0);
    if (!webview_.IsReady()) {
        OnCaptureStatus(L"页面尚未加载完成，请稍候再刷新");
        return;
    }
    OnCaptureStatus(L"正在刷新…");
    webview_.Reload();
    // 地址框/标题/计数随 NavigationCompleted 回调自动更新
}

void App::ClearLoginState() {
    if (MessageBoxW(main_,
            L"确定要清除登录状态吗？\n\n将删除所有已保存的 Cookie、Token 与网站登录会话，之后需要重新登录。",
            L"清除登录状态", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
        return;
    }
    OnCaptureStatus(L"正在清除登录状态…");
    UpdateWindow(main_);

    if (webview_.IsReady()) {
        webview_.ExecuteScript(L"try{localStorage.clear();sessionStorage.clear();}catch(e){}");
        webview_.DeleteAllCookies();
    }

    std::wstring userData = util::LocalAppDir() + L"\\userdata";
    bool recreated = webview_.Recreate(userData);
    bool dirGone = !util::DirExists(userData);

    capture_.ResetState();
    caption_.clear();
    if (recreated) {
        ShowStartPage();
        if (dirGone) OnCaptureStatus(L"登录状态已清除，请重新登录");
        else OnCaptureStatus(L"登录状态已清除（数据目录有残留，重启应用后可完全清除）");
    } else {
        OnCaptureStatus(L"清除失败：WebView2 重建失败，请重启应用后重试");
    }
    PushShellState(true, true);
}

void App::SaveSettingsClicked() {
    SaveUiSettings();
    // 预设选中且登录网址被修改：持久化到该预设
    if (pendingPresetIdx_ >= 1 && pendingPresetIdx_ <= (int)settings_.presets.size()) {
        std::wstring url = util::Trim(pendingLoginUrl_);
        if (!url.empty()) {
            if (!util::StartsWithI(url, L"http://") && !util::StartsWithI(url, L"https://") && !util::StartsWithI(url, L"file://"))
                url = L"https://" + url;
            if (url != settings_.presets[pendingPresetIdx_ - 1].url) {
                settings_.presets[pendingPresetIdx_ - 1].url = url;
                SaveSettings(settings_);
                pendingLoginUrl_ = url;
                if (startPageActive_) ShowStartPage();
            }
        }
    }
    hintMsg_ = L"设置已保存，立即生效。";
    PushShellState(false, true);
}

void App::OpenLogFolder() {
    ShellExecuteW(main_, L"open", logger_.Dir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void App::SaveUiSettings() {
    settings_.autoCapture = toggleOn_[0];
    settings_.clipboard = toggleOn_[1];
    settings_.history = toggleOn_[2];
    settings_.firstOpenCapture = toggleOn_[3];
    settings_.logFormat = segSel_ == 1 ? LogFormat::Json : (segSel_ == 2 ? LogFormat::Both : LogFormat::String);
    SaveSettings(settings_);
}

void App::OnCaptureStatus(const std::wstring& text) {
    statusText_ = text;
    PushShellState();
}

void App::OnTimer(int id) {
    switch (id) {
    case TIMER_LOGIN_POLL:
        capture_.PollTick();
        // WebView2 就绪检测：连续多次未就绪则提示（异步创建失败时用户无感知）
        if (!webview_.IsReady() && webviewFailChecks_ < 8) {
            webviewFailChecks_++;
            if (webviewFailChecks_ >= 8) {
                OnCaptureStatus(L"WebView2 初始化失败：请安装 Edge WebView2 运行时后重试");
            }
        }
        break;
    case 2: KillTimer(main_, 2); capture_.FirePending(); break;
    case 3: KillTimer(main_, 3); capture_.FireDelayedStep(); break;
    case TIMER_SHOW_FALLBACK:
        // 兜底：WebView2 迟迟不就绪（未安装运行时/创建失败）也必须让界面可见
        KillTimer(main_, TIMER_SHOW_FALLBACK);
        ShowMainWindow();
        break;
    }
}

void App::KillCaptureTimers() { capture_.KillTimers(); }

void App::TitleBarButton(int i) {
    if (i == 0) ShowWindow(main_, SW_MINIMIZE);
    else if (i == 1) ShowWindow(main_, IsZoomed(main_) ? SW_RESTORE : SW_MAXIMIZE);
    else if (i == 2) PostMessageW(main_, WM_CLOSE, 0, 0);
}

void App::OnStartPageMessage(const std::wstring& msg) {
    // {"cg":"nav","url":"..."}
    size_t p = msg.find(L"\"url\"");
    if (p == std::wstring::npos) return;
    p = msg.find(L':', p);
    if (p == std::wstring::npos) return;
    p++;
    while (p < msg.size() && (msg[p] == L' ' || msg[p] == L'\t')) p++;
    if (p >= msg.size() || msg[p] != L'"') return;
    p++;
    std::wstring url;
    while (p < msg.size() && msg[p] != L'"') {
        if (msg[p] == L'\\' && p + 1 < msg.size()) { url += msg[p + 1]; p += 2; continue; }
        url += msg[p++];
    }
    if (!url.empty()) OpenUrlDirect(url);
}

void App::ShowStartPage() {
    startPageActive_ = true;
    // CSS 变量由当前调色板生成（theme::StartPageCss）：起始页与 shell 同源配色
    std::string html =
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>"
        + theme::StartPageCss() +
        "*{box-sizing:border-box;margin:0;padding:0}"
        "body{font-family:'Segoe UI','Microsoft YaHei',sans-serif;background:var(--canvas);color:var(--text);"
        "min-height:100vh;display:flex;align-items:center;justify-content:center;padding:24px}"
        ".wrap{max-width:600px;width:100%}"
        ".logo{width:48px;height:48px;border-radius:14px;background:linear-gradient(135deg,var(--accent),var(--accent-fill));"
        "display:flex;align-items:center;justify-content:center;color:var(--on-accent);font-size:24px;font-weight:600;margin-bottom:20px}"
        "h1{font-size:22px;font-weight:600;margin-bottom:8px}"
        ".sub{font-size:13px;color:var(--text-2);margin-bottom:28px}"
        ".steps{display:flex;gap:12px;margin-bottom:28px}"
        ".step{flex:1;background:var(--surface);border:1px solid var(--border);border-radius:var(--radius-card);padding:14px 14px 16px}"
        ".step b{display:block;font-size:12px;color:var(--accent-text);margin-bottom:8px;font-weight:600}"
        ".step span{font-size:12px;color:var(--text-2);line-height:1.6}"
        ".chips{display:flex;flex-wrap:wrap;gap:8px}"
        ".chip{background:var(--surface);border:1px solid var(--border);border-radius:999px;padding:8px 16px;font-size:12px;"
        "cursor:pointer;transition:all .15s;user-select:none}"
        ".chip:hover{border-color:var(--accent);color:var(--accent-text);background:var(--accent-tint)}"
        ".tip{margin-top:28px;font-size:11px;color:var(--text-3);line-height:1.8}"
        "</style></head><body><div class=\"wrap\">"
        "<div class=\"logo\">C</div>"
        "<h1>Cookie 抓取工具</h1>"
        "<div class=\"sub\">在任意网站完成登录，自动捕获 Cookie 与 Token 并写入日志</div>"
        "<div class=\"steps\">"
        "<div class=\"step\"><b>1 · 打开</b><span>在上方地址栏输入网址并打开</span></div>"
        "<div class=\"step\"><b>2 · 登录</b><span>在页面内扫码，或使用账号密码登录</span></div>"
        "<div class=\"step\"><b>3 · 抓取</b><span>登录成功后自动抓取并写入日志文件</span></div>"
        "</div>";
    if (!settings_.presets.empty()) {
        html += "<div class=\"chips\">";
        for (auto& p : settings_.presets) {
            std::string name = util::WideToUtf8(p.name);
            std::string url = util::WideToUtf8(p.url);
            auto esc = [](std::string s) {
                std::string r;
                for (char c : s) { if (c == '&') r += "&amp;"; else if (c == '<') r += "&lt;"; else if (c == '>') r += "&gt;"; else if (c == '"') r += "&quot;"; else r += c; }
                return r;
            };
            html += "<div class=\"chip\" data-url=\"" + esc(url) + "\">" + esc(name) + "</div>";
        }
        html += "</div>";
    }
    html += "<div class=\"tip\">日志文件位于程序同目录（不可写时自动回退用户目录）· "
            "可在设置页调整抓取选项与日志格式 · 支持 Cookie / Token 三通道捕获</div>";
    html += "<script>document.querySelectorAll('.chip').forEach(function(c){c.addEventListener('click',function(){"
            "window.chrome.webview.postMessage(JSON.stringify({cg:'nav',url:c.getAttribute('data-url')}));});});</script>";
    html += "</div></body></html>";
    webview_.NavigateToString(util::Utf8ToWide(html));
    // 回到起始页：地址栏显式置空（includeAddress=true + startPageActive_=true → ""）
    PushShellState(true);
}

// ==================== 主窗口过程 ====================
static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    App* app = g_app;
    switch (msg) {
    case WM_CREATE:
        app->main_ = hwnd;
        app->SetDpi(GetDpiForWindow(hwnd));
        app->BuildBrushes();
        app->CreateControls();
        app->Layout();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        // 只剩一层底色：外缩 S(5) 缩放边带 + shell 未就绪时的过渡填充。
        // 所有 UI 元素都由 shell WebView 子窗口绘制，绘制顺序 shell 在 webHost 之下。
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (app->brBg_) FillRect(dc, &rc, app->brBg_);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SIZE:
        app->Layout();
        app->PushShellState(); // 最大化/还原图标随尺寸更新
        return 0;
    case WM_DPICHANGED: {
        RECT* rc = (RECT*)lp;
        SetWindowPos(hwnd, nullptr, rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        app->SetDpi(HIWORD(wp));
        app->ReleaseUiResources();
        app->BuildBrushes();
        app->Layout();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mm = (MINMAXINFO*)lp;
        mm->ptMinTrackSize.x = app->S(560);
        mm->ptMinTrackSize.y = app->S(560);
        return 0;
    }
    // ---- 无边框框架 ----
    case WM_NCCALCSIZE: {
        // wp=FALSE（如窗口创建期间的初次计算）也必须自行返回 0：
        // 客户区=窗口矩形。若交给 DefWindowProc 会保留 39px 标准标题栏/边框，
        // 且此后窗口尺寸不变时不会再收到 wp=TRUE 的修正机会 → 双标题栏
        if (!wp) return 0;
        auto* p = (NCCALCSIZE_PARAMS*)lp;
        if (IsZoomed(hwnd)) {
            int cx = GetSystemMetricsForDpi(SM_CXSIZEFRAME, app->Dpi()) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, app->Dpi());
            int cy = GetSystemMetricsForDpi(SM_CYSIZEFRAME, app->Dpi()) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, app->Dpi());
            p->rgrc[0].left += cx; p->rgrc[0].right -= cx;
            p->rgrc[0].top += cy;  p->rgrc[0].bottom -= cy;
        }
        return 0;
    }
    case WM_NCHITTEST: {
        // 仅在客户区最外 S(5) 环带上响应缩放手柄；其余区域交 HTML/子窗口处理。
        // 最大化态禁用边缘缩放（与系统窗口一致）。
        if (IsZoomed(hwnd)) return HTCLIENT;
        POINT pt = { PtX(lp), PtY(lp) };
        ScreenToClient(hwnd, &pt);
        RECT wr;
        GetClientRect(hwnd, &wr);
        int w = wr.right, h = wr.bottom;
        int b = app->S(5);
        if (pt.x < b) return (pt.y < b) ? HTTOPLEFT : (pt.y > h - b ? HTBOTTOMLEFT : HTLEFT);
        if (pt.x > w - b) return (pt.y < b) ? HTTOPRIGHT : (pt.y > h - b ? HTBOTTOMRIGHT : HTRIGHT);
        if (pt.y < b) return HTTOP;
        if (pt.y > h - b) return HTBOTTOM;
        return HTCLIENT;
    }
    case WM_NCACTIVATE:
        app->SetActive(wp != 0);
        app->PushShellState();
        return TRUE;
    case WM_ACTIVATE:
        app->SetActive(LOWORD(wp) != WA_INACTIVE);
        app->PushShellState();
        return 0;
    case WM_APP_WEBVIEW_READY:
        app->OnWebViewReady();
        return 0;
    case WM_APP_SHELL_READY:
        app->OnShellReady();
        return 0;
    case WM_APP_DRAG:
        // 系统移动模态：从 WebView2 回调栈中 PostMessage 出来后再执行
        ReleaseCapture();
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;
    case WM_TIMER:
        app->OnTimer((int)wp);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_LOGIN_POLL);
        KillTimer(hwnd, 2);
        KillTimer(hwnd, 3);
        KillTimer(hwnd, TIMER_SHOW_FALLBACK);
        app->KillCaptureTimers();
        app->ShutdownWebView();
        app->ReleaseSingleInstance();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// 宿主子窗口：WebView2 控制器异步创建期间，本窗口内没有子视图覆盖 →
// 若不自绘就是一块未初始化的黑区。按 canvas 色填充，与 shell HTML body 同色，
// 视觉无缝；控制器就绪后 WebView2 子窗口盖上（宿主类带 WS_CLIPCHILDREN）。
static LRESULT CALLBACK HostWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH br = CreateSolidBrush(theme::Current().canvas);
        FillRect(dc, &rc, br);
        DeleteObject(br);
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
