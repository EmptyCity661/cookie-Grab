// 应用主体：Win32 现代化无边框 GUI（响应式：宽/中/窄三档）
#include "app.h"
#include "util.h"
#include "theme.h"
#include "webtoolbar.h"
#include "json.h"
#include <commctrl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <cstdio>
#include <vector>
#include <string>

#pragma comment(lib, "dwmapi.lib")

// ==================== 主题 ====================
// 颜色一律取自 theme.h 的 Palette（theme::Current()），本文件不再私设调色板：
// 此前原生绘制用本地硬编码颜色、起始页另用一套靛蓝，导致风格割裂。

static const wchar_t kMainClass[] = L"CGMainWndClass";
static const wchar_t kHostClass[] = L"CGWebHostClass";
static App* g_app = nullptr;

// ---------------- 定时器 / 自定义消息 ----------------
// 定时器 ID：1=登录态轮询，2/3=抓取延时步骤（见 capture.cpp），4=启动显示兜底
static const int TIMER_LOGIN_POLL = 1;
static const int TIMER_SHOW_FALLBACK = 4;
// WebView2 就绪（由 WebViewHost 回调投递，延迟到回调栈之外再处理）
static const UINT WM_APP_WEBVIEW_READY = WM_APP + 1;
// HTML 工具栏的控制器就绪（方案 A 原型）
static const UINT WM_APP_TB_READY = WM_APP + 2;

static inline int PtX(LPARAM lp) { return (int)(short)LOWORD(lp); }
static inline int PtY(LPARAM lp) { return (int)(short)HIWORD(lp); }
static inline int WheelDelta(WPARAM wp) { return (int)(short)HIWORD(wp); }

static LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK HostWndProc(HWND, UINT, WPARAM, LPARAM);

// ---------------- 绘制辅助 ----------------
// CLR_NONE 统一由 theme.h 定义

static void FillRoundRect(HDC dc, RECT rc, int r, COLORREF fill, COLORREF border, int borderW = 1) {
    if (r < 1) r = 1;
    if (rc.right - rc.left < 2 * r) r = (rc.right - rc.left) / 2;
    if (rc.bottom - rc.top < 2 * r) r = (rc.bottom - rc.top) / 2;
    HBRUSH br = (fill == CLR_NONE) ? (HBRUSH)GetStockObject(NULL_BRUSH) : CreateSolidBrush(fill);
    HPEN pen = (border == CLR_NONE) ? (HPEN)GetStockObject(NULL_PEN) : CreatePen(PS_INSIDEFRAME, borderW, border);
    HGDIOBJ ob = SelectObject(dc, br), op = SelectObject(dc, pen);
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, r * 2, r * 2);
    SelectObject(dc, ob); SelectObject(dc, op);
    if (fill != CLR_NONE) DeleteObject(br);
    if (border != CLR_NONE) DeleteObject(pen);
}

static void TextOut2(HDC dc, const std::wstring& t, RECT rc, HFONT font, COLORREF color, UINT flags) {
    if (t.empty()) return;
    HGDIOBJ old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, t.c_str(), -1, &rc, flags);
    SelectObject(dc, old);
}

static void DrawCookieDot(HDC dc, int cx, int cy, int r, COLORREF c) {
    // 同色画笔：不设画笔时 Ellipse 会用默认 BLACK_PEN 描一圈黑边（与 draw::Dot 同实现）
    HBRUSH br = CreateSolidBrush(c);
    HPEN pen = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ ob = SelectObject(dc, br);
    HGDIOBJ op = SelectObject(dc, pen);
    Ellipse(dc, cx - r, cy - r, cx + r, cy + r);
    SelectObject(dc, op); DeleteObject(pen);
    SelectObject(dc, ob); DeleteObject(br);
}

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

    // 解析主题：必须先于窗口创建（WM_CREATE 即用调色板建画刷）与起始页生成，
    // 否则 theme::Current() 返回未初始化的全零调色板（纯黑）
    theme::Resolve(theme::DetectDarkMode());

    settings_ = LoadSettings();
    logger_.Init();

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

    // 窗口居中
    RECT wa{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    UINT dpi = GetDpiForWindow(main_);
    int w = MulDiv(1180, dpi, 96), h = MulDiv(780, dpi, 96);
    int x = wa.left + ((wa.right - wa.left) - w) / 2;
    int y = wa.top + ((wa.bottom - wa.top) - h) / 2;
    SetWindowPos(main_, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);

    SetTimer(main_, 1, 1500, nullptr); // 登录态轮询

    webview_.SetMessageHandler([this](const std::string& json) {
        // 起始页按钮消息优先处理
        std::wstring wj = util::Utf8ToWide(json);
        if (wj.find(L"\"cg\"") != std::wstring::npos) { OnStartPageMessage(wj); return; }
        capture_.OnWebMessage(json);
    });
    webview_.SetNavHandler([this](bool ok, const std::wstring& url) {
        capture_.OnNavigationCompleted(ok);
        bool realPage = !url.empty() &&
            (util::StartsWithI(url, L"http://") || util::StartsWithI(url, L"https://") || util::StartsWithI(url, L"file://"));
        if (realPage) {
            caption_ = CaptureManager::HostFromUrl(url);
            UpdateToolbarAddress(url);
        } else {
            caption_.clear();
        }
        InvalidateRect(main_, nullptr, FALSE);
        UpdateStatus();
    });
    std::wstring userData = util::LocalAppDir() + L"\\userdata";
    // 就绪通知投递给主窗口过程：不在 WebView2 回调栈内直接操作窗口（避免重入）
    webview_.SetReadyHandler([this]() {
        if (main_) PostMessageW(main_, WM_APP_WEBVIEW_READY, 0, 0);
    });
    webview_.Create(webHost_, userData);
    
    // 第二个 WebView 承载 HTML 工具栏（共用同一用户目录/加载器）
    InitHtmlToolbar();

    capture_.Init(&webview_, &settings_, &logger_,
        [this](const std::wstring& t) { OnCaptureStatus(t); },
        [this](int id, int ms) { ScheduleTimer(id, ms); });

    if (!initialUrl_.empty()) webview_.Navigate(initialUrl_);
    else ShowStartPage();

    // 启动不立即显示窗口：WebView2 是异步初始化，先显示会让用户盯着一块没有内容的
    // Web 区域（宿主子窗口此时没有任何子窗口可绘制）。改由就绪回调（OnWebViewReady）
    // 显示；超时兜底保证 WebView2 缺失/初始化失败时界面仍会出现，且能看到过渡提示。
    SetTimer(main_, TIMER_SHOW_FALLBACK, 500, nullptr);
    return true;
}

