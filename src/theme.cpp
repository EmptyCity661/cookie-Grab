#include "theme.h"
#include <cstdio>

static Palette g_pal;

static Palette MakeLight() {
    Palette p;
    p.dark = false;
    p.canvas      = RGB(0xE8, 0xF0, 0xFB);
    p.surface     = RGB(0xF2, 0xF7, 0xFE);
    p.surfaceAlt  = RGB(0xDC, 0xE9, 0xF8);
    p.border      = RGB(0xC9, 0xD8, 0xEE);
    p.divider     = RGB(0xDC, 0xE7, 0xF4);
    p.text        = RGB(0x1D, 0x31, 0x50);
    p.text2       = RGB(0x5D, 0x75, 0x97);
    p.text3       = RGB(0x71, 0x88, 0xA6);
    p.accent      = RGB(0x2E, 0x6F, 0xE6);
    p.accentCyan  = RGB(0x12, 0xA5, 0xB8);
    p.accentViolet= RGB(0x6E, 0x5C, 0xD6);
    // 主色实心按钮底：白字对比度需达标（浅色下用较深蓝）
    p.accentFill  = RGB(0x2E, 0x6F, 0xE6);
    p.accentHover = RGB(0x4C, 0x8D, 0xFF);
    p.accentActive= RGB(0x1F, 0x55, 0xC0);
    p.accentTint  = RGB(0xDC, 0xEB, 0xFF);
    p.accentText  = RGB(0x1F, 0x55, 0xC0);
    p.onAccent    = RGB(0xFF, 0xFF, 0xFF);
    p.success     = RGB(0x12, 0xA8, 0x77);
    p.danger      = RGB(0xD6, 0x45, 0x5F);
    p.dangerTint  = RGB(0xFB, 0xE7, 0xEC);
    p.hover       = RGB(0xE2, 0xEC, 0xF8);
    p.pressed     = RGB(0xD2, 0xE0, 0xF0);
    p.track       = RGB(0xDC, 0xE7, 0xF4);
    p.knobOff     = RGB(0x9F, 0xB2, 0xCC);
    p.focusRing   = RGB(0x2E, 0x6F, 0xE6);
    p.fieldBg     = RGB(0xFF, 0xFF, 0xFF);
    // 浅色玻璃材质令牌
    p.glassSurface = "rgba(255,255,255,0.55)";
    p.glassStrong  = "rgba(241,248,255,0.62)";
    p.glassBorder  = "rgba(255,255,255,0.72)";
    p.glassHi      = "rgba(255,255,255,0.90)";
    p.glow         = "rgba(76,141,255,0.50)";
    p.shadow       = "0 16px 40px rgba(54,99,148,0.16)";
    p.ambientA     = "#57ADFF";
    p.ambientB     = "#BA99FF";
    p.ambientC     = "#63E2DA";
    p.blurPx       = 24;
    p.saturate     = 135;
    return p;
}

static Palette MakeDark() {
    Palette p;
    p.dark = true;
    p.canvas      = RGB(0x07, 0x13, 0x26);
    p.surface     = RGB(0x14, 0x29, 0x46);
    p.surfaceAlt  = RGB(0x0B, 0x19, 0x30);
    p.border      = RGB(0x29, 0x40, 0x5F);
    p.divider     = RGB(0x1E, 0x33, 0x50);
    p.text        = RGB(0xEE, 0xF6, 0xFF);
    p.text2       = RGB(0x98, 0xAF, 0xCE);
    p.text3       = RGB(0x6F, 0x89, 0xAA);
    p.accent      = RGB(0x4C, 0x8D, 0xFF);
    p.accentCyan  = RGB(0x6B, 0xE6, 0xEF);
    p.accentViolet= RGB(0xA7, 0x97, 0xFF);
    // 主色实心按钮底：白字对比度达标
    p.accentFill  = RGB(0x2E, 0x6F, 0xE6);
    p.accentHover = RGB(0x4C, 0x8D, 0xFF);
    p.accentActive= RGB(0x24, 0x56, 0xB8);
    p.accentTint  = RGB(0x1B, 0x3A, 0x63);
    p.accentText  = RGB(0x8F, 0xC5, 0xFF);
    p.onAccent    = RGB(0xFF, 0xFF, 0xFF);
    p.success     = RGB(0x51, 0xE5, 0xB0);
    p.danger      = RGB(0xE8, 0x5C, 0x7A);
    p.dangerTint  = RGB(0x3A, 0x22, 0x30);
    p.hover       = RGB(0x1C, 0x2E, 0x48);
    p.pressed     = RGB(0x24, 0x38, 0x4F);
    p.track       = RGB(0x0E, 0x21, 0x38);
    p.knobOff     = RGB(0x5B, 0x78, 0xA0);
    p.focusRing   = RGB(0x79, 0xDE, 0xFF);
    p.fieldBg     = RGB(0x0A, 0x1A, 0x30);
    // 深色玻璃材质令牌（Glassmorphism Night）
    p.glassSurface = "rgba(255,255,255,0.075)";
    p.glassStrong  = "rgba(20,41,70,0.68)";
    p.glassBorder  = "rgba(205,229,255,0.15)";
    p.glassHi      = "rgba(255,255,255,0.105)";
    p.glow         = "rgba(80,163,255,0.85)";
    p.shadow       = "0 22px 55px rgba(0,0,0,0.22)";
    p.ambientA     = "#246BE8";
    p.ambientB     = "#7C4FE1";
    p.ambientC     = "#167B9E";
    p.blurPx       = 24;
    p.saturate     = 135;
    return p;
}

