// 设置模型与持久化（%LOCALAPPDATA%\CookieGrabber\settings.json）
#pragma once
#include <string>
#include <vector>

enum class LogFormat { String, Json, Both };

struct Preset {
    std::wstring name;
    std::wstring url;
};

struct Settings {
    bool autoCapture = true;       // 检测到登录状态变化自动抓取
    bool clipboard = true;         // 抓取后复制到剪贴板
    bool history = true;           // 追加 cookie_history.log
    bool firstOpenCapture = true;  // 首次打开即抓取
    LogFormat logFormat = LogFormat::String;
    std::vector<Preset> presets;
};

Settings LoadSettings();
bool SaveSettings(const Settings& s);
std::wstring SettingsFilePath();
Settings DefaultSettings();
