// 绘制层：后备缓冲 + 字体集 + 可复用绘制原语与组件。
// 所有像素都经由同一套原语产生，杜绝「同一个界面多种画法」造成的风格漂移。
#pragma once
#include <windows.h>
#include <string>
#include "theme.h"

// 后备缓冲：32 位 top-down DIB 段。
// 相比 CreateCompatibleBitmap，DIB 段的像素格式由本模块显式掌握（BI_BITFIELDS/32bpp），
// 位图尺寸与内容始终与本次绘制一致，避免高频 resize/滚动时出现旧帧残影。
class Painter {
public:
    bool Ensure(HDC ref, int w, int h);
    void Destroy();
    HDC  DC() const { return dc_; }
    bool Valid() const { return dc_ != nullptr; }
    void BlitTo(HDC dst, const RECT& client);

private:
    HDC     dc_ = nullptr;
    HBITMAP bmp_ = nullptr;
    void*   bits_ = nullptr;
    int     w_ = 0, h_ = 0;
};

// 字体集：按 DPI 一次性构建，DPI 变化时整体重建
struct Fonts {
    HFONT title = nullptr;
    HFONT nav = nullptr;
    HFONT navSel = nullptr;
    HFONT body = nullptr;
    HFONT bodyB = nullptr;
    HFONT caption = nullptr;
    HFONT btn = nullptr;
    HFONT cardTitle = nullptr;
    HFONT badge = nullptr;
    HFONT icon = nullptr;      // 16px 图标字体
    HFONT iconNav = nullptr;   // 导航图标

    void Build(int dpi);
    void Release();
    bool Ready() const { return body != nullptr; }
};

namespace draw {

// ---- 基础 ----
// 颜色混合（唯一入口，供悬停/投影等派生态使用）
COLORREF Mix(COLORREF a, COLORREF b, double t);
void Rect(HDC dc, RECT rc, COLORREF fill);
void RoundRect(HDC dc, RECT rc, int r, COLORREF fill, COLORREF border = CLR_NONE, int borderW = 1);
// 卡片：用数层递淡的描边模拟柔和投影（GDI 无高斯模糊，分层描边无锯齿副作用）
void Card(HDC dc, RECT rc, int r, const Palette& p, int dpi);
void HLine(HDC dc, const RECT& rc, int y, COLORREF c);
void VLine(HDC dc, const RECT& rc, int x, COLORREF c);
void Glyph(HDC dc, const wchar_t* glyph, RECT rc, HFONT f, COLORREF c);
void FocusRing(HDC dc, RECT rc, int r, COLORREF c, int dpi);

// 文字：outW 返回实际排版宽度（用于状态栏等需要精确测量的场景，杜绝重叠）
void Text(HDC dc, const std::wstring& s, RECT rc, HFONT f, COLORREF c, UINT flags, int* outW = nullptr);
int Measure(HDC dc, const std::wstring& s, HFONT f);
// 多行换行排版，返回占用高度
int WrapText(HDC dc, const std::wstring& s, RECT rc, HFONT f, COLORREF c);

// ---- 组件 ----
enum BtnVariant { BtnPrimary, BtnAccent, BtnSecondary, BtnGhost, BtnDanger };
struct BtnState {
    bool hot = false;
    bool press = false;
    bool focus = false;
    bool disabled = false;
};
// 返回文字实际所需宽度（用于自适应按钮宽度）
int ButtonW(HDC dc, const std::wstring& label, const wchar_t* glyph, const Fonts& f);
void Button(HDC dc, RECT rc, const std::wstring& label, const wchar_t* glyph,
            BtnVariant v, const BtnState& st, const Fonts& f, const Palette& p, int dpi);

void Toggle(HDC dc, RECT rc, const std::wstring& label, const std::wstring& hint,
            bool on, bool hot, const Fonts& f, const Palette& p, int dpi);

// 独立分段胶囊：每个选项自成一体、之间留缝。
// 此前用「共享容器 + 子控件平铺」，子控件方形底色会破坏容器圆角并残留旧选中态。
void Chip(HDC dc, RECT rc, const std::wstring& label, bool sel, bool hot, bool focus,
          const Fonts& f, const Palette& p, int dpi);

// 纤细滚动条：track 为整条轨道矩形
void Scrollbar(HDC dc, RECT track, int viewH, int contentH, int scroll,
               bool hot, bool drag, const Palette& p, int dpi);

void Dot(HDC dc, int cx, int cy, int r, COLORREF c);

} // namespace draw
