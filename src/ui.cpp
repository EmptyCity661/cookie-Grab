#include "ui.h"
#include <algorithm>
#include <cmath>

// ---------------- Painter ----------------

bool Painter::Ensure(HDC ref, int w, int h) {
    if (dc_ && w_ == w && h_ == h) return true;
    Destroy();
    if (w <= 0 || h <= 0) return false;

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;          // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    dc_ = CreateCompatibleDC(ref);
    if (!dc_) return false;
    bmp_ = CreateDIBSection(dc_, &bi, DIB_RGB_COLORS, &bits_, nullptr, 0);
    if (!bmp_) {
        DeleteDC(dc_);
        dc_ = nullptr;
        return false;
    }
    SelectObject(dc_, bmp_);
    w_ = w;
    h_ = h;
    return true;
}

void Painter::Destroy() {
    if (bmp_) { DeleteObject(bmp_); bmp_ = nullptr; }
    if (dc_) { DeleteDC(dc_); dc_ = nullptr; }
    bits_ = nullptr;
    w_ = h_ = 0;
}

void Painter::BlitTo(HDC dst, const RECT& client) {
    if (!dc_) return;
    BitBlt(dst, 0, 0, client.right, client.bottom, dc_, 0, 0, SRCCOPY);
}

// ---------------- Fonts ----------------

static HFONT MkFont(int px, int weight, int dpi, const wchar_t* face) {
    LOGFONTW lf = {};
    lf.lfHeight = -MulDiv(px, dpi, 96);
    lf.lfWeight = weight;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_NATURAL_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcsncpy(lf.lfFaceName, face, LF_FACESIZE - 1);
    lf.lfFaceName[LF_FACESIZE - 1] = 0;
    return CreateFontIndirectW(&lf);
}

// Win11 优先使用 Segoe Fluent Icons，回退 Segoe MDL2 Assets
static const wchar_t* IconFace() {
    static const wchar_t* cached = nullptr;
    if (cached) return cached;
    HDC dc = GetDC(nullptr);
    LOGFONTW probe = {};
    probe.lfCharSet = DEFAULT_CHARSET;
    wcsncpy(probe.lfFaceName, L"Segoe Fluent Icons", LF_FACESIZE - 1);
    probe.lfFaceName[LF_FACESIZE - 1] = 0;
    HFONT f = CreateFontIndirectW(&probe);
    HGDIOBJ old = SelectObject(dc, f);
    wchar_t actual[64] = {};
    GetTextFaceW(dc, 64, actual);
    SelectObject(dc, old);
    DeleteObject(f);
    ReleaseDC(nullptr, dc);
    cached = (wcscmp(actual, L"Segoe Fluent Icons") == 0)
                 ? L"Segoe Fluent Icons" : L"Segoe MDL2 Assets";
    return cached;
}

void Fonts::Build(int dpi) {
    const wchar_t* ui = L"Segoe UI";
    title     = MkFont(scale::FontTitle, FW_SEMIBOLD, dpi, ui);
    nav       = MkFont(scale::FontNav, FW_MEDIUM, dpi, ui);
    navSel    = MkFont(scale::FontNav, FW_SEMIBOLD, dpi, ui);
    body      = MkFont(scale::FontBody, FW_NORMAL, dpi, ui);
    bodyB     = MkFont(scale::FontBody, FW_SEMIBOLD, dpi, ui);
    caption   = MkFont(scale::FontCaption, FW_NORMAL, dpi, ui);
    btn       = MkFont(scale::FontBtn, FW_SEMIBOLD, dpi, ui);
    cardTitle = MkFont(scale::FontCardTitle, FW_SEMIBOLD, dpi, ui);
    badge     = MkFont(scale::FontBadge, FW_SEMIBOLD, dpi, ui);
    icon      = MkFont(scale::FontIcon, FW_NORMAL, dpi, IconFace());
    iconNav   = MkFont(scale::FontIconNav, FW_NORMAL, dpi, IconFace());
}

void Fonts::Release() {
    HFONT all[] = { title, nav, navSel, body, bodyB, caption, btn, cardTitle, badge, icon, iconNav };
    for (HFONT f : all) if (f) DeleteObject(f);
    title = nav = navSel = body = bodyB = caption = btn = cardTitle = badge = icon = iconNav = nullptr;
}

// ---------------- 基础原语 ----------------

