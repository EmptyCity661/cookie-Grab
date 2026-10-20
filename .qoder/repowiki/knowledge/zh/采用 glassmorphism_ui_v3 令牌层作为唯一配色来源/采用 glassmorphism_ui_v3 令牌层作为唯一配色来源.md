---
kind: design
name: 采用 glassmorphism_ui_v3 令牌层作为唯一配色来源
source: session
category: adr
---

# 采用 glassmorphism_ui_v3 令牌层作为唯一配色来源

_来源：be8a04c → 93bcb9f 提交周期内记录的编码计划——内容为规划时意图，实现可能滞后或有出入。_

**状态：** accepted

## 背景
需要一套可在深色/浅色下工作、且能被 CSS 变量直接消费的调色板，以支撑 shell 的玻璃表面、环境光斑与强调色。

## 决策驱动
- 资源包一致性（glassmorphism_ui_v3）
- CSS 变量直出（--glass/--glass-strong/--glass-border/--glass-hi/--blur/--saturate/--shadow/--glow/--ambient-a/b/c/--accent-cyan/--accent-violet）
- 语义色保留（success/#51E5B0）

## 备选方案
- **Palette 扩展 rgba 字段 + StartPageCss 导出 CSS 变量** — 优点：theme.cpp 的 `MakeDark()`/`MakeLight()` 直接输出 `#4C8DFF`/`#6BE6EF`/`#A797FF`/`#51E5B0`/`#071326`/`#EEF6FF` 等；起始页与现有工具栏即刻呈现玻璃观感，无结构改动；缺点：需新增 `theme::Rgba()` 辅助函数
- **运行时从 tokens.json 解析 JSON 令牌** _（已否决）_ — 优点：更灵活；缺点：引入额外解析开销与依赖，违背零第三方依赖约束

## 决策
在 `theme.h` 的 `Palette` 增加 `glassSurface/glassStrong/glassBorder/glassHi/glow`、`ambientA/B/C`、`blurPx`、`saturate`，在 `scale` 增加 `RadiusWindow=18`/`Blur=24`/`Saturate=135`/`Shadow`，并在 `StartPageCss()` 导出对应 CSS 变量供 shell 消费。

## 影响
所有 chrome 外观由 `theme/tokens.json` 驱动；浅色模式对应 `body.light-glass` 值，深色模式使用 `canvas=#071326`、`text=#EEF6FF` 等固定资源包值。