int App::Run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        // 输入方式判定：所有按键消息都先经过本循环（含发往子控件的），
        // 因此鼠标按下即可收起焦点提示，键盘导航再重新显示
        if (msg.message == WM_LBUTTONDOWN || msg.message == WM_RBUTTONDOWN || msg.message == WM_MBUTTONDOWN) {
            SetFocusCue(false);
        } else if (msg.message == WM_KEYDOWN &&
                   (msg.wParam == VK_TAB || msg.wParam == VK_LEFT || msg.wParam == VK_RIGHT ||
                    msg.wParam == VK_UP || msg.wParam == VK_DOWN)) {
            SetFocusCue(true);
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

void App::SetFocusCue(bool on) {
    if (focusCue_ == on) return;
    focusCue_ = on;
    // 立即重绘当前获得焦点的控件：焦点环随输入方式即时出现/消失
    if (HWND f = GetFocus()) InvalidateRect(f, nullptr, FALSE);
}

int App::S(int v) const { return MulDiv(v, dpi_, 96); }

void App::EnsureMemBuffer(HDC ref, int w, int h) {
    if (memDC_ && memBmp_ && memW_ == w && memH_ == h) return;
    FreeMemBuffer();
    if (w <= 0 || h <= 0) return;
    memDC_ = CreateCompatibleDC(ref);
    if (!memDC_) return;
    memBmp_ = CreateCompatibleBitmap(ref, w, h);
    if (!memBmp_) {
        DeleteDC(memDC_);
        memDC_ = nullptr;
        return;
    }
    SelectObject(memDC_, memBmp_); // 位图常驻于内存 DC
    memW_ = w;
    memH_ = h;
}

void App::FreeMemBuffer() {
    if (memDC_) { DeleteDC(memDC_); memDC_ = nullptr; }
    if (memBmp_) { DeleteObject(memBmp_); memBmp_ = nullptr; }
    memW_ = memH_ = 0;
}

void App::ShutdownWebView() {
    toolbarWv_.Destroy();   // 先销毁工具栏 WebView（共享加载器按引用计数归还）
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
    // 控制器已就绪，Web 卡片区域已有真实内容：取消兜底定时器并结束启动过渡
    KillTimer(main_, TIMER_SHOW_FALLBACK);
    ShowMainWindow();
}

void App::BuildFonts() {
    auto mk = [&](int px, int weight, const wchar_t* face) {
        LOGFONTW lf = {};
        lf.lfHeight = -MulDiv(px, dpi_, 96);
        lf.lfWeight = weight;
        lf.lfQuality = CLEARTYPE_QUALITY;
        wcsncpy(lf.lfFaceName, face, LF_FACESIZE - 1);
        return CreateFontIndirectW(&lf);
    };
    const wchar_t* ui = L"Segoe UI";
    fTitle_   = mk(14, FW_SEMIBOLD, ui);
    fNav_     = mk(13, FW_MEDIUM, ui);
    fNavB_    = mk(13, FW_SEMIBOLD, ui);
    fBody_    = mk(12, FW_NORMAL, ui);
    fCaption_ = mk(11, FW_NORMAL, ui);
    fBtn_     = mk(12, FW_SEMIBOLD, ui);
    fCard_    = mk(13, FW_SEMIBOLD, ui);
    fMdl2_    = mk(14, FW_NORMAL, L"Segoe MDL2 Assets");
}

void App::ApplyFonts() {
    HWND kids[] = { navBrowser_, navSettings_, navHelp_, presetCombo_, loginUrlEdit_, logFileEdit_,
        btnSave_, btnClear_, btnOpenLog_, chkAuto_, chkClip_, chkHist_, chkFirst_, radStr_, radJson_, radBoth_ };
    for (HWND c : kids) {
        if (!c) continue;
        HFONT f = fBody_;
        int id = GetDlgCtrlID(c);
        if (id == ID_BTN_SAVE) f = fBtn_;
        SendMessageW(c, WM_SETFONT, (WPARAM)f, TRUE);
    }
}

void App::BuildBrushes() {
    const Palette& pal = theme::Current();
    brAppBg_      = CreateSolidBrush(pal.canvas);
    brSurface_    = CreateSolidBrush(pal.surface);
    brSidebar_    = CreateSolidBrush(pal.surface);      // 2.0 结构无侧栏，与卡片同底
    brBorder_     = CreateSolidBrush(pal.border);
    brTitleInact_ = CreateSolidBrush(pal.surfaceAlt);  // 非激活标题栏：次级底色
    brAccentTint_ = CreateSolidBrush(pal.accentTint);
    brHover_      = CreateSolidBrush(pal.hover);
    brDangerTint_ = CreateSolidBrush(pal.dangerTint);
    penBorder_    = CreatePen(PS_SOLID, 1, pal.border);
}

void App::ReleaseUiResources() {
    HFONT fonts[] = { fTitle_, fNav_, fNavB_, fBody_, fCaption_, fBtn_, fCard_, fMdl2_ };
    for (HFONT f : fonts) if (f) DeleteObject(f);
    fTitle_ = fNav_ = fNavB_ = fBody_ = fCaption_ = fBtn_ = fCard_ = fMdl2_ = nullptr;
    HBRUSH brs[] = { brAppBg_, brSurface_, brSidebar_, brBorder_, brTitleInact_, brAccentTint_, brHover_, brDangerTint_ };
    for (HBRUSH b : brs) if (b) DeleteObject(b);
    brAppBg_ = brSurface_ = brSidebar_ = brBorder_ = brTitleInact_ = brAccentTint_ = brHover_ = brDangerTint_ = nullptr;
    if (penBorder_) { DeleteObject(penBorder_); penBorder_ = nullptr; }
}

void App::CreateControls() {
    // 顶部导航标签（2.0 结构）：浏览器 / 设置 / 说明
    navBrowser_ = CreateWindowW(L"BUTTON", L"浏览器", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 10, 10, main_, (HMENU)ID_NAV_BROWSER, inst_, nullptr);
    navSettings_ = CreateWindowW(L"BUTTON", L"设置", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 10, 10, main_, (HMENU)ID_NAV_SETTINGS, inst_, nullptr);
    navHelp_ = CreateWindowW(L"BUTTON", L"说明", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 10, 10, main_, (HMENU)ID_NAV_HELP, inst_, nullptr);

    // 浏览器页工具栏改由 HTML 渲染（tbHost_ + 第二个 WebView），不再创建原生地址框/按钮
    webHost_ = CreateWindowExW(0, kHostClass, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 10, 10, main_, nullptr, inst_, nullptr);
    // HTML 工具栏宿主（方案 A 原型）：与 webHost_ 同类，创建后置为不可见，
    // 由 SwitchPage/Layout 在浏览器页控制显隐
    tbHost_ = CreateWindowExW(0, kHostClass, L"", WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 10, 10, main_, nullptr, inst_, nullptr);

    // 设置页（2.0 表单）：预设下拉 / 登录网址 / 日志文件
    presetCombo_ = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
        0, 0, 10, 10, main_, (HMENU)ID_PRESET_COMBO, inst_, nullptr);
    loginUrlEdit_ = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
        0, 0, 10, 10, main_, (HMENU)ID_LOGIN_URL, inst_, nullptr);
    logFileEdit_ = CreateWindowW(L"EDIT", L"cookie.log", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
        0, 0, 10, 10, main_, (HMENU)ID_LOG_FILE, inst_, nullptr);

    // 抓取选项复选（2.0 文案）
    chkAuto_ = CreateWindowW(L"BUTTON", L"自动抓取（检测到登录后自动写入）",
        WS_CHILD | BS_OWNERDRAW | BS_AUTOCHECKBOX | BS_NOTIFY,
        0, 0, 10, 10, main_, (HMENU)ID_CHK_AUTO, inst_, nullptr);
    chkClip_ = CreateWindowW(L"BUTTON", L"抓取后复制到剪贴板",
        WS_CHILD | BS_OWNERDRAW | BS_AUTOCHECKBOX | BS_NOTIFY,
        0, 0, 10, 10, main_, (HMENU)ID_CHK_CLIP, inst_, nullptr);
    chkHist_ = CreateWindowW(L"BUTTON", L"记录历史日志（追加保存）",
        WS_CHILD | BS_OWNERDRAW | BS_AUTOCHECKBOX | BS_NOTIFY,
        0, 0, 10, 10, main_, (HMENU)ID_CHK_HIST, inst_, nullptr);
    chkFirst_ = CreateWindowW(L"BUTTON", L"首次打开若已是登录则自动抓取",
        WS_CHILD | BS_OWNERDRAW | BS_AUTOCHECKBOX | BS_NOTIFY,
        0, 0, 10, 10, main_, (HMENU)ID_CHK_FIRST, inst_, nullptr);

    // Cookie 格式单选（2.0 样式：圆点单选）
    radStr_ = CreateWindowW(L"BUTTON", L"字符串", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | BS_AUTORADIOBUTTON | WS_GROUP,
        0, 0, 10, 10, main_, (HMENU)ID_RAD_STR, inst_, nullptr);
    radJson_ = CreateWindowW(L"BUTTON", L"JSON", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | BS_AUTORADIOBUTTON,
        0, 0, 10, 10, main_, (HMENU)ID_RAD_JSON, inst_, nullptr);
    radBoth_ = CreateWindowW(L"BUTTON", L"两种都写", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | BS_AUTORADIOBUTTON,
        0, 0, 10, 10, main_, (HMENU)ID_RAD_BOTH, inst_, nullptr);

    // 设置页操作按钮（2.0）：保存设置（主色）/ 清除登录状态 / 打开日志文件夹
    btnSave_ = CreateWindowW(L"BUTTON", L"保存设置", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 10, 10, main_, (HMENU)ID_BTN_SAVE, inst_, nullptr);
    btnClear_ = CreateWindowW(L"BUTTON", L"清除登录状态", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 10, 10, main_, (HMENU)ID_BTN_CLEAR, inst_, nullptr);
    btnOpenLog_ = CreateWindowW(L"BUTTON", L"打开日志文件夹", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 10, 10, main_, (HMENU)ID_BTN_OPENLOG, inst_, nullptr);

    // 字体
    ApplyFonts();

    RefreshPresetList();
    SwitchPage(0);

    // 开关/分段状态（自管理）
    toggleOn_[0] = settings_.autoCapture;
    toggleOn_[1] = settings_.clipboard;
    toggleOn_[2] = settings_.history;
    toggleOn_[3] = settings_.firstOpenCapture;
    segSel_ = settings_.logFormat == LogFormat::Json ? 1 : (settings_.logFormat == LogFormat::Both ? 2 : 0);
}

// 设置页表单行几何（Layout 与 PaintMain 共用，保证标签与控件对齐）
struct SettingsRows {
    int pad, labelX, ctrlX, ctrlW, colW, chkW;
    int combo, login, logfile, radio, chk1, chk2, datadir, btns, hint, formH;
};
static SettingsRows SettingsFormMetrics(App* app, const RECT& card) {
    SettingsRows m;
    m.pad = app->S(24);
    int labelW = app->S(88);
    m.labelX = card.left + m.pad;
    m.ctrlX = m.labelX + labelW + app->S(12);
    m.ctrlW = card.right - m.pad - m.ctrlX;
    if (m.ctrlW > app->S(440)) m.ctrlW = app->S(440);
    if (m.ctrlW < app->S(200)) m.ctrlW = app->S(200);
    int y = app->S(6);
    m.combo = y; y += app->S(30) + app->S(16);
    m.login = y; y += app->S(30) + app->S(16);
    m.logfile = y; y += app->S(30) + app->S(16);
    m.radio = y; y += app->S(26) + app->S(14);
    m.chk1 = y; y += app->S(32);
    m.chk2 = y; y += app->S(24) + app->S(20);
    m.datadir = y; y += app->S(20) + app->S(16);
    m.btns = y; y += app->S(34) + app->S(12);
    m.hint = y;
    m.formH = y + app->S(18) + m.pad;
    m.colW = (card.right - m.pad - m.ctrlX) / 2;
    m.chkW = m.colW - app->S(12);
    return m;
}