COLORREF draw::Mix(COLORREF a, COLORREF b, double t) {
    auto cl = [](int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); };
    return RGB(cl((int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t)),
               cl((int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t)),
               cl((int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t)));
}

namespace {
// 在 dc 上以 f 绘制文字（成对还原字体）
void DrawWith(HDC dc, HFONT f, const std::wstring& s, RECT rc, COLORREF c, UINT flags, int* outW) {
    if (s.empty()) { if (outW) *outW = 0; return; }
    HGDIOBJ prev = SelectObject(dc, f);
    SetBkMode(dc, TRANSPARENT);
    COLORREF prevColor = GetTextColor(dc);
    SetTextColor(dc, c);
    RECT m = rc;
    DrawTextW(dc, s.c_str(), -1, &m, (flags | DT_CALCRECT) & ~(DT_VCENTER | DT_TOP | DT_BOTTOM));
    int w = m.right - m.left;
    int h = m.bottom - m.top;
    if (outW) *outW = w;
    if (!(flags & DT_WORDBREAK)) {
        // 单行：按垂直居中排版，避免 GDI 默认基线导致的上下偏移
        int cy = (rc.top + rc.bottom) / 2;
        RECT d = { rc.left, cy - (h + 1) / 2, rc.right, cy - (h + 1) / 2 + h };
        if (flags & DT_RIGHT) { d.left = rc.right - w; d.right = rc.right; }
        else if (flags & DT_CENTER) { d.left = rc.left + (rc.right - rc.left - w) / 2; d.right = d.left + w; }
        DrawTextW(dc, s.c_str(), -1, &d, (flags & ~(DT_VCENTER | DT_TOP | DT_BOTTOM)) | DT_SINGLELINE);
    } else {
        RECT d = rc;
        DrawTextW(dc, s.c_str(), -1, &d, flags);
    }
    SetTextColor(dc, prevColor);
    SelectObject(dc, prev);
}
} // namespace

void draw::Rect(HDC dc, RECT rc, COLORREF fill) {
    if (fill == CLR_NONE) return;
    HBRUSH br = CreateSolidBrush(fill);
    FillRect(dc, &rc, br);
    DeleteObject(br);
}

void draw::RoundRect(HDC dc, RECT rc, int r, COLORREF fill, COLORREF border, int borderW) {
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;
    if (r < 1) r = 1;
    r = std::min(r, std::min(w, h) / 2);
    HBRUSH br = (fill == CLR_NONE) ? (HBRUSH)GetStockObject(NULL_BRUSH) : CreateSolidBrush(fill);
    HPEN pen = (border == CLR_NONE) ? (HPEN)GetStockObject(NULL_PEN)
                                    : CreatePen(PS_INSIDEFRAME, borderW, border);
    HGDIOBJ ob = SelectObject(dc, br);
    HGDIOBJ op = SelectObject(dc, pen);
    ::RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, r * 2, r * 2);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    if (fill != CLR_NONE) DeleteObject(br);
    if (border != CLR_NONE) DeleteObject(pen);
}

void draw::Card(HDC dc, RECT rc, int r, const Palette& p, int dpi) {
    // 三层递淡描边模拟柔和外投影：不依赖 Alpha，缩放/DPI 下无锯齿毛边
    for (int i = 3; i >= 1; i--) {
        int grow = MulDiv(i, dpi, 96);
        double t = 1.0 - i * 0.28;
        RECT g = { rc.left - grow, rc.top - grow, rc.right + grow, rc.bottom + grow };
        COLORREF c = Mix(p.canvas, p.border, 0.35 + 0.2 * t);
        RoundRect(dc, g, r + grow, c, CLR_NONE);
    }
    RoundRect(dc, rc, r, p.surface, p.border, 1);
}

void draw::HLine(HDC dc, const RECT& rc, int y, COLORREF c) {
    RECT r = { rc.left, y, rc.right, y + 1 };
    Rect(dc, r, c);
}

void draw::VLine(HDC dc, const RECT& rc, int x, COLORREF c) {
    RECT r = { x, rc.top, x + 1, rc.bottom };
    Rect(dc, r, c);
}

void draw::Glyph(HDC dc, const wchar_t* glyph, RECT rc, HFONT f, COLORREF c) {
    if (!glyph || !glyph[0]) return;
    DrawWith(dc, f, glyph, rc, c, DT_CENTER | DT_VCENTER | DT_SINGLELINE, nullptr);
}

