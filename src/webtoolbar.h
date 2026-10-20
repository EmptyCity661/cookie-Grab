// HTML 工具栏（方案 A 原型）：用 WebView2 + HTML/CSS 重画浏览器页工具栏，
// 与原生 GDI 自绘版对照美化效果。配色一律取自 theme::StartPageCss()（同源调色板），
// 不写裸色值；动作经独立的消息通道回接到 App 既有的 OpenUrl/CaptureNow 等方法。
#pragma once
#include <string>
#include <vector>

namespace webtoolbar {

struct Preset {
    std::wstring name;
    std::wstring url;
};

// 生成完整 HTML 文档（UTF-8）。presets 用于预设下拉与地址框候选；
// address 为初始地址值（导航完成后由 C++ 侧通过 __cgSetAddress 更新）。
std::string Build(const std::vector<Preset>& presets, const std::wstring& address);

} // namespace webtoolbar
