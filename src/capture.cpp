#include "capture.h"
#include "util.h"
#include "json.h"
#include <algorithm>

using namespace mjson;

// 定时器 ID（由 App 分配）
enum { TIMER_POLL = 1, TIMER_DEBOUNCE = 2, TIMER_PRESCAN = 3 };
static const int kPollMs = 1500;
static const int kDebounceMs = 600;
static const int kPrescanMs = 250;

void CaptureManager::Init(WebViewHost* wv, const Settings* settings, LogWriter* logger,
                          StatusFn onStatus, ScheduleFn onSchedule) {
    wv_ = wv;
    settings_ = settings;
    logger_ = logger;
    onStatus_ = std::move(onStatus);
    onSchedule_ = std::move(onSchedule);
}

void CaptureManager::ResetState() {
    KillTimers();
    tokens_.clear();
    tokensDirty_ = false;
    lastSnapshot_.clear();
    lastDomain_.clear();
    lastTime_.clear();
    currentHost_.clear();
    lastCookieCount_ = 0;
    firstNavDone_ = false;
    firstOpenPending_ = false;
    stageDebounce_ = false;
    stagePrescan_ = false;
    manualNext_ = false;
    pollInFlight_ = false;
    captureInFlight_ = false;
}

std::wstring CaptureManager::HostFromUrl(const std::wstring& url) {
    size_t p = url.find(L"://");
    if (p == std::wstring::npos) return {};
    p += 3;
    size_t e = url.find_first_of(L"/?#", p);
    std::wstring host = (e == std::wstring::npos) ? url.substr(p) : url.substr(p, e - p);
    size_t at = host.rfind(L'@');
    if (at != std::wstring::npos) host = host.substr(at + 1);
    size_t colon = host.rfind(L':');
    if (colon != std::wstring::npos) host = host.substr(0, colon);
    return host;
}

void CaptureManager::SetStatus(const std::wstring& t) {
    if (onStatus_) onStatus_(t);
}

void CaptureManager::OnWebMessage(const std::string& json) {
    auto root = Parse(json);
    if (!root || root->type != Value::Object) return;
    if (root->GetStr("type") != "token") return;
    std::wstring name = util::Utf8ToWide(root->GetStr("name"));
    std::wstring value = util::Utf8ToWide(root->GetStr("value"));
    std::wstring source = util::Utf8ToWide(root->GetStr("source"));
    std::wstring url = util::Utf8ToWide(root->GetStr("url"));
    if (value.empty()) return;
    // 同源同名 token 保留最新值
    for (auto& t : tokens_) {
        if (t.name == name && t.source == source) {
            t.value = value;
            t.url = url;
            t.capturedAt = util::NowIsoUtc();
            tokensDirty_ = true;
            if (settings_->autoCapture) RequestCapture(false);
            return;
        }
    }
    TokenItem item;
    item.name = name;
    item.value = value;
    item.source = source;
    item.url = url;
    item.capturedAt = util::NowIsoUtc();
    tokens_.push_back(item);
    tokensDirty_ = true;
    if (settings_->autoCapture) RequestCapture(false);
}

void CaptureManager::OnNavigationCompleted(bool success) {
    if (!success) return;
    if (!firstNavDone_) {
        firstNavDone_ = true;
        if (settings_->firstOpenCapture) firstOpenPending_ = true;
    }
    // 域名切换：清空旧域名的 Token 累积
    std::wstring host = HostFromUrl(wv_->CurrentUrl());
    if (!host.empty() && host != currentHost_) {
        currentHost_ = host;
        tokens_.clear();
        lastSnapshot_.clear();
    }
}

void CaptureManager::PollTick() {
    if (!wv_->IsReady() || captureInFlight_) return;
    std::wstring url = wv_->CurrentUrl();
    if (url.empty()) return;
    // 仅对有效 URI（http/https/file）查询 Cookie；about:blank 等会使运行时崩溃
    if (!util::StartsWithI(url, L"http://") && !util::StartsWithI(url, L"https://") && !util::StartsWithI(url, L"file://")) return;

    if (firstOpenPending_) {
        firstOpenPending_ = false;
        captureInFlight_ = true;
        wv_->GetCookies(url, [this](std::vector<CookieItem> list) {
            captureInFlight_ = false;
            bool logged = IsLoggedIn(list) || !tokens_.empty();
            if (logged) {
                SetStatus(L"检测到已是登录状态，正在抓取…");
                RequestCapture(false);
            }
        });
        return;
    }
    if (!settings_->autoCapture) return;
    if (pollInFlight_) return;
    pollInFlight_ = true;
    wv_->GetCookies(url, [this](std::vector<CookieItem> list) {
        pollInFlight_ = false;
        std::string snap = LogWriter::CookieString(list);
        std::string last = util::WideToUtf8(lastSnapshot_);
        if (snap != last) RequestCapture(false);
    });
}

