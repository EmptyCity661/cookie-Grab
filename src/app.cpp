// 应用主体：Win32 无边框外壳 + WebView2 玻璃界面（shell 铺满客户区，GDI 自绘退役）
#include "app.h"
#include "util.h"
#include "theme.h"
#include "json.h"
#include <shellapi.h>
#include <dwmapi.h>
#include <cstdio>
#include <string>

#pragma comment(lib, "dwmapi.lib")

static const wchar_t kMainClass[] = L"CGMainWndClass";
static const wchar_t kHostClass[] = L"CGWebHostClass";
static App* g_app = nullptr;

// 定时器 / 自定义消息
static const int TIMER_LOGIN_POLL = 1;
static const int TIMER_SHOW_FALLBACK = 4;
static const UINT WM_APP_WEBVIEW_READY = WM_APP + 1; // Content WebView 控制器就绪
static const UINT WM_APP_SHELL_READY = WM_APP + 2;   // Shell 控制器就绪

// 宿主子窗口用途标记（GWLP_USERDATA）：0=shell，1=content
static const LONG_PTR HOST_TAG_SHELL = 0;
static const LONG_PTR HOST_TAG_CONTENT = 1;

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
    singleInstance_ = CreateMutexW(nullptr, TRUE, L"Local\\cookie抓取工具3.0SingleInstance");
    if (singleInstance_ && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND exist = FindWindowW(kMainClass, nullptr);
        if (exist) {
            ShowWindow(exist, SW_RESTORE);
            SetForegroundWindow(exist);
        } else {
            MessageBoxW(nullptr, L"cookie抓取工具3.0 已在运行中。", L"提示", MB_ICONINFORMATION);
        }
        CloseHandle(singleInstance_);
        singleInstance_ = nullptr;
        return false;
    }
    // COM 必须先初始化：SHGetKnownFolderPath / WebView2 均依赖 STA
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hrCo) && hrCo != RPC_E_CHANGED_MODE) return false;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = kMainClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;   // 客户区由 shell 铺满；WM_PAINT 兜底铺底色
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

    // 解析主题必须先于窗口/起始页：theme::Current() 决定 CSS 变量与宿主兜底色
    theme::Resolve(theme::DetectDarkMode());

    settings_ = LoadSettings();
    logger_.Init();

    main_ = CreateWindowExW(0, kMainClass, L"cookie抓取工具3.0",
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

    // 窗口居中
    RECT wa{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    UINT dpi = GetDpiForWindow(main_);
    int w = MulDiv(1180, dpi, 96), h = MulDiv(780, dpi, 96);
    int x = wa.left + ((wa.right - wa.left) - w) / 2;
    int y = wa.top + ((wa.bottom - wa.top) - h) / 2;
    SetWindowPos(main_, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);

    SetTimer(main_, TIMER_LOGIN_POLL, 1500, nullptr);

    // 宿主子窗口：shell 在下、content 在上（孔区由 content 覆盖）。
    // 两者必须 WS_VISIBLE：shell 铺满客户区常驻显示，content 的显隐由 Layout 按页面控制
    shellHost_ = CreateWindowExW(0, kHostClass, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 10, 10, main_, nullptr, inst_, nullptr);
    webHost_ = CreateWindowExW(0, kHostClass, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 10, 10, main_, nullptr, inst_, nullptr);
    SetWindowLongPtrW(shellHost_, GWLP_USERDATA, HOST_TAG_SHELL);
    SetWindowLongPtrW(webHost_, GWLP_USERDATA, HOST_TAG_CONTENT);
    SetWindowPos(webHost_, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    std::wstring userData = util::LocalAppDir() + L"\\userdata";

    // Content WebView：起始页按钮消息优先，其余交抓取后端
    webview_.SetMessageHandler([this](const std::string& json) {
        std::wstring wj = util::Utf8ToWide(json);
        if (wj.find(L"\"cg\"") != std::wstring::npos) { OnStartPageMessage(wj); return; }
        capture_.OnWebMessage(json);
    });
    webview_.SetNavHandler([this](bool ok, const std::wstring& url) {
        capture_.OnNavigationCompleted(ok);
        bool realPage = !url.empty() &&
            (util::StartsWithI(url, L"http://") || util::StartsWithI(url, L"https://") || util::StartsWithI(url, L"file://"));
        caption_ = realPage ? CaptureManager::HostFromUrl(url) : std::wstring();
        PushShellState(); // 地址/状态随导航更新
    });
    webview_.SetReadyHandler([this]() {
        if (main_) PostMessageW(main_, WM_APP_WEBVIEW_READY, 0, 0);
    });
    webview_.Create(webHost_, userData);

    // Shell WebView：整页玻璃外壳 + 统一消息桥
    InitShell();

    capture_.Init(&webview_, &settings_, &logger_,
        [this](const std::wstring& t) { OnCaptureStatus(t); },
        [this](int id, int ms) { ScheduleTimer(id, ms); });

    if (!initialUrl_.empty()) webview_.Navigate(initialUrl_);
    else ShowStartPage();

    Layout();

    // 启动不立即显示：WebView2 异步初始化，改由就绪回调显示；超时兜底保证仍会出现
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

void App::ShutdownWebView() {
    shellWv_.Destroy();   // 先销毁 shell（共享加载器按引用计数归还）
    webview_.Destroy();
}

void App::ShowMainWindow() {
    if (!main_ || IsWindowVisible(main_)) return;
    ShowWindow(main_, SW_SHOW);
    RedrawWindow(main_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

void App::OnWebViewReady() {
    KillTimer(main_, TIMER_SHOW_FALLBACK);
    ShowMainWindow();
}

// ==================== 布局 ====================
void App::Layout() {
    if (!main_) return;
    RECT rc;
    GetClientRect(main_, &rc);
    int cx = rc.right, cy = rc.bottom;

    // shell 铺满整个客户区（含标题栏/导航/工具栏/状态栏视觉与环境光斑）
    MoveWindow(shellHost_, 0, 0, cx, cy, TRUE);
    if (shellReady_) shellWv_.Resize(0, 0, cx, cy);

    // content 视口按 shell 上报的矩形定位（物理像素，客户区坐标）
    if (page_ == 0 && contentW_ > 0 && contentH_ > 0) {
        ShowWindow(webHost_, SW_SHOW);
        MoveWindow(webHost_, contentX_, contentY_, contentW_, contentH_, TRUE);
        if (webview_.IsReady()) webview_.Resize(0, 0, contentW_, contentH_);
    } else {
        ShowWindow(webHost_, SW_HIDE);
    }
    InvalidateRect(main_, nullptr, FALSE);
}

void App::ApplyContentLayout(double x, double y, double w, double h, double dpr) {
    if (dpr <= 0) dpr = 1.0;
    int px = (int)(x * dpr + 0.5), py = (int)(y * dpr + 0.5);
    int pw = (int)(w * dpr + 0.5), ph = (int)(h * dpr + 0.5);
    if (pw <= 0 || ph <= 0) return; // 非浏览器页时孔被隐藏，忽略零尺寸上报
    contentX_ = px; contentY_ = py; contentW_ = pw; contentH_ = ph;
    if (page_ != 0) return;         // 仅在浏览器页应用（否则等切回时再定位）
    ShowWindow(webHost_, SW_SHOW);
    MoveWindow(webHost_, px, py, pw, ph, TRUE);
    if (webview_.IsReady()) webview_.Resize(0, 0, pw, ph);
}

void App::SwitchPage(int page) {
    page_ = page;
    Layout();
    PushShellState(); // 通知 shell 切换视图（tab 高亮 / 面板显隐）
}

// ==================== 业务 ====================
void App::OpenUrlDirect(const std::wstring& urlIn) {
    std::wstring url = util::Trim(urlIn);
    if (url.empty()) return;
    if (!util::StartsWithI(url, L"http://") && !util::StartsWithI(url, L"https://") && !util::StartsWithI(url, L"file://"))
        url = L"https://" + url;
    startPageActive_ = false;
    SwitchPage(0);
    caption_ = CaptureManager::HostFromUrl(url);
    webview_.Navigate(url);
}

void App::CaptureNow(int mask) {
    capture_.RequestCapture(true, mask); // 1=仅Cookie 2=仅Token
}

void App::RefreshPage() {
    SwitchPage(0);
    if (!webview_.IsReady()) {
        OnCaptureStatus(L"页面尚未加载完成，请稍候再刷新");
        return;
    }
    OnCaptureStatus(L"正在刷新…");
    webview_.Reload();
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
    InvalidateRect(main_, nullptr, FALSE);
}

void App::OpenLogFolder() {
    ShellExecuteW(main_, L"open", logger_.Dir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void App::OnCaptureStatus(const std::wstring& text) {
    statusText_ = text;
    UpdateStatus();
}

void App::UpdateStatus() {
    PushShellState();
}

void App::OnTimer(int id) {
    switch (id) {
    case TIMER_LOGIN_POLL:
        capture_.PollTick();
        if (!webview_.IsReady() && webviewFailChecks_ < 8) {
            webviewFailChecks_++;
            if (webviewFailChecks_ >= 8)
                OnCaptureStatus(L"WebView2 初始化失败：请安装 Edge WebView2 运行时后重试");
        }
        break;
    case 2: KillTimer(main_, 2); capture_.FirePending(); break;
    case 3: KillTimer(main_, 3); capture_.FireDelayedStep(); break;
    case TIMER_SHOW_FALLBACK:
        KillTimer(main_, TIMER_SHOW_FALLBACK);
        ShowMainWindow();
        break;
    }
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
    // CSS 变量由当前调色板生成（theme::StartPageCss）：起始页与 shell 同源配色，跟随深浅色
    std::string html =
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>"
        + theme::StartPageCss() +
        "*{box-sizing:border-box;margin:0;padding:0}"
        "html,body{min-height:100vh}"
        "body{font-family:'Segoe UI','Microsoft YaHei',sans-serif;color:var(--text);"
        "background:radial-gradient(ellipse at 17% 8%,var(--ambient-a),transparent 34%),"
        "radial-gradient(ellipse at 87% 15%,var(--ambient-b),transparent 33%),"
        "radial-gradient(ellipse at 68% 92%,var(--ambient-c),transparent 36%),var(--canvas);"
        "background-blend-mode:screen,screen,screen,normal;"
        "display:flex;align-items:center;justify-content:center;padding:24px}"
        ".wrap{max-width:600px;width:100%}"
        ".logo{width:48px;height:48px;border-radius:14px;background:linear-gradient(135deg,var(--accent-cyan),var(--accent) 62%,var(--accent-violet));"
        "display:flex;align-items:center;justify-content:center;color:var(--on-accent);font-size:24px;font-weight:600;margin-bottom:20px;"
        "box-shadow:0 0 24px var(--glow)}"
        "h1{font-size:22px;font-weight:600;margin-bottom:8px}"
        ".sub{font-size:13px;color:var(--text-2);margin-bottom:28px}"
        ".steps{display:flex;gap:12px;margin-bottom:28px}"
        ".step{flex:1;background:var(--glass);border:1px solid var(--glass-border);border-radius:var(--radius-card);"
        "padding:14px 14px 16px;backdrop-filter:blur(var(--blur)) saturate(var(--saturate));"
        "-webkit-backdrop-filter:blur(var(--blur)) saturate(var(--saturate));"
        "box-shadow:inset 0 1px 0 var(--glass-hi),var(--shadow)}"
        ".step b{display:block;font-size:12px;color:var(--accent-text);margin-bottom:8px;font-weight:600}"
        ".step span{font-size:12px;color:var(--text-2);line-height:1.6}"
        ".chips{display:flex;flex-wrap:wrap;gap:8px}"
        ".chip{background:var(--glass);border:1px solid var(--glass-border);border-radius:999px;padding:8px 16px;font-size:12px;"
        "backdrop-filter:blur(var(--blur)) saturate(var(--saturate));-webkit-backdrop-filter:blur(var(--blur)) saturate(var(--saturate));"
        "box-shadow:inset 0 1px 0 var(--glass-hi);cursor:pointer;transition:all .15s;user-select:none}"
        ".chip:hover{border-color:var(--accent);color:var(--accent-text);background:var(--accent-tint);box-shadow:0 0 14px var(--glow),inset 0 1px 0 var(--glass-hi)}"
        ".tip{margin-top:28px;font-size:11px;color:var(--text-3);line-height:1.8}"
        "</style></head><body><div class=\"wrap\">"
        "<div class=\"logo\">C</div>"
        "<h1>cookie抓取工具3.0</h1>"
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
}

// ==================== Shell 玻璃外壳 ====================
void App::InitShell() {
    if (!shellHost_) return;
    shellWv_.SetMessageHandler([this](const std::string& json) { OnShellMessage(json); });
    shellWv_.SetReadyHandler([this]() {
        if (main_) PostMessageW(main_, WM_APP_SHELL_READY, 0, 0);
    });
    std::wstring userData = util::LocalAppDir() + L"\\userdata"; // 与 content 共用（加载器引用计数共享）
    shellWv_.Create(shellHost_, userData);
}

void App::OnShellReady() {
    shellReady_ = true;
    ShowShellHtml();
    Layout();
}

void App::ShowShellHtml() {
    std::string html = shell::Build(MakeShellState());
    shellWv_.NavigateToString(util::Utf8ToWide(html));
}

shell::State App::MakeShellState() {
    shell::State s;
    s.page = page_;
    s.dark = theme::Current().dark;
    s.status = statusText_;
    s.cookieCount = capture_.CookieCount();
    s.tokenCount = capture_.TokenCount();
    s.address = startPageActive_ ? std::wstring() : webview_.CurrentUrl();
    s.logDir = logger_.Dir();
    s.hint = hintMsg_;
    s.startPage = startPageActive_;
    s.settings = settings_;
    return s;
}

void App::PushShellState() {
    if (!shellReady_) return;
    std::string js = shell::StateJs(MakeShellState());
    shellWv_.ExecuteScript(util::Utf8ToWide(js));
}

// JS→host 统一分发：{t:"...",...}
void App::OnShellMessage(const std::string& json) {
    auto v = mjson::Parse(json);
    if (!v || v->type != mjson::Value::Object) return;
    std::string t = v->GetStr("t");

    if (t == "ready") {
        // shell 文档加载完成：推送初值并触发首次内容视口定位
        PushShellState();
        return;
    }
    if (t == "layout") {
        double x = v->Has("x") ? v->obj["x"]->num : 0;
        double y = v->Has("y") ? v->obj["y"]->num : 0;
        double w = v->Has("w") ? v->obj["w"]->num : 0;
        double h = v->Has("h") ? v->obj["h"]->num : 0;
        double dpr = v->Has("dpr") ? v->obj["dpr"]->num : 1.0;
        ApplyContentLayout(x, y, w, h, dpr);
        return;
    }
    if (t == "page") {
        SwitchPage((int)(v->Has("p") ? v->obj["p"]->num : 0));
        return;
    }
    if (t == "open") { OpenUrlDirect(util::Utf8ToWide(v->GetStr("url"))); return; }
    if (t == "reload") { RefreshPage(); return; }
    if (t == "regex") { capture_.SetCookieFilter(util::Utf8ToWide(v->GetStr("re"))); return; }
    if (t == "cap") {
        if (v->Has("re")) capture_.SetCookieFilter(util::Utf8ToWide(v->GetStr("re")));
        CaptureNow((int)(v->Has("m") ? v->obj["m"]->num : 3));
        return;
    }
    if (t == "win") {
        std::string a = v->GetStr("a");
        if (a == "min") ShowWindow(main_, SW_MINIMIZE);
        else if (a == "max") ShowWindow(main_, IsZoomed(main_) ? SW_RESTORE : SW_MAXIMIZE);
        else if (a == "close") PostMessageW(main_, WM_CLOSE, 0, 0);
        else if (a == "drag") { ReleaseCapture(); SendMessageW(main_, WM_NCLBUTTONDOWN, HTCAPTION, 0); }
        else if (a == "resize") {
            std::string d = v->GetStr("d");
            WPARAM ht = HTCLIENT;
            if (d == "l") ht = HTLEFT; else if (d == "r") ht = HTRIGHT;
            else if (d == "t") ht = HTTOP; else if (d == "b") ht = HTBOTTOM;
            else if (d == "tl") ht = HTTOPLEFT; else if (d == "tr") ht = HTTOPRIGHT;
            else if (d == "bl") ht = HTBOTTOMLEFT; else if (d == "br") ht = HTBOTTOMRIGHT;
            if (ht != HTCLIENT && !IsZoomed(main_)) {
                ReleaseCapture();
                SendMessageW(main_, WM_NCLBUTTONDOWN, ht, 0);
            }
        }
        return;
    }
    if (t == "cfg") {
        std::string a = v->GetStr("a");
        if (a == "openlog") { OpenLogFolder(); return; }
        if (a == "clearlogin") { ClearLoginState(); return; }
        if (a == "save") {
            auto it = v->obj.find("data");
            if (it == v->obj.end() || it->second->type != mjson::Value::Object) return;
            const mjson::Value& d = *it->second;
            settings_.autoCapture = d.GetBool("autoCapture", settings_.autoCapture);
            settings_.clipboard = d.GetBool("clipboard", settings_.clipboard);
            settings_.history = d.GetBool("history", settings_.history);
            settings_.firstOpenCapture = d.GetBool("firstOpenCapture", settings_.firstOpenCapture);
            int fmt = d.Has("logFormat") ? (int)d.obj.at("logFormat")->num : 0;
            settings_.logFormat = fmt == 1 ? LogFormat::Json : (fmt == 2 ? LogFormat::Both : LogFormat::String);
            // presets 为整体覆盖（非合并）
            if (d.Has("presets") && d.obj.at("presets")->type == mjson::Value::Array) {
                settings_.presets.clear();
                for (auto& pv : d.obj.at("presets")->arr) {
                    if (pv && pv->type == mjson::Value::Object) {
                        Preset pr;
                        pr.name = util::Utf8ToWide(pv->GetStr("name"));
                        pr.url = util::Utf8ToWide(pv->GetStr("url"));
                        if (!pr.name.empty()) settings_.presets.push_back(pr);
                    }
                }
            }
            SaveSettings(settings_);
            hintMsg_ = L"设置已保存，立即生效。";
            PushShellState();
            return;
        }
        return;
    }
}

// ==================== 主窗口过程 ====================
static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    App* app = g_app;
    switch (msg) {
    case WM_CREATE:
        app->main_ = hwnd;
        app->SetDpi(GetDpiForWindow(hwnd));
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        // shell 铺满后本窗口基本不可见；仅在 shell 就绪前铺一层玻璃底色避免闪黑
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
    case WM_SIZE:
        app->Layout();
        return 0;
    case WM_DPICHANGED: {
        RECT* nr = (RECT*)lp;
        SetWindowPos(hwnd, nullptr, nr->left, nr->top, nr->right - nr->left, nr->bottom - nr->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        app->SetDpi(HIWORD(wp));
        app->Layout();
        app->PushShellState();
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
        // shell 就绪后由 JS 桥接拖动/缩放（win:drag/resize）；此处仅为 shell 未就绪时兜底
        if (app->ShellReady()) break;
        POINT pt = { PtX(lp), PtY(lp) };
        ScreenToClient(hwnd, &pt);
        RECT wr; GetClientRect(hwnd, &wr);
        int w = wr.right, h = wr.bottom, b = app->S(5);
        if (!IsZoomed(hwnd)) {
            if (pt.x < b) return (pt.y < b) ? HTTOPLEFT : (pt.y > h - b ? HTBOTTOMLEFT : HTLEFT);
            if (pt.x > w - b) return (pt.y < b) ? HTTOPRIGHT : (pt.y > h - b ? HTBOTTOMRIGHT : HTRIGHT);
            if (pt.y < b) return HTTOP;
            if (pt.y > h - b) return HTBOTTOM;
        }
        if (pt.y < app->S(46)) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_APP_WEBVIEW_READY:
        app->OnWebViewReady();
        return 0;
    case WM_APP_SHELL_READY:
        app->OnShellReady();
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

// 宿主子窗口过程：WebView2 就绪前铺底色 + （content 宿主）启动过渡提示，避免闪黑
static LRESULT CALLBACK HostWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        const Palette& pal = theme::Current();
        HBRUSH br = CreateSolidBrush(pal.canvas);
        FillRect(dc, &rc, br);
        DeleteObject(br);
        LONG_PTR tag = GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        App* app = g_app;
        if (tag == HOST_TAG_CONTENT && app && !app->IsWebViewReady() &&
            rc.right - rc.left > app->S(200) && rc.bottom - rc.top > app->S(60)) {
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, pal.text3);
            LOGFONTW lf = {};
            lf.lfHeight = -MulDiv(13, app->Dpi(), 96);
            lf.lfQuality = CLEARTYPE_QUALITY;
            wcsncpy(lf.lfFaceName, L"Segoe UI", LF_FACESIZE - 1);
            HFONT f = CreateFontIndirectW(&lf);
            HGDIOBJ old = SelectObject(dc, f);
            DrawTextW(dc, L"正在初始化浏览器引擎…", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dc, old);
            DeleteObject(f);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