void App::Layout() {
    if (!main_) return;
    RECT rc;
    GetClientRect(main_, &rc);
    int cx = rc.right, cy = rc.bottom;

    titleH_ = S(46);
    if (cx >= S(900)) mode_ = LayoutExpanded;
    else if (cx >= S(640)) mode_ = LayoutCompact;
    else mode_ = LayoutNarrow;
    sideW_ = 0;              // 2.0 结构：无侧栏，顶部导航条
    topNavH_ = S(52);

    // 标题栏按钮（右对齐，通高）
    int bw = S(46);
    tbRects_[2] = { cx - bw, 0, cx, titleH_ };
    tbRects_[1] = { cx - bw * 2, 0, cx - bw, titleH_ };
    tbRects_[0] = { cx - bw * 3, 0, cx - bw * 2, titleH_ };

    // 顶部导航标签（2.0）：品牌文字由 PaintMain 绘制，标签从固定起点横排
    int navH = topNavH_;
    int tabW = S(88);
    int tabX = S(236);
    MoveWindow(navBrowser_, tabX, titleH_, tabW, navH, TRUE);
    MoveWindow(navSettings_, tabX + tabW, titleH_, tabW, navH, TRUE);
    MoveWindow(navHelp_, tabX + tabW * 2, titleH_, tabW, navH, TRUE);

    // 全宽状态栏（2.0）
    int statusH = S(30);
    statusLine_ = { 0, cy - statusH, cx, cy };

    // 内容区
    int contentX = S(24);
    int contentY = titleH_ + navH + S(16);
    int contentW = cx - contentX * 2;
    if (contentW < S(80)) contentW = S(80);

    if (page_ == 0) {
        // 工具栏由 HTML/CSS 渲染（tbHost_）：卡片高度按宽度分三档，控件换行交给 CSS flex-wrap
        int cardW = contentW;
        int tbH = (cardW >= S(820)) ? S(64) : (cardW >= S(600) ? S(104) : S(150));
        // 外扩一圈画布底色（与 body padding 12px 逻辑像素对齐），圆角卡片与柔和投影由 HTML 绘制
        int M = S(12);
        int hostW = cardW + 2 * M, hostH = tbH + 2 * M;
        MoveWindow(tbHost_, contentX - M, contentY - M, hostW, hostH, TRUE);
        if (toolbarWebReady_) toolbarWv_.Resize(0, 0, hostW, hostH);

        // Web 卡片
        int webY = contentY + tbH + S(12);
        int webBottom = statusLine_.top - S(12);
        webCard_ = { contentX, webY, contentX + contentW, webBottom };
        MoveWindow(webHost_, webCard_.left + S(1), webCard_.top + S(1),
            webCard_.right - webCard_.left - S(2), webCard_.bottom - webCard_.top - S(2), TRUE);
        webview_.Resize(0, 0, webCard_.right - webCard_.left - S(2), webCard_.bottom - webCard_.top - S(2));
    } else if (page_ == 1) {
        // 设置页：单张大表单卡片（2.0 结构）
        settingsView_ = { contentX, contentY, contentX + contentW, statusLine_.top - S(12) };
        settingsViewH_ = settingsView_.bottom - settingsView_.top;
        SettingsRows m = SettingsFormMetrics(this, settingsView_);
        settingsContentH_ = m.formH;
        int maxScroll = settingsContentH_ - settingsViewH_;
        if (maxScroll < 0) maxScroll = 0;
        if (settingsScroll_ > maxScroll) settingsScroll_ = maxScroll;
        if (settingsScroll_ < 0) settingsScroll_ = 0;
        int sy = -settingsScroll_;
        RECT card = settingsView_;

        MoveWindow(presetCombo_, m.ctrlX, card.top + m.combo + sy, m.ctrlW, S(200), TRUE);
        MoveWindow(loginUrlEdit_, m.ctrlX, card.top + m.login + sy, m.ctrlW, S(30), TRUE);
        MoveWindow(logFileEdit_, m.ctrlX, card.top + m.logfile + sy, m.ctrlW, S(30), TRUE);
        int rw1 = S(92), rw2 = S(76), rw3 = S(116);
        MoveWindow(radStr_, m.ctrlX, card.top + m.radio + sy, rw1, S(26), TRUE);
        MoveWindow(radJson_, m.ctrlX + rw1, card.top + m.radio + sy, rw2, S(26), TRUE);
        MoveWindow(radBoth_, m.ctrlX + rw1 + rw2, card.top + m.radio + sy, rw3, S(26), TRUE);
        MoveWindow(chkAuto_, m.ctrlX, card.top + m.chk1 + sy, m.chkW, S(24), TRUE);
        MoveWindow(chkClip_, m.ctrlX + m.colW, card.top + m.chk1 + sy, m.chkW, S(24), TRUE);
        MoveWindow(chkHist_, m.ctrlX, card.top + m.chk2 + sy, m.chkW, S(24), TRUE);
        MoveWindow(chkFirst_, m.ctrlX + m.colW, card.top + m.chk2 + sy, m.chkW, S(24), TRUE);
        int saveW = S(110), clearW = S(140), logW = S(150);
        MoveWindow(btnSave_, m.ctrlX, card.top + m.btns + sy, saveW, S(34), TRUE);
        MoveWindow(btnClear_, m.ctrlX + saveW + S(12), card.top + m.btns + sy, clearW, S(34), TRUE);
        MoveWindow(btnOpenLog_, m.ctrlX + saveW + S(12) + clearW + S(12), card.top + m.btns + sy, logW, S(34), TRUE);

        // 滚动子控件必须被裁剪到内容视口内：子窗口的父窗口是主窗口，
        // 其位置不受 settingsView_ 限制——完全滚出视口后若不隐藏会盖住标题栏/状态行，
        // 部分滚出时若不裁剪会在视口外留下错位的残片
        HWND scrollKids[] = { presetCombo_, loginUrlEdit_, logFileEdit_,
            chkAuto_, chkClip_, chkHist_, chkFirst_, radStr_, radJson_, radBoth_,
            btnSave_, btnClear_, btnOpenLog_ };
        for (HWND c : scrollKids) {
            if (!c) continue;
            RECT wr;
            GetWindowRect(c, &wr);
            MapWindowPoints(nullptr, main_, (POINT*)&wr, 2);
            RECT inter;
            if (!IntersectRect(&inter, &wr, &settingsView_)) {
                ShowWindow(c, SW_HIDE);
                SetWindowRgn(c, nullptr, FALSE);
                continue;
            }
            ShowWindow(c, SW_SHOW);
            if (inter.left > wr.left || inter.top > wr.top ||
                inter.right < wr.right || inter.bottom < wr.bottom) {
                // 部分可见：按交集裁剪窗口区域（HRGN 交由系统接管，不要 DeleteObject）
                HRGN rgn = CreateRectRgn(inter.left - wr.left, inter.top - wr.top,
                    inter.right - wr.left, inter.bottom - wr.top);
                SetWindowRgn(c, rgn, TRUE);
            } else {
                SetWindowRgn(c, nullptr, FALSE);
            }
        }
    }
    // 关键：滚动/布局变更后必须让【整个客户区】失效。
    // 子控件被 MoveWindow 移动后在父窗口腾出的旧区域不会被系统擦除
    //（窗口类背景刷为 nullptr 且 WM_ERASEBKGND 返回 1 以配合双缓冲），
    // 若只失效内容视口，视口边缘/子控件腾出的旧像素无人覆盖 → 每次滚动累积成多重残影。
    InvalidateRect(main_, nullptr, FALSE);
}

void App::SwitchPage(int page) {
    page_ = page;
    settingsScroll_ = 0;
    HWND browserKids[] = { webHost_ };
    for (HWND c : browserKids) {
        ShowWindow(c, page_ == 0 ? SW_SHOW : SW_HIDE);
        if (page_ != 0) SetWindowRgn(c, nullptr, FALSE);
    }
    if (tbHost_) ShowWindow(tbHost_, page_ == 0 ? SW_SHOW : SW_HIDE);
    HWND settingsKids[] = { presetCombo_, loginUrlEdit_, logFileEdit_,
        chkAuto_, chkClip_, chkHist_, chkFirst_, radStr_, radJson_, radBoth_,
        btnSave_, btnClear_, btnOpenLog_ };
    for (HWND c : settingsKids) {
        ShowWindow(c, page_ == 1 ? SW_SHOW : SW_HIDE);
        if (page_ != 1) SetWindowRgn(c, nullptr, FALSE); // 切换页面时清掉滚动裁剪区
    }
    Layout();
    // 导航按钮为自绘子窗口，需显式刷新
    InvalidateRect(navBrowser_, nullptr, FALSE);
    InvalidateRect(navSettings_, nullptr, FALSE);
    InvalidateRect(navHelp_, nullptr, FALSE);
    InvalidateRect(main_, nullptr, FALSE);
}