void CaptureManager::RequestCapture(bool manual, int mask) {
    if (mask <= 0) mask = 3;
    maskNext_ |= mask;   // 合并而非覆盖：防抖窗口内多次请求时保留先到的抓取范围
    manualNext_ = manual;
    if (!stageDebounce_) {
        stageDebounce_ = true;
        if (onSchedule_) onSchedule_(TIMER_DEBOUNCE, kDebounceMs);
    }
}

void CaptureManager::FirePending() {
    stageDebounce_ = false;
    if (!wv_->IsReady()) return;
    if (manualNext_ && (maskNext_ & 2)) {
        // 手动抓取（含 Token）：先主动扫一次存储，再按需拉 Cookie
        wv_->ExecuteScript(L"(window.__cgScanStorage && window.__cgScanStorage()) || 0");
        stagePrescan_ = true;
        if (onSchedule_) onSchedule_(TIMER_PRESCAN, kPrescanMs);
    } else {
        DoGetCookies();
    }
}

void CaptureManager::FireDelayedStep() {
    stagePrescan_ = false;
    if (maskNext_ & 1) DoGetCookies();
    else FinishCapture();   // 仅 Token：扫描完成后直接写 Token，不拉 Cookie
}

void CaptureManager::DoGetCookies() {
    if (!wv_->IsReady() || captureInFlight_) return;
    std::wstring url = wv_->CurrentUrl();
    if (url.empty()) { SetStatus(L"尚未打开页面，无法抓取"); return; }
    if (!util::StartsWithI(url, L"http://") && !util::StartsWithI(url, L"https://") && !util::StartsWithI(url, L"file://")) {
        SetStatus(L"当前页面不支持抓取（非 http/https/file 页面）");
        return;
    }
    captureInFlight_ = true;
    SetStatus(L"正在抓取…");
    wv_->GetCookies(url, [this](std::vector<CookieItem> list) { OnCookies(std::move(list)); });
}

void CaptureManager::OnCookies(std::vector<CookieItem> cookies) {
    captureInFlight_ = false;
    lastCookies_ = std::move(cookies);
    lastCookieCount_ = (int)lastCookies_.size();
    FinishCapture();
}

void CaptureManager::FinishCapture() {
    int mask = maskNext_;
    maskNext_ = 0;
    if (!mask) return;
    std::wstring url = wv_->CurrentUrl();
    std::wstring domain = HostFromUrl(url);
    // 变更检测：含 Cookie（mask&1）时比较 Cookie 快照；仅 Token 时 lastCookies_ 已过时，只看 tokensDirty_
    bool changed;
    std::string snap;
    if (mask & 1) {
        snap = LogWriter::CookieString(lastCookies_);
        std::string last = util::WideToUtf8(lastSnapshot_);
        changed = (snap != last) || tokensDirty_;
    } else {
        changed = tokensDirty_;
    }
    if (!changed && !manualNext_) {
        SetStatus(L"登录状态无变化");
        tokensDirty_ = false;
        return;
    }
    manualNext_ = false;

    std::wstring err;
    bool ok = logger_->Write(lastCookies_, tokens_, *settings_, domain, url, err, mask);
    if (mask & 1) {
        lastSnapshot_ = util::Utf8ToWide(snap);   // 仅含 Cookie 时更新 Cookie 快照
    }
    lastDomain_ = domain;
    lastTime_ = util::NowLogTime();
    tokensDirty_ = false;

    if (!ok) {
        SetStatus(L"抓取完成但写日志失败: " + err);
        return;
    }
    if (mask & 1) {
        if (settings_->clipboard) {
            if (util::SetClipboardText(util::Utf8ToWide(snap), nullptr))
                SetStatus(L"已抓取 Cookie 并复制到剪贴板");
            else
                SetStatus(L"已抓取 Cookie（剪贴板复制失败）");
        } else {
            SetStatus(L"已抓取 Cookie");
        }
    } else {
        SetStatus(L"已抓取 Token（" + std::to_wstring(tokens_.size()) + L" 个）");
    }
}

bool CaptureManager::IsLoggedIn(const std::vector<CookieItem>& cookies) const {
    static const wchar_t* marks[] = {
        L"session", L"sess", L"token", L"auth", L"jwt", L"ticket", L"login", L"sso", L"sid"
    };
    for (auto& c : cookies) {
        std::wstring n = util::ToLower(c.name);
        for (auto* m : marks) {
            if (n.find(m) != std::wstring::npos) return true;
        }
    }
    return false;
}

void CaptureManager::KillTimers() {
    stageDebounce_ = false;
    stagePrescan_ = false;
    pollInFlight_ = false;
    captureInFlight_ = false;
}
