#include "webtoolbar.h"
#include "theme.h"
#include "util.h"
#include "json.h"

// HTML 文本转义（用于 <option>/属性等标签内容）
static std::string HtmlEsc(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '&': r += "&amp;"; break;
        case '<': r += "&lt;"; break;
        case '>': r += "&gt;"; break;
        case '"': r += "&quot;"; break;
        case '\'': r += "&#39;"; break;
        default: r += c;
        }
    }
    return r;
}

namespace webtoolbar {

std::string Build(const std::vector<Preset>& presets, const std::wstring& address) {
    const Palette& p = theme::Current();

    // 预设下拉项：首项为「自定义（手动输入）」，与原生 presetCombo_ 语义一致
    std::string opts = "<option value=\"-1\">自定义（手动输入）</option>";
    std::string datalist, jsPresets = "[";
    for (size_t i = 0; i < presets.size(); i++) {
        std::string name = HtmlEsc(util::WideToUtf8(presets[i].name));
        std::string urlRaw = util::WideToUtf8(presets[i].url);
        std::string urlJs = mjson::Escape(urlRaw);
        opts += "<option value=\"" + std::to_string(i) + "\">" + name + "</option>";
        datalist += "<option value=\"" + HtmlEsc(urlRaw) + "\"></option>";
        jsPresets += "\"" + urlJs + "\"";
        if (i + 1 < presets.size()) jsPresets += ",";
    }
    jsPresets += "]";

    std::string addrInit = HtmlEsc(util::WideToUtf8(address));
    // 下拉箭头按调色板着色，避免固定灰（保持同源配色）
    // 下拉箭头按调色板着色（#→%23），以双引号 url() 直接写进 CSS（SVG 属性用单引号，
    // 不与外层 url("...") 冲突），避开将单引号内嵌进 JS 字符串导致的截断
    std::string arrowColor = theme::Hex(p.text3);   // #RRGGBB
    if (!arrowColor.empty() && arrowColor[0] == '#') arrowColor = "%23" + arrowColor.substr(1);
    std::string arrowCss = "background-image:url(\"data:image/svg+xml;utf8,"
        "<svg xmlns='http://www.w3.org/2000/svg' width='10' height='6' viewBox='0 0 10 6'>"
        "<path d='M1 1l4 4 4-4' fill='none' stroke='" + arrowColor + "' stroke-width='1.5' stroke-linecap='round'/></svg>\");";

    std::string html;
    html += "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>";
    html += theme::StartPageCss();     // :root 变量（同源调色板）
    html +=
        "*{box-sizing:border-box;margin:0;padding:0}"
        "html,body{height:100%}"
        "body{font-family:'Segoe UI','Microsoft YaHei',sans-serif;background:var(--canvas);color:var(--text);"
        "display:flex;padding:12px;overflow:hidden;-webkit-user-select:none;user-select:none}"
        ".card{flex:1;display:flex;flex-wrap:wrap;align-items:center;gap:10px;"
        "background:var(--surface);border:1px solid var(--border);border-radius:var(--radius-card);"
        "padding:12px 14px;box-shadow:0 6px 18px rgba(0,0,0,.10),0 1px 3px rgba(0,0,0,.06)}"
        ".field{height:38px;border-radius:var(--radius-control);border:1px solid var(--border);"
        "background:var(--field-bg);color:var(--text);padding:0 12px;font-size:13px;outline:none;"
        "transition:border-color .15s,box-shadow .15s}"
        ".field::placeholder{color:var(--text-3)}"
        ".field:focus{border-color:var(--accent);box-shadow:0 0 0 3px var(--accent-tint)}"
        "#address{flex:1 1 240px;min-width:160px}"
        "#preset{-webkit-appearance:none;appearance:none;flex:0 0 auto;min-width:150px;padding-right:30px;"
        "cursor:pointer;background-repeat:no-repeat;background-position:right 10px center;background-size:10px 6px}"
        "#regex{flex:0 1 200px;min-width:130px;font-family:Consolas,'Courier New',monospace}"
        ".btn{height:38px;border-radius:var(--radius-control);border:none;padding:0 16px;font-size:13px;"
        "font-weight:600;cursor:pointer;white-space:nowrap;box-shadow:0 1px 2px rgba(0,0,0,.15);"
        "transition:transform .06s,box-shadow .15s,filter .15s}"
        ".btn:hover{box-shadow:0 3px 10px rgba(0,0,0,.18);filter:brightness(1.04)}"
        ".btn:active{transform:translateY(1px);box-shadow:0 1px 2px rgba(0,0,0,.18)}"
        ".btn:focus-visible{outline:none;box-shadow:0 0 0 3px var(--accent-tint)}"
        ".primary{background:linear-gradient(180deg,var(--accent-fill),var(--accent-hover));color:var(--on-accent)}"
        ".cap{background:linear-gradient(180deg,var(--accent),var(--accent-fill));color:var(--on-accent)}"
        ".ghost{background:var(--surface);color:var(--text-2);border:1px solid var(--border);box-shadow:none}"
        ".ghost:hover{background:var(--hover);color:var(--text)}";
    // 下拉箭头作为独立规则追加（CSS 层叠，background-image 归到 #preset）
    html += "#preset{" + arrowCss + "}";
    html += "</style></head><body><div class=\"card\">";
    html += "<input id=\"address\" class=\"field\" list=\"presets\" placeholder=\"输入网址并回车，或点右侧打开\" value=\"" + addrInit + "\">";
    html += "<datalist id=\"presets\">" + datalist + "</datalist>";
    html += "<select id=\"preset\" class=\"field\">" + opts + "</select>";
    html += "<input id=\"regex\" class=\"field\" placeholder=\"筛选正则（按 Cookie 名）\" spellcheck=\"false\">";
    html += "<button id=\"open\" class=\"btn primary\">打开</button>";
    html += "<button id=\"refresh\" class=\"btn ghost\">刷新</button>";
    html += "<button id=\"capCookie\" class=\"btn cap\">抓取 Cookie</button>";
    html += "<button id=\"capToken\" class=\"btn cap\">抓取 Token</button>";
    html += "</div>";

    html += "<script>"
        "var PRESETS=" + jsPresets + ";"
        "var addr=document.getElementById('address');"
        "var regexEl=document.getElementById('regex');"
        "function curRe(){return (regexEl.value||'').trim();}"
        "function post(o){try{window.chrome.webview.postMessage(JSON.stringify(o));}catch(e){}}"
        "var reTimer=0;"
        "regexEl.addEventListener('input',function(){clearTimeout(reTimer);var v=this.value;"
        "reTimer=setTimeout(function(){post({tb:'regex',re:v});},300);});"
        "document.getElementById('preset').addEventListener('change',function(){var i=parseInt(this.value,10);"
        "if(i>=0&&PRESETS[i])addr.value=PRESETS[i];addr.focus();});"
        "function doOpen(){var u=(addr.value||'').trim();if(u)post({tb:'open',url:u});}"
        "document.getElementById('open').addEventListener('click',doOpen);"
        "addr.addEventListener('keydown',function(e){if(e.key==='Enter')doOpen();});"
        "document.getElementById('refresh').addEventListener('click',function(){post({tb:'refresh'});});"
        "document.getElementById('capCookie').addEventListener('click',function(){post({tb:'capcookie',re:curRe()});});"
        "document.getElementById('capToken').addEventListener('click',function(){post({tb:'captoken',re:curRe()});});"
        "window.__cgSetAddress=function(u){addr.value=u;};"
        "</script></body></html>";

    return html;
}

} // namespace webtoolbar