void App::OpenUrlDirect(const std::wstring& urlIn) {
    std::wstring url = util::Trim(urlIn);
    if (url.empty()) return;
    if (!util::StartsWithI(url, L"http://") && !util::StartsWithI(url, L"https://") && !util::StartsWithI(url, L"file://"))
        url = L"https://" + url;
    startPageActive_ = false;
    SwitchPage(0);
    caption_ = CaptureManager::HostFromUrl(url);
    InvalidateRect(main_, nullptr, FALSE);
    webview_.Navigate(url);
}

void App::CaptureNow(int mask) {
    // mask: 1=仅 Cookie 2=仅 Token；两个按钮互不影响（采集路径独立）
    capture_.RequestCapture(true, mask);
}

void App::RefreshPage() {
    // 刷新：确保在浏览器页 -> 重新加载当前页面 -> 界面数据与状态随导航完成事件更新
    SwitchPage(0);
    if (!webview_.IsReady()) {
        OnCaptureStatus(L"页面尚未加载完成，请稍候再刷新");
        return;
    }
    OnCaptureStatus(L"正在刷新…");
    webview_.Reload();
    // 地址框/标题/计数随 NavigationCompleted 回调自动更新（成功或失败均会触发）
}

void App::ClearLoginState() {
    // 1) 破坏性操作确认
    if (MessageBoxW(main_,
            L"确定要清除登录状态吗？\n\n将删除所有已保存的 Cookie、Token 与网站登录会话，之后需要重新登录。",
            L"清除登录状态", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
        return;
    }
    // 2) 立即反馈（强制绘制，避免同步耗时期间界面无响应）
    OnCaptureStatus(L"正在清除登录状态…");
    UpdateWindow(main_);

    // 3) 尽力清除当前页面的 Web 存储（IndexedDB 等随数据目录删除一并清除）
    if (webview_.IsReady()) {
        webview_.ExecuteScript(L"try{localStorage.clear();sessionStorage.clear();}catch(e){}");
        webview_.DeleteAllCookies();
    }

    // 4) 关闭浏览器会话并删除数据目录，再重建（全新未登录状态）
    std::wstring userData = util::LocalAppDir() + L"\\userdata";
    bool recreated = webview_.Recreate(userData);
    bool dirGone = !util::DirExists(userData);

    // 5) 界面状态同步为未登录
    capture_.ResetState();
    caption_.clear();
    UpdateToolbarAddress(L"");
    if (recreated) {
        ShowStartPage();
        if (dirGone) {
            OnCaptureStatus(L"登录状态已清除，请重新登录");
        } else {
            OnCaptureStatus(L"登录状态已清除（数据目录有残留，重启应用后可完全清除）");
        }
    } else {
        OnCaptureStatus(L"清除失败：WebView2 重建失败，请重启应用后重试");
    }
    InvalidateRect(main_, nullptr, FALSE);
}

void App::RefreshPresetList() {
    // 预设下拉（2.0）：首项为「自定义（手动输入）」
    SendMessageW(presetCombo_, CB_RESETCONTENT, 0, 0);
    SendMessageW(presetCombo_, CB_ADDSTRING, 0, (LPARAM)L"自定义（手动输入）");
    for (auto& p : settings_.presets)
        SendMessageW(presetCombo_, CB_ADDSTRING, 0, (LPARAM)p.name.c_str());
    SendMessageW(presetCombo_, CB_SETCURSEL, 0, 0);
    SetWindowTextW(loginUrlEdit_, L"");
    // HTML 工具栏：就绪后重渲染，同步预设下拉与地址候选
    if (toolbarWebReady_) ShowToolbarHtml();
}

void App::OnPresetComboSel() {
    // 选择预设：回填登录网址；「自定义（手动输入）」清空网址由用户填写
    int sel = (int)SendMessageW(presetCombo_, CB_GETCURSEL, 0, 0);
    if (sel <= 0 || sel > (int)settings_.presets.size())
        SetWindowTextW(loginUrlEdit_, L"");
    else
        SetWindowTextW(loginUrlEdit_, settings_.presets[sel - 1].url.c_str());
    hintMsg_.clear();
    InvalidateRect(main_, nullptr, FALSE);
}

void App::SaveSettingsClicked() {
    SaveUiSettings();
    // 预设选中且登录网址被修改：持久化到该预设
    int sel = (int)SendMessageW(presetCombo_, CB_GETCURSEL, 0, 0);
    if (sel > 0 && sel <= (int)settings_.presets.size()) {
        wchar_t buf[2048] = {};
        GetWindowTextW(loginUrlEdit_, buf, 2048);
        std::wstring url = util::Trim(buf);
        if (!url.empty()) {
            if (!util::StartsWithI(url, L"http://") && !util::StartsWithI(url, L"https://") && !util::StartsWithI(url, L"file://"))
                url = L"https://" + url;
            if (url != settings_.presets[sel - 1].url) {
                settings_.presets[sel - 1].url = url;
                SaveSettings(settings_);
                RefreshPresetList();
                SendMessageW(presetCombo_, CB_SETCURSEL, (WPARAM)sel, 0);
                SetWindowTextW(loginUrlEdit_, url.c_str());
                if (startPageActive_) ShowStartPage();
            }
        }
    }
    hintMsg_ = L"设置已保存，立即生效。";
    InvalidateRect(main_, nullptr, FALSE);
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
    UpdateStatus();
}

void App::UpdateStatus() {
    InvalidateRect(main_, &statusLine_, FALSE);
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
        // 兜底：WebView2 迟迟不就绪（未安装运行时/创建失败）也必须让界面可见，
        // 此时 Web 区域由 HostWndProc 的 WM_PAINT 显示过渡提示
        KillTimer(main_, TIMER_SHOW_FALLBACK);
        ShowMainWindow();
        break;
    }
}

void App::KillCaptureTimers() { capture_.KillTimers(); }

