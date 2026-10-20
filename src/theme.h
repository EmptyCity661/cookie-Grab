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
};

// 尺度令牌：一律以 96dpi 逻辑像素书写，绘制前经 App::S() 缩放
namespace scale {
    enum {
        RadiusCard = 14,
        RadiusControl = 10,
        RadiusChip = 8,
        RadiusPill = 999,

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

} // namespace theme
