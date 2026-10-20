// WebView2 玻璃外壳（shell）：用单个铺满客户区的 WebView2 渲染全部窗口 chrome——
// 标题栏 / 顶部导航 / 浏览器页工具栏 / 内容视口孔 / 设置页 / 说明页 / 状态栏，
// 全部消费 theme::StartPageCss() 的同源玻璃令牌，取代退役的原生 GDI 自绘层。
//
// 拓扑：shell WebView 铺满客户区绘制环境光斑与各视图；被导航的第三方站点仍由独立的
// Content WebView（App::webview_）承载，按 shell 上报的 #webhole 矩形定位并覆盖其上
// （二者是主窗口的兄弟子窗口，Content 在孔区 Z 序更高）。
//
// 双向桥：JS→host 用 window.chrome.webview.postMessage(JSON)，统一信封 {t:"...",...}；
// host→JS 用 ExecuteScript("window.CG.state({...})")。Cookie/Token 值绝不进入 shell。
#pragma once
#include <string>
#include <vector>
#include "settings.h"

namespace shell {

// shell 运行态快照：用于生成整页 HTML 初值，以及增量推送到已加载页面。
struct State {
    int  page = 0;                 // 0 浏览器 / 1 设置 / 2 说明
    bool dark = false;
    std::wstring status;           // 状态栏文字（抓取进度/提示）
    int  cookieCount = 0;
    int  tokenCount = 0;
    std::wstring address;          // 浏览器页地址框当前值
    std::wstring logDir;           // 日志目录
    std::wstring hint;             // 设置页提示文字
    bool startPage = true;         // 内容视口当前是否显示起始页（影响地址框）
    Settings settings;             // 设置页表单初值
};

// 生成整页玻璃外壳 HTML（UTF-8，含内联 CSS 变量、结构与脚本）。
std::string Build(const State& st);

// 生成增量推送脚本体：window.CG.state({...});（不含外层 ExecuteScript 包装）。
std::string StateJs(const State& st);

} // namespace shell