void draw::FocusRing(HDC dc, RECT rc, int r, COLORREF c, int dpi) {
    int inset = MulDiv(2, dpi, 96);
    RECT g = { rc.left + inset, rc.top + inset, rc.right - inset, rc.bottom - inset };
    RoundRect(dc, g, r, CLR_NONE, c, MulDiv(2, dpi, 96));
}

void draw::Text(HDC dc, const std::wstring& s, RECT rc, HFONT f, COLORREF c, UINT flags, int* outW) {
    DrawWith(dc, f, s, rc, c, flags, outW);
}

int draw::Measure(HDC dc, const std::wstring& s, HFONT f) {
    if (s.empty()) return 0;
    HGDIOBJ prev = SelectObject(dc, f);
    SIZE s2 = {};
    GetTextExtentPoint32W(dc, s.c_str(), (int)s.size(), &s2);
    SelectObject(dc, prev);
    return s2.cx;
}

int draw::WrapText(HDC dc, const std::wstring& s, RECT rc, HFONT f, COLORREF c) {
    if (s.empty()) return 0;
    HGDIOBJ prev = SelectObject(dc, f);
    SetBkMode(dc, TRANSPARENT);
    COLORREF pc = GetTextColor(dc);
    SetTextColor(dc, c);
    RECT m = { rc.left, rc.top, rc.right, rc.top };
    DrawTextW(dc, s.c_str(), -1, &m, DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX | DT_EDITCONTROL);
    int h = m.bottom - m.top;
    DrawTextW(dc, s.c_str(), -1, const_cast<RECT*>(&rc), DT_WORDBREAK | DT_TOP | DT_NOPREFIX | DT_EDITCONTROL);
    SetTextColor(dc, pc);
    SelectObject(dc, prev);
    return h;
}

// ---------------- 组件 ----------------

int draw::ButtonW(HDC dc, const std::wstring& label, const wchar_t* glyph, const Fonts& f) {
    int w = Measure(dc, label, f.btn);
    if (glyph && glyph[0]) w += Measure(dc, glyph, f.icon) + 6;
    return w;
}

void draw::Button(HDC dc, RECT rc, const std::wstring& label, const wchar_t* glyph,
                  BtnVariant v, const BtnState& st, const Fonts& f, const Palette& p, int dpi) {
    int r = MulDiv(scale::RadiusControl, dpi, 96);
    COLORREF fill = CLR_NONE, border = CLR_NONE, text = p.text;
    switch (v) {
    case BtnPrimary:
        fill = st.disabled ? p.track : (st.press ? p.accentActive : (st.hot ? p.accentHover : p.accentFill));
        border = CLR_NONE;
        text = st.disabled ? p.text3 : p.onAccent;
        break;
    case BtnAccent:
        fill = st.disabled ? p.track : (st.press ? p.accentTint : (st.hot ? p.hover : p.surface));
        border = st.disabled ? p.border : p.accent;
        text = st.disabled ? p.text3 : p.accentText;
        break;
    case BtnSecondary:
        fill = st.disabled ? p.track : (st.press ? p.pressed : (st.hot ? p.hover : p.surface));
        border = st.hot || st.press ? p.accent : p.border;
        text = st.disabled ? p.text3 : p.text;
        break;
    case BtnGhost:
        fill = st.press ? p.pressed : (st.hot ? p.hover : CLR_NONE);
        border = CLR_NONE;
        text = st.disabled ? p.text3 : p.text2;
        break;
    case BtnDanger:
        fill = st.press ? p.danger : (st.hot ? p.dangerTint : p.surface);
        border = st.hot || st.press ? p.danger : p.border;
        text = st.hot || st.press ? (st.press ? p.onAccent : p.danger) : p.danger;
        break;
    }
    if (fill == CLR_NONE && border == CLR_NONE) return;
    RoundRect(dc, rc, r, fill, border, MulDiv(1, dpi, 96));
    if (st.focus) FocusRing(dc, rc, r, p.focusRing, dpi);

    int gw = (glyph && glyph[0]) ? Measure(dc, glyph, f.icon) : 0;
    int gap = gw ? MulDiv(8, dpi, 96) : 0;
    int lw = Measure(dc, label, f.btn);
    int total = gw + gap + lw;
    int cx = rc.left + (rc.right - rc.left - total) / 2;
    int cy = (rc.top + rc.bottom) / 2;
    if (gw) {
        RECT gr = { cx, cy - dpi / 8, cx + gw, cy + dpi / 8 };
        Glyph(dc, glyph, gr, f.icon, text);
        cx += gw + gap;
    }
    RECT tr = { cx, rc.top, rc.right - MulDiv(6, dpi, 96), rc.bottom };
    DrawWith(dc, f.btn, label, tr, text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, nullptr);
}

