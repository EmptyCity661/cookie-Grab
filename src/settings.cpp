#include "settings.h"
#include "util.h"
#include "json.h"

using namespace mjson;

static const wchar_t* kFileName = L"settings.json";

std::wstring SettingsFilePath() {
    return util::LocalAppDir() + L"\\" + kFileName;
}

Settings DefaultSettings() {
    Settings s;
    // 主流网盘优先（登录态 Cookie/Token 抓取需求最集中），再保留两个常用站点
    s.presets.push_back({ L"百度网盘", L"https://pan.baidu.com" });
    s.presets.push_back({ L"阿里云盘", L"https://www.alipan.com" });
    s.presets.push_back({ L"夸克网盘", L"https://pan.quark.cn" });
    s.presets.push_back({ L"天翼云盘", L"https://cloud.189.cn" });
    s.presets.push_back({ L"迅雷网盘", L"https://pan.xunlei.com" });
    s.presets.push_back({ L"115网盘", L"https://115.com" });
    s.presets.push_back({ L"B站", L"https://www.bilibili.com" });
    s.presets.push_back({ L"GitHub", L"https://github.com/login" });
    return s;
}

Settings LoadSettings() {
    Settings s = DefaultSettings();
    std::string data;
    if (!util::ReadFileBytes(SettingsFilePath(), data)) return s;
    auto root = Parse(data);
    if (!root || root->type != Value::Object) return s;
    s.autoCapture = root->GetBool("autoCapture", true);
    s.clipboard = root->GetBool("clipboard", true);
    s.history = root->GetBool("history", true);
    s.firstOpenCapture = root->GetBool("firstOpenCapture", true);
    std::string fmt = root->GetStr("logFormat", "string");
    if (fmt == "json") s.logFormat = LogFormat::Json;
    else if (fmt == "both") s.logFormat = LogFormat::Both;
    else s.logFormat = LogFormat::String;
    auto it = root->obj.find("presets");
    if (it != root->obj.end() && it->second->type == Value::Array) {
        s.presets.clear();
        for (auto& pv : it->second->arr) {
            if (pv->type != Value::Object) continue;
            Preset p;
            p.name = util::Utf8ToWide(pv->GetStr("name"));
            p.url = util::Utf8ToWide(pv->GetStr("url"));
            if (!p.url.empty()) s.presets.push_back(p);
        }
    }
    return s;
}

bool SaveSettings(const Settings& s) {
    Writer w;
    w.pretty = true;
    w.BeginObject();
    w.Key("autoCapture"); w.Bool(s.autoCapture);
    w.Key("clipboard"); w.Bool(s.clipboard);
    w.Key("history"); w.Bool(s.history);
    w.Key("firstOpenCapture"); w.Bool(s.firstOpenCapture);
    w.Key("logFormat");
    const char* fmt = "string";
    if (s.logFormat == LogFormat::Json) fmt = "json";
    else if (s.logFormat == LogFormat::Both) fmt = "both";
    w.Str(fmt);
    w.Key("presets");
    w.BeginArray();
    for (auto& p : s.presets) {
        w.BeginObject();
        w.Key("name"); w.Str(util::WideToUtf8(p.name));
        w.Key("url"); w.Str(util::WideToUtf8(p.url));
        w.EndObject();
    }
    w.EndArray();
    w.EndObject();
    w.out += "\n";
    return util::WriteFileAtomic(SettingsFilePath(), w.out);
}
