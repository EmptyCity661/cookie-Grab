#include "logwriter.h"
#include "util.h"
#include "json.h"
#include <set>

using namespace mjson;
using namespace util;

std::string LogWriter::CookieString(const std::vector<CookieItem>& cookies) {
    std::string out;
    for (size_t i = 0; i < cookies.size(); i++) {
        if (i) out += "; ";
        out += WideToUtf8(cookies[i].name);
        out += '=';
        out += WideToUtf8(cookies[i].value);
    }
    return out;
}

std::string LogWriter::TokenString(const std::vector<TokenItem>& tokens) {
    std::string out;
    for (size_t i = 0; i < tokens.size(); i++) {
        if (i) out += "; ";
        out += WideToUtf8(tokens[i].name);
        out += '=';
        out += WideToUtf8(tokens[i].value);
    }
    return out;
}

void LogWriter::Init() {
    dir_ = util::ResolveLogDir();
}

bool LogWriter::Write(const std::vector<CookieItem>& cookies,
                      const std::vector<TokenItem>& tokens,
                      const Settings& s,
                      const std::wstring& domain,
                      const std::wstring& url,
                      std::wstring& errOut,
                      int mask) {
    std::lock_guard<std::mutex> lk(mu_);
    bool ok = true;
    std::string timeA = util::NowLogTimeA();
    std::string cookieStr = CookieString(cookies);
    std::string tokenStr = TokenString(tokens);
    std::wstring base = dir_ + L"\\";

    auto fail = [&](const wchar_t* what) {
        ok = false;
        if (!errOut.empty()) errOut += L"; ";
        errOut += what;
    };

    // ---- cookie.log（字符串格式，第1行时间、第2行 Cookie 串）——仅 Cookie 抓取路径 ----
    if ((mask & 1) && (s.logFormat == LogFormat::String || s.logFormat == LogFormat::Both)) {
        std::string data = timeA + "\r\n" + cookieStr + "\r\n";
        if (!util::WriteFileAtomic(base + L"cookie.log", data)) fail(L"cookie.log");
    }

    // ---- cookie.json（扁平简洁型）——仅 Cookie 抓取路径 ----
    if ((mask & 1) && (s.logFormat == LogFormat::Json || s.logFormat == LogFormat::Both)) {
        Writer w;
        w.pretty = true;
        w.BeginObject();
        w.Key("time"); w.Str(timeA);
        w.Key("domain"); w.Str(WideToUtf8(domain));
        w.Key("url"); w.Str(WideToUtf8(url));
        w.Key("cookie"); w.Str(cookieStr);
        w.Key("token");
        if (tokens.empty()) {
            w.Str("");
        } else if (tokens.size() == 1) {
            w.Str(WideToUtf8(tokens[0].value)); // 单个 token 直接为字符串
        } else {
            w.BeginObject();
            for (auto& t : tokens) { w.Key(WideToUtf8(t.name)); w.Str(WideToUtf8(t.value)); }
            w.EndObject();
        }
        w.EndObject();
        w.out += "\n";
        if (!util::WriteFileAtomic(base + L"cookie.json", w.out)) fail(L"cookie.json");
    }

    // ---- cookie_detail.json（结构化明细型）——仅 Cookie 抓取路径 ----
    if ((mask & 1) && (s.logFormat == LogFormat::Json || s.logFormat == LogFormat::Both)) {
        Writer w;
        w.pretty = true;
        w.BeginObject();
        w.Key("time"); w.Str(timeA);
        w.Key("domain"); w.Str(WideToUtf8(domain));
        w.Key("url"); w.Str(WideToUtf8(url));
        w.Key("cookies");
        w.BeginArray();
        for (auto& c : cookies) {
            w.BeginObject();
            w.Key("name"); w.Str(WideToUtf8(c.name));
            w.Key("value"); w.Str(WideToUtf8(c.value));
            w.Key("domain"); w.Str(WideToUtf8(c.domain));
            w.Key("path"); w.Str(WideToUtf8(c.path));
            w.Key("expires"); w.Str(util::CookieExpiresToIso(c.expires, c.isSession));
            w.Key("httpOnly"); w.Bool(c.httpOnly);
            w.Key("secure"); w.Bool(c.secure);
            w.Key("sameSite"); w.Str(c.sameSite == 0 ? "None" : (c.sameSite == 1 ? "Lax" : "Strict"));
            w.EndObject();
        }
        w.EndArray();
        w.Key("tokens");
        w.BeginArray();
        for (auto& t : tokens) {
            w.BeginObject();
            w.Key("name"); w.Str(WideToUtf8(t.name));
            w.Key("value"); w.Str(WideToUtf8(t.value));
            w.Key("source"); w.Str(WideToUtf8(t.source));
            w.Key("url"); w.Str(WideToUtf8(t.url));
            w.Key("capturedAt"); w.Str(t.capturedAt);
            w.EndObject();
        }
        w.EndArray();
        w.EndObject();
        w.out += "\n";
        if (!util::WriteFileAtomic(base + L"cookie_detail.json", w.out)) fail(L"cookie_detail.json");
    }

    // ---- cookie_history.log（追加；按抓取类型记录，Cookie/Token 各自独立）----
    if (s.history) {
        std::string data;
        data += "[";
        data += timeA;
        data += "] domain=";
        data += WideToUtf8(domain);
        data += " url=";
        data += WideToUtf8(url);
        if (mask & 1) { data += " cookies="; data += std::to_string(cookies.size()); }
        if (mask & 2) { data += " tokens=";  data += std::to_string(tokens.size()); }
        data += "\r\n";
        if ((mask & 1) && !cookieStr.empty()) { data += cookieStr; data += "\r\n"; }
        if ((mask & 2) && !tokenStr.empty()) { data += tokenStr; data += "\r\n"; }
        data += "\r\n";
        if (!util::AppendFile(base + L"cookie_history.log", data)) fail(L"cookie_history.log");
    }
    return ok;
}