void draw::Toggle(HDC dc, RECT rc, const std::wstring& label, const std::wstring& hint,
                  bool on, bool hot, const Fonts& f, const Palette& p, int dpi) {
    if (hot) {
        // 悬停底恰好铺满控件矩形：控件本身已含内边距，外扩只会被窗口裁剪成直角残边
        RoundRect(dc, rc, MulDiv(scale::RadiusControl, dpi, 96), p.hover, CLR_NONE);
    }
    int cy = (rc.top + rc.bottom) / 2;
    int tw = MulDiv(44, dpi, 96), th = MulDiv(24, dpi, 96);
    RECT track = { rc.left, cy - th / 2, rc.left + tw, cy + th / 2 };
    RoundRect(dc, track, th / 2, on ? p.accentFill : p.knobOff, CLR_NONE);
    int pad = MulDiv(3, dpi, 96);
    int kd = th - pad * 2;
    int kx = on ? track.right - kd - pad : track.left + pad;
    RECT knob = { kx, cy - kd / 2, kx + kd, cy + kd / 2 };
    RoundRect(dc, knob, kd / 2, p.surface, CLR_NONE);

    int tx = track.right + MulDiv(12, dpi, 96);
    RECT tr = { tx, rc.top, rc.right, rc.bottom };
    if (hint.empty()) {
        DrawWith(dc, f.body, label, tr, p.text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, nullptr);
    } else {
        int half = (rc.top + rc.bottom) / 2;
        RECT a = { tx, rc.top, rc.right, half };
        RECT b = { tx, half, rc.right, rc.bottom };
        DrawWith(dc, f.bodyB, label, a, p.text, DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS, nullptr);
        DrawWith(dc, f.caption, hint, b, p.text2, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS, nullptr);
    }
}

void draw::Chip(HDC dc, RECT rc, const std::wstring& label, bool sel, bool hot, bool focus,
                const Fonts& f, const Palette& p, int dpi) {
    int r = MulDiv(scale::RadiusControl, dpi, 96);
    COLORREF fill = sel ? p.accentTint : (hot ? p.hover : p.surface);
    COLORREF border = sel ? p.accent : (hot ? p.accent : p.border);
    COLORREF text = sel ? p.accentText : p.text2;
    RoundRect(dc, rc, r, fill, border, MulDiv(1, dpi, 96));
    if (focus) FocusRing(dc, rc, r, p.focusRing, dpi);
    DrawWith(dc, sel ? f.btn : f.body, label, rc, text,
             DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, nullptr);
}

void draw::Scrollbar(HDC dc, RECT track, int viewH, int contentH, int scroll,
                     bool hot, bool drag, const Palette& p, int dpi) {
    if (contentH <= viewH) return;
    int w = track.right - track.left;
    RoundRect(dc, track, w / 2, p.track, CLR_NONE);
    int thumbH = (int)((long long)viewH * viewH / contentH);
    thumbH = std::max(thumbH, MulDiv(34, dpi, 96));
    int range = viewH - thumbH;
    int maxScroll = contentH - viewH;
    if (maxScroll <= 0) maxScroll = 1;
    int ty = track.top + (int)((long long)scroll * range / maxScroll);
    RECT thumb = { track.left, ty, track.right, ty + thumbH };
    RoundRect(dc, thumb, w / 2, (hot || drag) ? p.accent : p.knobOff, CLR_NONE);
}

void draw::Dot(HDC dc, int cx, int cy, int r, COLORREF c) {
    HBRUSH br = CreateSolidBrush(c);
    HPEN pen = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ ob = SelectObject(dc, br);
    HGDIOBJ op = SelectObject(dc, pen);
    Ellipse(dc, cx - r, cy - r, cx + r, cy + r);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(br);
    DeleteObject(pen);
}
