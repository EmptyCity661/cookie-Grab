// 外壳 HTML（全量 WebView2 化）：标题栏 + 导航 + 工具栏 + 内容占位(webSlot) +
// 设置页 + 说明页 + 状态栏，整个 UI chrome 由一个 WebView2 用 HTML/CSS 渲染。
// 配色一律取自 theme::StartPageCss() 的同源调色板（CSS 变量），不写裸色值。
// 交互经 window.chrome.webview.postMessage 回传 C++（App::OnShellMessage）；
// C++ 侧通过 App::PushShellState 调用本页的 window.__cgUpdate(obj) 做增量更新。
#pragma once
#include <string>

namespace shellhtml {

// 生成完整外壳 HTML 文档（UTF-8）。为静态骨架 + JS 运行时；
// 初始状态在页面 load 后 post({sh:'ready'})，由 C++ 立即 PushShellState 填充。
std::string Build();

} // namespace shellhtml