void theme::Resolve(bool darkMode) {
    g_pal = darkMode ? MakeDark() : MakeLight();
}

const Palette& theme::Current() {
    return g_pal;
}

bool theme::DetectDarkMode() {
    HKEY k = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0, KEY_READ, &k) != ERROR_SUCCESS)
        return false;
    DWORD v = 1, sz = sizeof(v), type = 0;
    bool dark = false;
    if (RegQueryValueExW(k, L"AppsUseLightTheme", nullptr, &type, (LPBYTE)&v, &sz) == ERROR_SUCCESS &&
        type == REG_DWORD)
        dark = (v == 0);
    RegCloseKey(k);
    return dark;
}

std::string theme::Hex(COLORREF c) {
    char b[8];
    snprintf(b, sizeof(b), "#%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c));
    return b;
}

std::string theme::Rgba(COLORREF c, double alpha) {
    char b[40];
    if (alpha < 0) alpha = 0;
    if (alpha > 1) alpha = 1;
    snprintf(b, sizeof(b), "rgba(%d,%d,%d,%.3f)", GetRValue(c), GetGValue(c), GetBValue(c), alpha);
    return b;
}

std::string theme::StartPageCss() {
    const Palette& p = g_pal;
    std::string css;
    css += ":root{\n";
    css += "  --canvas:" + Hex(p.canvas) + ";\n";
    css += "  --surface:" + Hex(p.surface) + ";\n";
    css += "  --surface-alt:" + Hex(p.surfaceAlt) + ";\n";
    css += "  --border:" + Hex(p.border) + ";\n";
    css += "  --divider:" + Hex(p.divider) + ";\n";
    css += "  --text:" + Hex(p.text) + ";\n";
    css += "  --text-2:" + Hex(p.text2) + ";\n";
    css += "  --text-3:" + Hex(p.text3) + ";\n";
    css += "  --accent:" + Hex(p.accent) + ";\n";
    css += "  --accent-fill:" + Hex(p.accentFill) + ";\n";
    css += "  --accent-hover:" + Hex(p.accentHover) + ";\n";
    css += "  --accent-active:" + Hex(p.accentActive) + ";\n";
    css += "  --accent-tint:" + Hex(p.accentTint) + ";\n";
    css += "  --accent-text:" + Hex(p.accentText) + ";\n";
    css += "  --on-accent:" + Hex(p.onAccent) + ";\n";
    css += "  --danger:" + Hex(p.danger) + ";\n";
    css += "  --danger-tint:" + Hex(p.dangerTint) + ";\n";
    css += "  --success:" + Hex(p.success) + ";\n";
    css += "  --knob-off:" + Hex(p.knobOff) + ";\n";
    css += "  --hover:" + Hex(p.hover) + ";\n";
    css += "  --pressed:" + Hex(p.pressed) + ";\n";
    css += "  --track:" + Hex(p.track) + ";\n";
    css += "  --field-bg:" + Hex(p.fieldBg) + ";\n";
    css += "  --focus-ring:" + Hex(p.focusRing) + ";\n";
    // 玻璃材质令牌（shell / 起始页 / 工具栏共用）
    css += "  --accent-cyan:" + Hex(p.accentCyan) + ";\n";
    css += "  --accent-violet:" + Hex(p.accentViolet) + ";\n";
    css += "  --glass:" + p.glassSurface + ";\n";
    css += "  --glass-strong:" + p.glassStrong + ";\n";
    css += "  --glass-border:" + p.glassBorder + ";\n";
    css += "  --glass-hi:" + p.glassHi + ";\n";
    css += "  --glow:" + p.glow + ";\n";
    css += "  --shadow:" + p.shadow + ";\n";
    css += "  --ambient-a:" + p.ambientA + ";\n";
    css += "  --ambient-b:" + p.ambientB + ";\n";
    css += "  --ambient-c:" + p.ambientC + ";\n";
    css += "  --blur:" + std::to_string(p.blurPx) + "px;\n";
    css += "  --saturate:" + std::to_string(p.saturate) + "%;\n";
    css += "  --radius-window:" + std::to_string(scale::RadiusWindow) + "px;\n";
    css += "  --radius-card:" + std::to_string(scale::RadiusCard) + "px;\n";
    css += "  --radius-control:" + std::to_string(scale::RadiusControl) + "px;\n";
    css += "}\n";
    return css;
}
