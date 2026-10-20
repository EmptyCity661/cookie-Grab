#include "theme.h"
#include <cstdio>

static Palette g_pal;

static Palette MakeLight() {
    Palette p;
    p.dark = false;
    p.canvas      = RGB(0xF2, 0xF5, 0xF9);
    p.surface     = RGB(0xFF, 0xFF, 0xFF);
    p.surfaceAlt  = RGB(0xF7, 0xFA, 0xFC);
    p.border      = RGB(0xDD, 0xE5, 0xEE);
    p.divider     = RGB(0xEA, 0xEF, 0xF5);
    p.text        = RGB(0x10, 0x1A, 0x2B);
    p.text2       = RGB(0x4E, 0x5C, 0x73);
    p.text3       = RGB(0x86, 0x95, 0xA8);
    p.accent      = RGB(0x0D, 0x9A, 0x6C);
    // 实心按钮底：白字对比度 4.6:1（此前的 #10B981 配白字仅 2.2:1，是「看不清楚」的直接原因）
    p.accentFill  = RGB(0x04, 0x78, 0x57);
    p.accentHover = RGB(0x06, 0x5F, 0x46);
    p.accentActive= RGB(0x03, 0x4B, 0x38);
    p.accentTint  = RGB(0xD6, 0xF2, 0xE5);
    p.accentText  = RGB(0x04, 0x6B, 0x4E);
    p.onAccent    = RGB(0xFF, 0xFF, 0xFF);
    p.success     = RGB(0x0F, 0x8B, 0x5A);
    p.danger      = RGB(0xC9, 0x2E, 0x2E);
    p.dangerTint  = RGB(0xFD, 0xEC, 0xEC);
    p.hover       = RGB(0xEC, 0xF1, 0xF7);
    p.pressed     = RGB(0xDD, 0xE5, 0xEE);
    p.track       = RGB(0xEE, 0xF3, 0xF8);
    p.knobOff     = RGB(0xB6, 0xC2, 0xD2);
    p.focusRing   = RGB(0x0D, 0x9A, 0x6C);
    p.fieldBg     = RGB(0xFF, 0xFF, 0xFF);
    return p;
}

static Palette MakeDark() {
    Palette p;
    p.dark = true;
    p.canvas      = RGB(0x10, 0x14, 0x1A);
    p.surface     = RGB(0x1A, 0x20, 0x28);
    p.surfaceAlt  = RGB(0x22, 0x2A, 0x34);
    p.border      = RGB(0x33, 0x3D, 0x4A);
    p.divider     = RGB(0x28, 0x31, 0x3C);
    p.text        = RGB(0xE9, 0xEE, 0xF5);
    p.text2       = RGB(0xAB, 0xB7, 0xC6);
    p.text3       = RGB(0x7C, 0x89, 0x9A);
    p.accent      = RGB(0x2F, 0xC7, 0x8F);
    p.accentFill  = RGB(0x14, 0x7A, 0x55);
    p.accentHover = RGB(0x1A, 0x8F, 0x65);
    p.accentActive= RGB(0x0F, 0x63, 0x46);
    p.accentTint  = RGB(0x17, 0x3A, 0x30);
    p.accentText  = RGB(0x5B, 0xD9, 0xA8);
    p.onAccent    = RGB(0xFF, 0xFF, 0xFF);
    p.success     = RGB(0x3F, 0xC0, 0x85);
    p.danger      = RGB(0xF0, 0x7A, 0x7A);
    p.dangerTint  = RGB(0x3A, 0x22, 0x24);
    p.hover       = RGB(0x26, 0x2F, 0x3A);
    p.pressed     = RGB(0x2F, 0x3A, 0x47);
    p.track       = RGB(0x24, 0x2C, 0x36);
    p.knobOff     = RGB(0x59, 0x66, 0x76);
    p.focusRing   = RGB(0x2F, 0xC7, 0x8F);
    p.fieldBg     = RGB(0x14, 0x19, 0x20);
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
    css += "  --accent-tint:" + Hex(p.accentTint) + ";\n";
    css += "  --accent-text:" + Hex(p.accentText) + ";\n";
    css += "  --on-accent:" + Hex(p.onAccent) + ";\n";
    css += "  --danger:" + Hex(p.danger) + ";\n";
    css += "  --radius-card:" + std::to_string(scale::RadiusCard) + "px;\n";
    css += "  --radius-control:" + std::to_string(scale::RadiusControl) + "px;\n";
    css += "}\n";
    return css;
}
