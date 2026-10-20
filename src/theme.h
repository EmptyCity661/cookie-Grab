// 设计令牌：全局唯一的视觉语言来源。原生窗口绘制与 WebView2 起始页共用同一份调色板，
// 从根上消除「两套配色」（此前原生为翠绿、起始页为靛蓝）导致的风格割裂。
#pragma once
#include <windows.h>
#include <string>

#define CLR_NONE ((COLORREF)0xFFFFFFFF)

// 调色板（语义化命名，禁止在绘制代码里再出现裸 RGB 字面量）
struct Palette {
    bool dark = false;
    COLORREF canvas;      // 应用底色
    COLORREF surface;     // 卡片/控件底色
    COLORREF surfaceAlt;  // 次级底色（轨道、表头）
    COLORREF border;      // 描边
    COLORREF divider;     // 分隔线
    COLORREF text;        // 主文字
    COLORREF text2;       // 次文字
    COLORREF text3;       // 辅助/占位文字
    COLORREF accent;      // 主色（指示条、图形）
    COLORREF accentFill;  // 主色实心按钮底（与白字对比度达标）
    COLORREF accentHover;
    COLORREF accentActive;
    COLORREF accentTint;  // 选中底
    COLORREF accentText;  // 主色文字
    COLORREF onAccent;    // 主色底上的文字
    COLORREF success;
    COLORREF danger;
    COLORREF dangerTint;
    COLORREF hover;
    COLORREF pressed;
    COLORREF track;
    COLORREF knobOff;
    COLORREF focusRing;
    COLORREF fieldBg;     // 输入框底
    COLORREF accentCyan;  // 青色强调（玻璃光效辅助色）
    COLORREF accentViolet;// 紫色强调（玻璃光效辅助色）

    // ---- 玻璃材质令牌（仅供 WebView2/CSS 消费，原生自绘不使用 alpha）----
    // 这些是「同源」视觉契约的一部分：shell/起始页/工具栏都从这里取 CSS 变量，
    // 保证原生近似色与 Web 真玻璃色出自同一套设计令牌。
    std::string glassSurface;   // 面板半透明底（rgba）
    std::string glassStrong;    // 强玻璃底：标题栏/侧栏（rgba）
    std::string glassBorder;    // 玻璃外描边（rgba）
    std::string glassHi;        // 顶部内侧高光（rgba）
    std::string glow;           // 悬停/激活发光色（rgba）
    std::string shadow;         // 面板投影 box-shadow 值
    std::string ambientA;       // 环境光斑·蓝（rgb）
    std::string ambientB;       // 环境光斑·紫（rgb）
    std::string ambientC;       // 环境光斑·青（rgb）
    int blurPx = 24;            // backdrop-filter 模糊半径（CSS px）
    int saturate = 135;         // backdrop-filter 饱和度增强（%）
};

// 尺度令牌：一律以 96dpi 逻辑像素书写，绘制前经 App::S() 缩放
namespace scale {
    enum {
        RadiusCard = 14,
        RadiusControl = 10,
        RadiusChip = 8,
        RadiusPill = 999,
        RadiusWindow = 18,   // 玻璃窗口/大面板圆角

        GapXS = 4,
        GapS = 8,
        GapM = 12,
        GapL = 16,
        GapXL = 20,
        GapXXL = 24,

        PadCard = 18,
        PageMargin = 16,

        TitleBarH = 46,
        SideBarW = 236,
        SideBarCollapsedW = 76,
        TopNavH = 54,
        NavItemH = 40,

        FieldH = 38,
        BtnH = 38,
        RowH = 46,
        StatusH = 36,
        CardTitleH = 34,

        FontTitle = 14,
        FontNav = 13,
        FontBody = 13,
        FontCaption = 12,
        FontBtn = 13,
        FontCardTitle = 14,
        FontBadge = 12,
        FontIcon = 16,
        FontIconNav = 18
    };
}

namespace theme {

const Palette& Current();
void Resolve(bool darkMode);
bool DetectDarkMode();

// 由当前调色板生成起始页 CSS：与原生绘制同源
std::string StartPageCss();
std::string Hex(COLORREF c);
// 由 COLORREF + alpha(0~1) 生成 rgba(...) 字符串，供玻璃令牌导出
std::string Rgba(COLORREF c, double alpha);

} // namespace theme
