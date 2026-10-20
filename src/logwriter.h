// 日志写入：cookie.log / cookie.json / cookie_detail.json / cookie_history.log
// 规则：UTF-8 无 BOM；cookie.log 等覆盖文件用「临时文件+原子替换」；history 追加；
//       进程内互斥；日志目录优先 exe 目录，不可写回退 %LOCALAPPDATA%\CookieGrabber\logs
#pragma once
#include <string>
#include <vector>
#include <mutex>
#include "settings.h"

struct CookieItem {
    std::wstring name;
    std::wstring value;
    std::wstring domain;
    std::wstring path;
    double expires = 0;
    bool httpOnly = false;
    bool secure = false;
    bool isSession = false;
    int sameSite = 0; // COREWEBVIEW2_COOKIE_SAME_SITE_KIND: 0=None 1=Lax 2=Strict
};

struct TokenItem {
    std::wstring name;    // 请求头名 / 存储键 / JSON 字段路径
    std::wstring value;
    std::wstring source;  // request-header / localStorage / sessionStorage / response-body
    std::wstring url;     // 捕获所在 URL
    std::string capturedAt; // ISO8601 UTC
};

class LogWriter {
public:
    void Init(); // 解析日志目录
    const std::wstring& Dir() const { return dir_; }

    // 按设置写各日志文件；返回全部成功。
    // mask: 1=写 Cookie 类文件（cookie.log / cookie.json / cookie_detail.json）
    //       2=记录 Token（历史日志 Token 部分）；3=两者
    bool Write(const std::vector<CookieItem>& cookies,
               const std::vector<TokenItem>& tokens,
               const Settings& s,
               const std::wstring& domain,
               const std::wstring& url,
               std::wstring& errOut,
               int mask = 3);

    static std::string CookieString(const std::vector<CookieItem>& cookies);
    static std::string TokenString(const std::vector<TokenItem>& tokens);

private:
    std::wstring dir_;
    std::mutex mu_;
};
