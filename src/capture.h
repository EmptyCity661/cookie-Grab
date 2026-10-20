// 抓取编排：Token 三通道汇总 + Cookie 拉取 + 登录态检测 + 自动抓取（防抖）
#pragma once
#include <string>
#include <vector>
#include <functional>
#include "webview.h"
#include "settings.h"
#include "logwriter.h"

class CaptureManager {
public:
    using StatusFn = std::function<void(const std::wstring& text)>;
    using ScheduleFn = std::function<void(int timerId, int ms)>;

    void Init(WebViewHost* wv, const Settings* settings, LogWriter* logger,
              StatusFn onStatus, ScheduleFn onSchedule);
    void ResetState();           // 清除登录状态后重置（清空 Token/快照/计数/首次标志）
    void OnWebMessage(const std::string& json);
    void OnNavigationCompleted(bool success);
    void PollTick();              // 定时轮询：Cookie 快照比对 + 首次打开检查
    void RequestCapture(bool manual, int mask = 3); // mask: 1=Cookie 2=Token 3=两者
    void SetCookieFilter(const std::wstring& regex); // Cookie 名正则筛选（空=不筛选，写盘时应用）
    void FirePending();           // 防抖到期
    void FireDelayedStep();       // 预扫描等待到期
    void KillTimers();

    int CookieCount() const { return lastCookieCount_; }
    int TokenCount() const { return (int)tokens_.size(); }
    const std::wstring& LastTime() const { return lastTime_; }
    const std::wstring& LastDomain() const { return lastDomain_; }

    static std::wstring HostFromUrl(const std::wstring& url);

private:
    void DoGetCookies();
    void OnCookies(std::vector<CookieItem> cookies);
    std::vector<CookieItem> ApplyFilter(const std::vector<CookieItem>& in) const; // 按 cookieFilter_ 筛选
    void FinishCapture();        // 按掩码写日志并更新状态（Cookie/Token 独立）
    bool IsLoggedIn(const std::vector<CookieItem>& cookies) const;
    void SetStatus(const std::wstring& t);

    WebViewHost* wv_ = nullptr;
    const Settings* settings_ = nullptr;
    LogWriter* logger_ = nullptr;
    StatusFn onStatus_;
    ScheduleFn onSchedule_;

    std::vector<TokenItem> tokens_;
    std::vector<CookieItem> lastCookies_; // 最近一次 Cookie 快照（Token 独立写入时复用）
    std::wstring lastSnapshot_;     // 上次成功写入的 Cookie 串
    std::wstring lastDomain_;
    std::wstring lastTime_;
    std::wstring currentHost_;
    std::wstring cookieFilter_;   // Cookie 名正则（空串=全量写入）
    int lastCookieCount_ = 0;
    int maskNext_ = 0;              // 待写入掩码：1=Cookie 2=Token

    bool firstNavDone_ = false;
    bool firstOpenPending_ = false;
    bool stageDebounce_ = false;    // 防抖中
    bool stagePrescan_ = false;     // 等待存储预扫描
    bool manualNext_ = false;
    bool pollInFlight_ = false;
    bool captureInFlight_ = false;
    bool tokensDirty_ = false;
};