void App::SetSettingsScroll(int v) {
    int maxScroll = settingsContentH_ - settingsViewH_;
    if (maxScroll < 0) maxScroll = 0;
    if (v > maxScroll) v = maxScroll;
    if (v < 0) v = 0;
    if (v == settingsScroll_) return;
    settingsScroll_ = v;
    Layout();
    // 滚动是即时交互：同步完成父窗口与所有子控件的重绘，
    // 避免消息队列里 WM_PAINT 被后续滚轮/鼠标消息挤压而出现的中间错位帧
    RedrawWindow(main_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

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
    // CSS 变量由当前调色板生成（theme::StartPageCss）：起始页与原生绘制同源，
    // 消除此前硬编码靛蓝 #4B62E6 与原生翠绿两套配色并存的问题，并跟随系统深浅色
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
            // 简单转义
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

// ==================== HTML 工具栏（方案 A 原型）====================
void App::InitHtmlToolbar() {
    if (!tbHost_) return;
    toolbarWv_.SetMessageHandler([this](const std::string& json) { OnToolbarMessage(json); });
    // 就绪通知同样投递回主窗口过程处理，避免在 WebView2 回调栈内直接导航
    toolbarWv_.SetReadyHandler([this]() {
        if (main_) PostMessageW(main_, WM_APP_TB_READY, 0, 0);
    });
    std::wstring userData = util::LocalAppDir() + L"\\userdata";  // 与主 WebView 共用（加载器已引用计数共享）
    toolbarWv_.Create(tbHost_, userData);
}

void App::OnHtmlToolbarReady() {
    toolbarWebReady_ = true;
    ShowToolbarHtml();
    Layout();
}

void App::ShowToolbarHtml() {
    std::vector<webtoolbar::Preset> presets;
    presets.reserve(settings_.presets.size());
    for (auto& p : settings_.presets) presets.push_back({ p.name, p.url });
    std::wstring addr = startPageActive_ ? std::wstring() : webview_.CurrentUrl();
    std::string html = webtoolbar::Build(presets, addr);
    toolbarWv_.NavigateToString(util::Utf8ToWide(html));
}

void App::OnToolbarMessage(const std::string& json) {
    // {"tb":"open","url":"..."} / {"tb":"refresh"} / {"tb":"capcookie","re":"..."} /
    // {"tb":"captoken","re":"..."} / {"tb":"regex","re":"..."}
    auto v = mjson::Parse(json);
    if (!v || v->type != mjson::Value::Object) return;
    std::string tb = v->GetStr("tb");
    // 正则框（Cookie 名筛选）：任何携带 re 字段的消息都先同步到抓取后端
    if (v->Has("re")) {
        capture_.SetCookieFilter(util::Utf8ToWide(v->GetStr("re")));
    }
    if (tb == "open") {
        OpenUrlDirect(util::Utf8ToWide(v->GetStr("url")));
    } else if (tb == "refresh") {
        RefreshPage();
    } else if (tb == "capcookie") {
        CaptureNow(1);
    } else if (tb == "captoken") {
        CaptureNow(2);
    }
    // tb == "regex"：仅更新筛选，不触发抓取
}

void App::UpdateToolbarAddress(const std::wstring& url) {
    if (!toolbarWebReady_) return;
    // 经 mjson::Escape 转义后嵌入 JS 双引号串（兼容 \" \\ \u 序列）
    std::string esc = mjson::Escape(util::WideToUtf8(url));
    std::wstring js = L"if(window.__cgSetAddress)__cgSetAddress(\"" + util::Utf8ToWide(esc) + L"\");";
    toolbarWv_.ExecuteScript(js);
}

// ==================== 自绘控件 ====================
static bool NavSelected(int id, int page) {
    return (id == ID_NAV_BROWSER && page == 0) || (id == ID_NAV_SETTINGS && page == 1) ||
           (id == ID_NAV_HELP && page == 2);
}

static void DrawNavButton(const DRAWITEMSTRUCT* dis, App* app) {
    HDC dc = dis->hDC;
    RECT rc = dis->rcItem;
    const Palette& pal = theme::Current();
    // 背景：owner-draw BUTTON 的擦除行为不可靠，必须自行填充底色，
    // 否则旧帧像素（如上一次的选中态）会永久残留
    RECT rb = dis->rcItem;
    HBRUSH bgs = CreateSolidBrush(pal.surface);
    FillRect(dc, &rb, bgs);
    DeleteObject(bgs);
    int id = GetDlgCtrlID(dis->hwndItem);
    bool sel = NavSelected(id, app->Page());
    bool hot = app->HoverCtl() == dis->hwndItem;
    // 2.0 风格：纯文本标签，选中 = 主色文字 + 底部下划线
    COLORREF tc = sel ? pal.accentText : (hot ? pal.text : pal.text2);
    wchar_t label[32] = {};
    GetWindowTextW(dis->hwndItem, label, 32);
    TextOut2(dc, label, rc, sel ? app->FontNavB() : app->FontNav(), tc,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (sel) {
        int uw = (rc.right - rc.left) - app->S(28);
        int ux = rc.left + app->S(14);
        int uy = rc.bottom - app->S(4);
        FillRoundRect(dc, { ux, uy, ux + uw, uy + app->S(3) }, app->S(2), pal.accent, CLR_NONE);
    }
}

static void DrawModernButton(const DRAWITEMSTRUCT* dis, App* app) {
    HDC dc = dis->hDC;
    RECT rc = dis->rcItem;
    int id = GetDlgCtrlID(dis->hwndItem);
    bool hot = app->HoverCtl() == dis->hwndItem;
    bool press = (dis->itemState & ODS_SELECTED) != 0;
    bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    const Palette& pal = theme::Current();
    int r = app->S(8);
    // 状态色与 ui.cpp draw::Button 同语义（BtnPrimary/BtnSecondary/BtnDanger）
    COLORREF fill = CLR_NONE, border = CLR_NONE, text = pal.text;
    if (disabled) {
        fill = pal.track; text = pal.text3;
    } else if (id == ID_BTN_SAVE) {
        fill = press ? pal.accentActive : (hot ? pal.accentHover : pal.accentFill);
        text = pal.onAccent;
    } else if (id == ID_BTN_OPENLOG) {
        fill = press ? pal.pressed : pal.surface;
        border = pal.border;
    } else if (id == ID_BTN_CLEAR) {
        fill = (hot || press) ? pal.dangerTint : pal.surface;
        border = (hot || press) ? pal.danger : pal.border;
        text = pal.danger;
    } else {
        fill = (hot || press) ? pal.hover : CLR_NONE;
        text = pal.text2;
    }
    // 自填背景：owner-draw BUTTON 擦除不可靠，透明态须显式铺卡片底色
    if (fill == CLR_NONE) {
        RECT rb = dis->rcItem;
        HBRUSH bgs = CreateSolidBrush(pal.surface);
        FillRect(dc, &rb, bgs);
        DeleteObject(bgs);
    }
    if (fill != CLR_NONE || border != CLR_NONE) FillRoundRect(dc, rc, r, fill, border);
    wchar_t t[64] = {};
    GetWindowTextW(dis->hwndItem, t, 64);
    // 图标（清除登录状态），图标在左、文字在右，整体居中
    const wchar_t* glyph = nullptr;
    if (id == ID_BTN_CLEAR) glyph = L"\uE107";
    int iconSize = app->S(16), gap = app->S(6);
    if (glyph && t[0]) {
        SelectObject(dc, app->FontBtn());
        SIZE ts; GetTextExtentPoint32W(dc, t, (int)wcslen(t), &ts);
        int total = iconSize + gap + ts.cx;
        int startX = rc.left + (rc.right - rc.left - total) / 2;
        int midY = (rc.top + rc.bottom) / 2;
        RECT ir = { startX, midY - iconSize / 2, startX + iconSize, midY + iconSize / 2 };
        TextOut2(dc, glyph, ir, app->FontMdl2(), text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        RECT tr = { startX + iconSize + gap, rc.top, rc.right - app->S(6), rc.bottom };
        TextOut2(dc, t, tr, app->FontBtn(), text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    } else {
        TextOut2(dc, t, rc, app->FontBtn(), text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    // 焦点提示：不能再用 DrawFocusRect——它是 XOR 点状【直角】框，
    // 四角落在圆角按钮之外，看起来就是一条被裁掉的影子边；XOR 还会在彩色填充上反色。
    // 改为内缩的圆角焦点环，且只在键盘导航时显示（鼠标点击由 App::SetFocusCue 收起）。
    if (app->FocusCueOn() && (dis->itemState & ODS_FOCUS)) {
        int inset = app->S(2);
        RECT fr = { rc.left + inset, rc.top + inset, rc.right - inset, rc.bottom - inset };
        bool onAccent = (fill == pal.accentFill || fill == pal.accentHover || fill == pal.accentActive);
        FillRoundRect(dc, fr, r - inset, CLR_NONE, onAccent ? pal.onAccent : pal.focusRing, app->S(2));
    }
}

static int ToggleIndex(int id) {
    switch (id) {
    case ID_CHK_AUTO: return 0;
    case ID_CHK_CLIP: return 1;
    case ID_CHK_HIST: return 2;
    case ID_CHK_FIRST: return 3;
    }
    return -1;
}

static void DrawToggle(const DRAWITEMSTRUCT* dis, App* app) {
    HDC dc = dis->hDC;
    RECT rc = dis->rcItem;
    const Palette& pal = theme::Current();
    // 自填背景，防止 hover 灰底/旧帧残留（BUTTON 擦除不可靠）
    RECT rb = dis->rcItem;
    HBRUSH bgs = CreateSolidBrush(pal.surface);
    FillRect(dc, &rb, bgs);
    DeleteObject(bgs);
    int idx = ToggleIndex(GetDlgCtrlID(dis->hwndItem));
    bool on = idx >= 0 && app->ToggleOn(idx);
    // 2.0 样式：方框复选 + 对勾
    int bs = app->S(16);
    int bx = rc.left + app->S(2);
    int by = rc.top + (rc.bottom - rc.top - bs) / 2;
    FillRoundRect(dc, { bx, by, bx + bs, by + bs }, app->S(4),
        on ? pal.accentFill : pal.surface, on ? CLR_NONE : pal.border);
    if (on) {
        HPEN pen = CreatePen(PS_SOLID, app->S(2), pal.onAccent);
        HGDIOBJ op = SelectObject(dc, pen);
        MoveToEx(dc, bx + bs * 24 / 100, by + bs * 52 / 100, nullptr);
        LineTo(dc, bx + bs * 40 / 100, by + bs * 70 / 100);
        LineTo(dc, bx + bs * 76 / 100, by + bs * 30 / 100);
        SelectObject(dc, op); DeleteObject(pen);
    }
    wchar_t t[160] = {};
    GetWindowTextW(dis->hwndItem, t, 160);
    RECT tr = { bx + bs + app->S(10), rc.top, rc.right - app->S(4), rc.bottom };
    TextOut2(dc, t, tr, app->FontBody(), pal.text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static int SegIndex(int id) {
    switch (id) {
    case ID_RAD_STR: return 0;
    case ID_RAD_JSON: return 1;
    case ID_RAD_BOTH: return 2;
    }
    return -1;
}

static void DrawSegment(const DRAWITEMSTRUCT* dis, App* app) {
    HDC dc = dis->hDC;
    RECT rc = dis->rcItem;
    const Palette& pal = theme::Current();
    // 自填背景，防止上次选中态残留（owner-draw 擦除不可靠）
    RECT rb = dis->rcItem;
    HBRUSH bgs = CreateSolidBrush(pal.surface);
    FillRect(dc, &rb, bgs);
    DeleteObject(bgs);
    int idx = SegIndex(GetDlgCtrlID(dis->hwndItem));
    bool on = idx >= 0 && app->SegSel() == idx;
    bool hot = app->HoverCtl() == dis->hwndItem;
    // 2.0 样式：圆点单选
    int r = app->S(7);
    int cx = rc.left + r + app->S(2);
    int cy = (rc.top + rc.bottom) / 2;
    HPEN pen = CreatePen(PS_SOLID, app->S(2), on ? pal.accent : (hot ? pal.text2 : pal.text3));
    HGDIOBJ op = SelectObject(dc, pen);
    HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Ellipse(dc, cx - r, cy - r, cx + r, cy + r);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(pen);
    if (on) {
        HBRUSH br = CreateSolidBrush(pal.accent);
        HGDIOBJ ob2 = SelectObject(dc, br);
        int r2 = r - app->S(3);
        Ellipse(dc, cx - r2, cy - r2, cx + r2, cy + r2);
        SelectObject(dc, ob2);
        DeleteObject(br);
    }
    wchar_t t[32] = {};
    GetWindowTextW(dis->hwndItem, t, 32);
    RECT tr = { cx + r + app->S(8), rc.top, rc.right, rc.bottom };
    TextOut2(dc, t, tr, app->FontBody(), on ? pal.text : pal.text2,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

// ==================== 主窗口绘制 ====================
static void DrawTitleButton(HDC dc, App* app, HWND hwnd, int i) {
    RECT rc = app->TitleBtnRect(i);
    bool hot = app->TitleBtnHot() == i;
    bool press = app->TitleBtnPress() == i;
    const Palette& pal = theme::Current();
    COLORREF bg = CLR_NONE, glyph = pal.text;
    if (i == 2) {
        // 关闭按钮：与 draw::Button(BtnDanger) 同语义（hot 浅红底红字 / press 红底白字），不再私设 RGB
        if (press) { bg = pal.danger; glyph = pal.onAccent; }
        else if (hot) { bg = pal.dangerTint; glyph = pal.danger; }
    } else {
        if (press) bg = pal.pressed;
        else if (hot) bg = pal.hover;
    }
    if (bg != CLR_NONE) {
        HBRUSH br = CreateSolidBrush(bg);
        FillRect(dc, &rc, br);
        DeleteObject(br);
    }
    const wchar_t* g = i == 0 ? L"\uE921" : (i == 1 ? (IsZoomed(hwnd) ? L"\uE923" : L"\uE922") : L"\uE8BB");
    TextOut2(dc, g, rc, app->FontMdl2(), glyph, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static RECT ScrollBarRect(App* app) {
    RECT v = app->SettingsViewRect();
    return { v.right - app->S(10), v.top + app->S(2), v.right - app->S(4), v.bottom - app->S(2) };
}

static void PaintMain(HWND hwnd, HDC dc, const RECT& rc) {
    App* app = g_app;
    const Palette& pal = theme::Current();
    int titleH = app->TitleH();
    // 内容底
    FillRect(dc, &rc, app->BrushAppBg());

    // 标题栏
    RECT tb = { 0, 0, rc.right, titleH };
    FillRect(dc, &tb, app->IsActive() ? app->BrushSurface() : app->BrushTitleInact());
    RECT tbLine = { 0, titleH - 1, rc.right, titleH };
    FillRect(dc, &tbLine, app->BrushBorder());
    // 图标 + 标题 + 摘要
    int iconS = app->S(16);
    DrawIconEx(dc, app->S(16), (titleH - iconS) / 2, (HICON)GetClassLongPtrW(hwnd, GCLP_HICON),
        iconS, iconS, 0, nullptr, DI_NORMAL);
    std::wstring title = L"Cookie 抓取工具";
    if (!app->CaptionText().empty()) title += L"   ·   " + app->CaptionText();
    RECT tr = { app->S(42), 0, app->TitleBtnRect(0).left - app->S(8), titleH };
    TextOut2(dc, title, tr, app->FontTitle(), app->IsActive() ? pal.text : pal.text3,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    for (int i = 0; i < 3; i++) DrawTitleButton(dc, app, hwnd, i);

    // 顶部导航条（2.0 结构）：品牌 + 文本标签（选中下划线由自绘控件绘制）
    RECT nav = { 0, titleH, rc.right, titleH + app->TopNavH() };
    FillRect(dc, &nav, app->BrushSurface());
    RECT nl = { 0, nav.bottom - 1, rc.right, nav.bottom };
    FillRect(dc, &nl, app->BrushBorder());
    {
        int bx = app->S(24);
        SelectObject(dc, app->FontTitle());
        const wchar_t* brand = L"Cookie抓取工具";
        SIZE bs; GetTextExtentPoint32W(dc, brand, 10, &bs);
        RECT br = { bx, nav.top, bx + bs.cx + app->S(4), nav.bottom };
        TextOut2(dc, brand, br, app->FontTitle(), pal.accentText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT vr = { bx + bs.cx + app->S(8), nav.top, bx + bs.cx + app->S(64), nav.bottom };
        TextOut2(dc, L"v3.0", vr, app->FontCaption(), pal.text3, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // 全宽状态栏（2.0）：● 状态文字 … Cookie/Token 计数 … 日志路径
    RECT sl = app->statusLine_;
    FillRect(dc, &sl, app->BrushSurface());
    RECT slLine = { sl.left, sl.top, sl.right, sl.top + 1 };
    FillRect(dc, &slLine, app->BrushBorder());
    {
        COLORREF dot = pal.text3;
        const std::wstring& st = app->StatusText();
        if (st.find(L"成功") != std::wstring::npos) dot = pal.success;
        else if (st.find(L"失败") != std::wstring::npos || st.find(L"错误") != std::wstring::npos) dot = pal.danger;
        else if (st.find(L"正在") != std::wstring::npos || st.find(L"抓取中") != std::wstring::npos) dot = pal.accent;
        int cy = (sl.top + sl.bottom) / 2;
        int dr = app->S(4);
        DrawCookieDot(dc, sl.left + app->S(16), cy, dr, dot);
        std::wstring status = st.empty() ? L"就绪" : st;
        // 最右侧：日志路径（按实际文字宽度定位，超长省略到 300px）
        int dirMaxW = app->S(300);
        int dirRight = sl.right - app->S(14);
        const std::wstring& dir = app->LogDir();
        SelectObject(dc, app->FontCaption());
        SIZE ds; GetTextExtentPoint32W(dc, dir.c_str(), (int)dir.size(), &ds);
        int dirLeft = dirRight - (ds.cx < dirMaxW ? ds.cx : dirMaxW);
        // 紧贴路径文字左侧：Cookie / Token 水平并排（一行），标签灰、数字主色绿
        std::wstring l1 = L"Cookie ", n1 = std::to_wstring(app->CookieCount());
        std::wstring l2 = L"Token ", n2 = std::to_wstring(app->TokenCount());
        SelectObject(dc, app->FontBtn());
        SIZE lw1, nw1, lw2, nw2;
        GetTextExtentPoint32W(dc, l1.c_str(), (int)l1.size(), &lw1);
        GetTextExtentPoint32W(dc, n1.c_str(), (int)n1.size(), &nw1);
        GetTextExtentPoint32W(dc, l2.c_str(), (int)l2.size(), &lw2);
        GetTextExtentPoint32W(dc, n2.c_str(), (int)n2.size(), &nw2);
        int gap = app->S(16);
        int total = lw1.cx + nw1.cx + gap + lw2.cx + nw2.cx;
        int countsRight = dirLeft - app->S(6);
        int countsLeft = countsRight - total;
        int x = countsLeft;
        RECT r1 = { x, sl.top, x + lw1.cx, sl.bottom };
        TextOut2(dc, l1, r1, app->FontBtn(), pal.text2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        x += lw1.cx;
        RECT r2 = { x, sl.top, x + nw1.cx, sl.bottom };
        TextOut2(dc, n1, r2, app->FontBtn(), pal.accentText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        x += nw1.cx + gap;
        RECT r3 = { x, sl.top, x + lw2.cx, sl.bottom };
        TextOut2(dc, l2, r3, app->FontBtn(), pal.text2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        x += lw2.cx;
        RECT r4 = { x, sl.top, x + nw2.cx, sl.bottom };
        TextOut2(dc, n2, r4, app->FontBtn(), pal.accentText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        // 状态文字（左侧，到计数边界为止）
        int statusRight = countsLeft - app->S(10);
        if (statusRight < sl.left + app->S(160)) statusRight = sl.left + app->S(160);
        RECT sr = { sl.left + app->S(30), sl.top, statusRight, sl.bottom };
        TextOut2(dc, status, sr, app->FontBody(), pal.text2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        RECT lr = { dirLeft, sl.top, dirRight, sl.bottom };
        TextOut2(dc, dir, lr, app->FontCaption(), pal.text3,
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS);
    }

    if (app->Page() == 0) {
        // 工具栏卡片由 HTML 工具栏（WebView2）自行绘制，主窗口仅保留 Web 卡片底
        FillRoundRect(dc, app->webCard_, app->S(10), pal.surface, pal.border);
    } else if (app->Page() == 1) {
        // 设置页：单张大表单卡片（2.0 结构）
        RECT view = app->SettingsViewRect();
        FillRoundRect(dc, view, app->S(12), pal.surface, pal.border);
        SettingsRows m = SettingsFormMetrics(app, view);
        int sy = -app->SettingsScroll();
        HRGN clip = CreateRectRgn(view.left, view.top, view.right, view.bottom);
        SelectClipRgn(dc, clip);
        auto label = [&](int row, const wchar_t* t) {
            RECT lr = { m.labelX, view.top + row + sy, m.labelX + app->S(88), view.top + row + sy + app->S(26) };
            TextOut2(dc, t, lr, app->FontBody(), pal.text2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        };
        label(m.combo, L"网站预设");
        label(m.login, L"登录网址");
        label(m.logfile, L"日志文件");
        label(m.radio, L"Cookie 格式");
        label(m.datadir, L"数据目录");
        RECT dvr = { m.ctrlX, view.top + m.datadir + sy, view.right - m.pad, view.top + m.datadir + sy + app->S(20) };
        TextOut2(dc, app->LogDir(), dvr, app->FontCaption(), pal.text3,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS);
        std::wstring hint = app->HintText();
        if (hint.empty()) hint = L"提示：修改后点击「保存设置」立即生效。";
        RECT hr = { m.labelX, view.top + m.hint + sy, view.right - m.pad, view.top + m.hint + sy + app->S(18) };
        TextOut2(dc, hint, hr, app->FontCaption(), pal.text3, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectClipRgn(dc, nullptr);
        DeleteObject(clip);

        // 滚动条
        if (app->SettingsContentH() > app->SettingsViewH()) {
            RECT sb = ScrollBarRect(app);
            int trackW = sb.right - sb.left;
            int viewH = view.bottom - view.top;
            int thumbH = (int)((int64_t)viewH * viewH / app->SettingsContentH());
            if (thumbH < app->S(28)) thumbH = app->S(28);
            int maxScroll = app->SettingsContentH() - viewH;
            int ty = view.top + (int)((int64_t)app->SettingsScroll() * (viewH - thumbH) / maxScroll);
            RECT thumb = { sb.left, ty, sb.right, ty + thumbH };
            FillRoundRect(dc, sb, trackW / 2, pal.track, CLR_NONE);
            // thumb 与 draw::Scrollbar 同语义：hot/drag 用主色，常态用 knobOff（不再私设 RGB）
            FillRoundRect(dc, thumb, trackW / 2, app->ScrollBarHot() || app->ScrollBarDragging() ? pal.accent : pal.knobOff, CLR_NONE);
        }
    } else {
        // 说明页（2.0 第三标签）
        RECT card = { app->S(24), titleH + app->TopNavH() + app->S(16),
            rc.right - app->S(24), app->statusLine_.top - app->S(12) };
        FillRoundRect(dc, card, app->S(12), pal.surface, pal.border);
        int pad = app->S(28);
        int y = card.top + pad;
        RECT tr = { card.left + pad, y, card.right - pad, y + app->S(30) };
        TextOut2(dc, L"使用说明", tr, app->FontCard(), pal.text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        y += app->S(46);
        struct HelpLine { const wchar_t* head; const wchar_t* body; };
        HelpLine lines[] = {
            { L"浏览器", L"地址栏输入网址后点击「打开」；登录网站后即可抓取 Cookie / Token。" },
            { L"立即抓取Cookie", L"仅拉取当前站点的 Cookie 并写入日志（cookie.log / cookie.json / cookie_detail.json），不扫描页面存储。" },
            { L"立即抓取Token", L"仅扫描当前页面存储中的 Token（localStorage / sessionStorage 等），不读取 Cookie。" },
            { L"设置", L"选择网站预设回填登录网址、切换 Cookie 格式（字符串 / JSON / 两种都写）、勾选抓取选项，点击「保存设置」立即生效。" },
            { L"数据与日志", L"「打开日志文件夹」定位日志输出目录；cookie.log 每次抓取覆盖写，cookie_history.log 追加保存全部抓取记录。" },
            { L"清除登录状态", L"删除本程序保存的全部登录数据（Cookie、Token、网站会话），操作不可恢复，需重新登录。" },
        };
        for (auto& ln : lines) {
            RECT h1 = { card.left + pad, y, card.left + pad + app->S(140), y + app->S(22) };
            TextOut2(dc, ln.head, h1, app->FontBtn(), pal.accentText, DT_LEFT | DT_TOP | DT_SINGLELINE);
            RECT b1 = { card.left + pad + app->S(150), y, card.right - pad, y + app->S(42) };
            TextOut2(dc, ln.body, b1, app->FontBody(), pal.text2, DT_LEFT | DT_TOP | DT_WORDBREAK);
            y += app->S(48);
        }
    }
}

// ==================== 主窗口过程 ====================
static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    App* app = g_app;
    switch (msg) {
    case WM_CREATE:
        app->main_ = hwnd;
        app->SetDpi(GetDpiForWindow(hwnd));
        app->BuildFonts();
        app->BuildBrushes();
        app->CreateControls();
        app->Layout();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        // 双缓冲缓存：位图仅在尺寸变化时重建，高频滚动不再反复分配大位图；
        // 分配失败时降级为直接绘制，保证内容正确（不会出现 1x1 位图拉伸的模糊）
        app->EnsureMemBuffer(dc, rc.right, rc.bottom);
        if (app->MemDC() && app->MemBmp()) {
            PaintMain(hwnd, app->MemDC(), rc);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, app->MemDC(), 0, 0, SRCCOPY);
        } else {
            PaintMain(hwnd, dc, rc);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SIZE:
        app->Layout();
        InvalidateRect(hwnd, nullptr, FALSE); // 尺寸变化需全幅重绘（双缓冲缓存自动重建）
        return 0;
    case WM_DPICHANGED: {
        RECT* rc = (RECT*)lp;
        SetWindowPos(hwnd, nullptr, rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        app->SetDpi(HIWORD(wp));
        app->ReleaseUiResources();
        app->BuildFonts();
        app->BuildBrushes();
        app->ApplyFonts();
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
        POINT pt = { PtX(lp), PtY(lp) };
        ScreenToClient(hwnd, &pt);
        RECT wr;
        GetClientRect(hwnd, &wr);
        int w = wr.right, h = wr.bottom;
        int b = app->S(5);
        bool maxed = IsZoomed(hwnd);
        for (int i = 0; i < 3; i++) {
            RECT r = app->TitleBtnRect(i);
            if (PtInRect(&r, pt)) return HTCLIENT;
        }
        if (pt.y < app->TitleH()) return HTCAPTION;
        if (!maxed) {
            if (pt.x < b) return (pt.y < b) ? HTTOPLEFT : (pt.y > h - b ? HTBOTTOMLEFT : HTLEFT);
            if (pt.x > w - b) return (pt.y < b) ? HTTOPRIGHT : (pt.y > h - b ? HTBOTTOMRIGHT : HTRIGHT);
            if (pt.y < b) return HTTOP;
            if (pt.y > h - b) return HTBOTTOM;
        }
        return HTCLIENT;
    }
    case WM_NCACTIVATE:
        app->SetActive(wp != 0);
        InvalidateRect(hwnd, nullptr, FALSE);
        return TRUE;
    case WM_ACTIVATE:
        app->SetActive(LOWORD(wp) != WA_INACTIVE);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDBLCLK: {
        POINT pt = { PtX(lp), PtY(lp) };
        if (pt.y < app->TitleH()) {
            bool onBtn = false;
            for (int i = 0; i < 3; i++) { RECT r = app->TitleBtnRect(i); if (PtInRect(&r, pt)) onBtn = true; }
            if (!onBtn) ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = { PtX(lp), PtY(lp) };
        // 标题栏按钮
        if (pt.y < app->TitleH()) {
            for (int i = 0; i < 3; i++) {
                RECT r = app->TitleBtnRect(i);
                if (PtInRect(&r, pt)) {
                    app->SetTitleBtnPress(i);
                    SetCapture(hwnd);
                    InvalidateRect(hwnd, &r, FALSE);
                    return 0;
                }
            }
        }
        // 设置页滚动条
        if (app->Page() == 1 && app->SettingsContentH() > app->SettingsViewH()) {
            RECT sb = ScrollBarRect(app);
            if (PtInRect(&sb, pt)) {
                RECT view = app->SettingsViewRect();
                int viewH = view.bottom - view.top;
                int thumbH = (int)((int64_t)viewH * viewH / app->SettingsContentH());
                if (thumbH < app->S(28)) thumbH = app->S(28);
                int maxScroll = app->SettingsContentH() - viewH;
                int ty = view.top + (int)((int64_t)app->SettingsScroll() * (viewH - thumbH) / maxScroll);
                RECT thumb = { sb.left, ty, sb.right, ty + thumbH };
                SetCapture(hwnd);
                if (PtInRect(&thumb, pt)) {
                    app->SetScrollBarDragging(true);
                    app->SetSbDragOffset(pt.y - ty);
                } else {
                    // 点击轨道：跳转
                    int target = (int)((int64_t)(pt.y - view.top - thumbH / 2) * maxScroll / (viewH - thumbH));
                    app->SetSettingsScroll(target);
                }
                return 0;
            }
        }
        break;
    }
    case WM_LBUTTONUP: {
        if (app->TitleBtnPress() >= 0) {
            int i = app->TitleBtnPress();
            POINT pt = { PtX(lp), PtY(lp) };
            RECT r = app->TitleBtnRect(i);
            ReleaseCapture();
            app->SetTitleBtnPress(-1);
            if (PtInRect(&r, pt)) app->TitleBarButton(i);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (app->ScrollBarDragging()) {
            ReleaseCapture();
            app->SetScrollBarDragging(false);
            return 0;
        }
        break;
    }
    case WM_MOUSEMOVE: {
        POINT pt = { PtX(lp), PtY(lp) };
        // 标题栏按钮悬停
        int hot = -1;
        if (pt.y < app->TitleH()) {
            for (int i = 0; i < 3; i++) { RECT r = app->TitleBtnRect(i); if (PtInRect(&r, pt)) hot = i; }
        }
        if (hot != app->TitleBtnHot()) {
            int old = app->TitleBtnHot();
            app->SetTitleBtnHot(hot);
            if (old >= 0) { RECT r = app->TitleBtnRect(old); InvalidateRect(hwnd, &r, FALSE); }
            if (hot >= 0) { RECT r = app->TitleBtnRect(hot); InvalidateRect(hwnd, &r, FALSE); }
        }
        // 控件悬停
        HWND under = ChildWindowFromPoint(hwnd, pt);
        if (under && !IsWindowVisible(under)) under = nullptr;
        if (under && !IsWindowEnabled(under)) under = nullptr;
        int cid = under ? GetDlgCtrlID(under) : -1;
        bool trackable = cid == ID_NAV_BROWSER || cid == ID_NAV_SETTINGS || cid == ID_NAV_HELP ||
            cid == ID_BTN_CLEAR || cid == ID_BTN_SAVE || cid == ID_BTN_OPENLOG ||
            cid == ID_CHK_AUTO || cid == ID_CHK_CLIP || cid == ID_CHK_HIST ||
            cid == ID_CHK_FIRST || cid == ID_RAD_STR || cid == ID_RAD_JSON || cid == ID_RAD_BOTH;
        HWND newHover = trackable ? under : nullptr;
        if (newHover != app->HoverCtl()) {
            HWND old = app->HoverCtl();
            app->SetHoverCtl(newHover);
            if (old) InvalidateRect(old, nullptr, FALSE);
            if (newHover) InvalidateRect(newHover, nullptr, FALSE);
        }
        // 滚动条拖动
        if (app->ScrollBarDragging()) {
            RECT view = app->SettingsViewRect();
            int viewH = view.bottom - view.top;
            int thumbH = (int)((int64_t)viewH * viewH / app->SettingsContentH());
            if (thumbH < app->S(28)) thumbH = app->S(28);
            int maxScroll = app->SettingsContentH() - viewH;
            int range = viewH - thumbH;
            if (range > 0) {
                int target = (int)((int64_t)(pt.y - app->SbDragOffset() - view.top) * maxScroll / range);
                app->SetSettingsScroll(target);
            }
        }
        // 滚动条悬停
        if (app->Page() == 1 && app->SettingsContentH() > app->SettingsViewH() && !app->ScrollBarDragging()) {
            RECT sb = ScrollBarRect(app);
            bool hotSb = PtInRect(&sb, pt) != FALSE;
            if (hotSb != app->ScrollBarHot()) {
                app->SetScrollBarHot(hotSb);
                InvalidateRect(hwnd, &sb, FALSE);
            }
        }
        if (!app->Tracking()) {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            app->SetTracking(true);
        }
        break;
    }
    case WM_MOUSELEAVE:
        app->SetTracking(false);
        if (app->TitleBtnHot() != -1) {
            int old = app->TitleBtnHot();
            app->SetTitleBtnHot(-1);
            RECT r = app->TitleBtnRect(old);
            InvalidateRect(hwnd, &r, FALSE);
        }
        if (app->HoverCtl()) {
            HWND old = app->HoverCtl();
            app->SetHoverCtl(nullptr);
            InvalidateRect(old, nullptr, FALSE);
        }
        if (app->ScrollBarHot()) {
            app->SetScrollBarHot(false);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_CAPTURECHANGED:
        if (app->TitleBtnPress() >= 0) {
            app->SetTitleBtnPress(-1);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        if (app->ScrollBarDragging()) app->SetScrollBarDragging(false);
        return 0;
    case WM_MOUSEWHEEL:
        if (app->Page() == 1 && app->SettingsContentH() > app->SettingsViewH()) {
            // 合并队列中积压的连续滚轮事件，洪流/高频滚动下只渲染最终位置
            int step = WheelDelta(wp) / 120 * app->S(90);
            MSG pending;
            while (PeekMessageW(&pending, hwnd, WM_MOUSEWHEEL, WM_MOUSEWHEEL, PM_REMOVE)) {
                step += WheelDelta(pending.wParam) / 120 * app->S(90);
            }
            app->SetSettingsScroll(app->SettingsScroll() - step);
            return 0;
        }
        break;
    case WM_DRAWITEM: {
        auto* dis = (DRAWITEMSTRUCT*)lp;
        int id = dis->CtlID;
        if (id == ID_NAV_BROWSER || id == ID_NAV_SETTINGS || id == ID_NAV_HELP) { DrawNavButton(dis, app); return TRUE; }
        if (id == ID_BTN_CLEAR || id == ID_BTN_SAVE || id == ID_BTN_OPENLOG) {
            DrawModernButton(dis, app); return TRUE;
        }
        if (id == ID_CHK_AUTO || id == ID_CHK_CLIP || id == ID_CHK_HIST || id == ID_CHK_FIRST) {
            DrawToggle(dis, app); return TRUE;
        }
        if (id == ID_RAD_STR || id == ID_RAD_JSON || id == ID_RAD_BOTH) { DrawSegment(dis, app); return TRUE; }
        break;
    }
    case WM_CTLCOLORBTN: {
        // owner-draw BUTTON 的背景擦除刷：不用系统默认（COLOR_BTNFACE 灰）
        return (LRESULT)app->BrushSurface();
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = (HDC)wp;
        SetBkColor(dc, theme::Current().surface);
        SetTextColor(dc, theme::Current().text);
        return (LRESULT)app->BrushSurface();
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        int code = HIWORD(wp);
        switch (id) {
        case ID_NAV_BROWSER: if (code == BN_CLICKED) app->SwitchPage(0); return 0;
        case ID_NAV_SETTINGS: if (code == BN_CLICKED) app->SwitchPage(1); return 0;
        case ID_NAV_HELP: if (code == BN_CLICKED) app->SwitchPage(2); return 0;
        case ID_BTN_CLEAR: app->ClearLoginState(); return 0;
        case ID_BTN_SAVE: if (code == BN_CLICKED) app->SaveSettingsClicked(); return 0;
        case ID_BTN_OPENLOG: if (code == BN_CLICKED) app->OpenLogFolder(); return 0;
        case ID_PRESET_COMBO:
            if (code == CBN_SELCHANGE) app->OnPresetComboSel();
            return 0;
        case ID_LOGIN_URL:
            if (code == EN_CHANGE) app->ClearHint();
            return 0;
        case ID_CHK_AUTO: case ID_CHK_CLIP: case ID_CHK_HIST: case ID_CHK_FIRST:
            if (code == BN_CLICKED) {
                int idx = ToggleIndex(id);
                if (idx >= 0) {
                    app->SetToggleOn(idx, !app->ToggleOn(idx));
                    app->SaveUiSettings();
                    app->ClearHint();
                    InvalidateRect((HWND)lp, nullptr, FALSE);
                }
            }
            return 0;
        case ID_RAD_STR: case ID_RAD_JSON: case ID_RAD_BOTH:
            if (code == BN_CLICKED) {
                int idx = SegIndex(id);
                if (idx >= 0) {
                    app->SetSegSel(idx);
                    app->SaveUiSettings();
                    app->ClearHint();
                    InvalidateRect(GetDlgItem(hwnd, ID_RAD_STR), nullptr, FALSE);
                    InvalidateRect(GetDlgItem(hwnd, ID_RAD_JSON), nullptr, FALSE);
                    InvalidateRect(GetDlgItem(hwnd, ID_RAD_BOTH), nullptr, FALSE);
                }
            }
            return 0;
        }
        break;
    }
    case WM_APP_WEBVIEW_READY:
        app->OnWebViewReady();
        return 0;
    case WM_APP_TB_READY:
        app->OnHtmlToolbarReady();
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
        app->ShutdownWebView();   // 先关闭 WebView2 再销毁窗口
        app->FreeMemBuffer();
        app->ReleaseSingleInstance();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK HostWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        // WebView2 控制器异步创建期间，宿主子窗口里没有任何子窗口覆盖：
        // 不自绘就是一块未初始化的黑区（启动即闪黑/空白的直接原因）。
        // 这里按主题底色 + 提示文字绘制启动过渡帧，控制器就绪后由 WebView2
        // 子窗口盖住（宿主类带 WS_CLIPCHILDREN），并靠 InvalidateRect 清掉旧帧。
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        const Palette& pal = theme::Current();
        HBRUSH br = CreateSolidBrush(pal.surface);
        FillRect(dc, &rc, br);
        DeleteObject(br);
        App* app = g_app;
        if (app && !app->IsWebViewReady() && rc.right > rc.left && rc.bottom > rc.top) {
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, pal.text3);
            HGDIOBJ old = SelectObject(dc, app->FontBody());
            DrawTextW(dc, L"正在初始化浏览器引擎…", -1, &rc,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dc, old);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
