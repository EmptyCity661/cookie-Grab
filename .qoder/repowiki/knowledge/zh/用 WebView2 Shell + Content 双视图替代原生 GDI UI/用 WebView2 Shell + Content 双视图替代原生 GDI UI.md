---
kind: design
name: 用 WebView2 Shell + Content 双视图替代原生 GDI UI
source: session
category: adr
---

# 用 WebView2 Shell + Content 双视图替代原生 GDI UI

_来源：be8a04c → 93bcb9f 提交周期内记录的编码计划——内容为规划时意图，实现可能滞后或有出入。_

**状态：** accepted

## 背景
现有 UI 由 `src/ui.cpp`/`src/ui.h` 的 `Painter`/`draw::`/`Fonts` 自绘标题栏、导航、工具栏、状态栏与设置页，维护成本高且无法实现玻璃效果；同时需保持单文件、零第三方依赖并兼容 Win10 22H2。

## 决策驱动
- 单文件 / 零第三方依赖
- Win10 22H2 兼容性（无 DWMWA_SYSTEMBACKDROP_TYPE）
- 可维护性（HTML/CSS 替代自绘）
- 内容区仍为真实网页不可玻璃化

## 备选方案
- **WebView2 Shell + Content 双视图** — 优点：复用已内嵌 WebView2；shell 渲染 chrome 与设置/说明/起始页，content 承载站点；通过 postMessage 双向桥通信；hit-test 转发保留原生拖动/缩放/Snap/阴影；缺点：需子类化 shell 宿主 HWND 处理 WM_NCLBUTTONDOWN/HTLEFT 等；DPI 换算与 ResizeObserver 时序需防抖
- **保留原生窗口机制 + HTML 仅画视觉** _（已否决）_ — 优点：实现简单，无需 hit-test 转发；缺点：无法获得原生拖拽/缩放体验；退化为顶部原生可拖条
- **Mica/Acrylic 透桌面玻璃** _（已否决）_ — 优点：系统级背景模糊；缺点：Win10 22H2 无 `DWMWA_SYSTEMBACKDROP_TYPE`，不满足硬约束

## 决策
在 `app.h/.cpp` 中将 `toolbarWv_`/`tbHost_` 升级为 shell 宿主，新增 `src/shell.h/.cpp` 产出整页 HTML（含 titlebar/main-nav/glass-panel/statusbar/ambient），Content WebView（现 `webview_`）按 shell 上报的 `layout: {x,y,w,h,dpr}` 定位；所有交互经 `{t,...}` 统一信封桥（win/page/nav/cap/cfg/layout）在 JS→host 与 host→JS 间传递。

## 影响
删除 `src/ui.cpp`/`src/ui.h` 及 `App` 中全部 GDI 绘制成员（`HFONT/HBRUSH/HPEN`、`BuildFonts`/`BuildBrushes`/`WM_PAINT` 双缓冲等），仅保留极少量原生底色避免 WebView2 就绪前闪黑；`capture`/`settings`/`logwriter`/WebView2 加载器/单实例/登录轮询/`TokenHookScript` 保持不变。安全红线：Cookie/Token 值与站点会话绝不进入 shell DOM、`state` 或页面日志。