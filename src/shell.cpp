#include "shell.h"
#include "theme.h"
#include "util.h"
#include "json.h"
#include <string>
#include <vector>

// ---- HTML 文本转义（标签内容 / 属性）----
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

namespace shell {

// State 的动态字段序列化为 JSON 对象文本（供 CG.state 初值与增量推送共用）。
static std::string StateObj(const State& st) {
    mjson::Writer w;
    w.BeginObject();
    w.Key("page");        w.Int(st.page);
    w.Key("status");      w.Str(util::WideToUtf8(st.status));
    w.Key("cookieCount"); w.Int(st.cookieCount);
    w.Key("tokenCount");  w.Int(st.tokenCount);
    w.Key("address");     w.Str(util::WideToUtf8(st.address));
    w.Key("hint");        w.Str(util::WideToUtf8(st.hint));
    w.Key("logDir");      w.Str(util::WideToUtf8(st.logDir));
    w.Key("dark");        w.Bool(st.dark);
    w.EndObject();
    return w.out;
}

std::string StateJs(const State& st) {
    return "window.CG&&window.CG.state(" + StateObj(st) + ");";
}

// 预设数组序列化为 JS 字面量 [{name,url},...]
static std::string PresetsJsonArray(const std::vector<Preset>& presets) {
    std::string s = "[";
    for (size_t i = 0; i < presets.size(); i++) {
        s += "{\"name\":\"" + mjson::Escape(util::WideToUtf8(presets[i].name)) +
             "\",\"url\":\"" + mjson::Escape(util::WideToUtf8(presets[i].url)) + "\"}";
        if (i + 1 < presets.size()) s += ",";
    }
    s += "]";
    return s;
}

std::string Build(const State& st) {
    const Palette& p = theme::Current();
    const Settings& set = st.settings;

    // 下拉箭头按调色板着色（避免固定灰），以 url() 写进 CSS，SVG 属性用单引号
    std::string arrow = theme::Hex(p.text3);
    if (!arrow.empty() && arrow[0] == '#') arrow = "%23" + arrow.substr(1);
    std::string arrowCss =
        "background-image:url(\"data:image/svg+xml;utf8,"
        "<svg xmlns='http://www.w3.org/2000/svg' width='10' height='6' viewBox='0 0 10 6'>"
        "<path d='M1 1l4 4 4-4' fill='none' stroke='" + arrow + "' stroke-width='1.5' stroke-linecap='round'/></svg>\");";

    std::string h;
    h += "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>";
    h += theme::StartPageCss();          // :root 同源玻璃令牌
    h +=
        // ---- 基础 ----
        "*{box-sizing:border-box;margin:0;padding:0}"
        "html,body{height:100%;overflow:hidden}"
        "body{font-family:'Segoe UI','Microsoft YaHei',sans-serif;color:var(--text);"
        "background:var(--canvas);-webkit-user-select:none;user-select:none;font-size:13px}"
        // 环境光斑（玻璃的真实边界只在本文档内，Win10 无法透桌面）
        ".amb{position:fixed;border-radius:50%;filter:blur(70px);opacity:.55;"
        "mix-blend-mode:screen;pointer-events:none;z-index:0}"
        ".amb.a{width:46vw;height:46vw;left:-6vw;top:-10vw;background:var(--ambient-a)}"
        ".amb.b{width:40vw;height:40vw;right:-8vw;top:6vh;background:var(--ambient-b);opacity:.45}"
        ".amb.c{width:44vw;height:44vw;left:28vw;bottom:-16vw;background:var(--ambient-c);opacity:.4}"
        "body.light .amb{mix-blend-mode:multiply;opacity:.30}"
        // 外框（铺满，纵向 flex）
        ".frame{position:relative;z-index:1;height:100%;display:flex;flex-direction:"
        "column;padding:0 8px}"
        // 玻璃面板通用配方
        ".glass{background:var(--glass);border:1px solid var(--glass-border);border-radius:var(--radius-card);"
        "backdrop-filter:blur(var(--blur)) saturate(var(--saturate));"
        "-webkit-backdrop-filter:blur(var(--blur)) saturate(var(--saturate));"
        "box-shadow:inset 0 1px 0 var(--glass-hi),var(--shadow)}"
        // ---- 标题栏 ----
        ".titlebar{height:46px;display:flex;align-items:center;gap:10px;padding:0 4px 0 12px;"
        "flex:0 0 auto}"
        ".brand{display:flex;align-items:center;gap:8px;pointer-events:none}"
        ".logo{width:22px;height:22px;border-radius:7px;display:flex;align-items:center;justify-content:center;"
        "background:linear-gradient(135deg,var(--accent-cyan),var(--accent) 60%,var(--accent-violet));"
        "color:var(--on-accent);font-size:13px;font-weight:700;box-shadow:0 0 12px var(--glow)}"
        ".bt{font-weight:600;font-size:13px;color:var(--text)}"
        ".ver{font-size:11px;color:var(--text-3)}"
        ".dragfill{flex:1;height:100%;cursor:default}"
        ".winbtns{display:flex;gap:4px}"
        ".wb{width:34px;height:26px;border-radius:8px;border:1px solid transparent;background:transparent;"
        "color:var(--text-2);cursor:pointer;display:flex;align-items:center;justify-content:center;"
        "font-family:'Segoe Fluent Icons','Segoe MDL2 Assets',sans-serif;font-size:10px;line-height:1;"
        "transition:background .12s,color .12s}"
        ".wb:hover{background:var(--hover);color:var(--text)}"
        ".wb.close:hover{background:var(--danger);color:var(--on-accent)}"
        // ---- 顶部导航 ----
        ".nav{display:flex;gap:6px;padding:2px 4px 8px;flex:0 0 auto}"
        ".tab{position:relative;height:30px;padding:0 16px;border:none;background:transparent;color:var(--text-2);"
        "font-size:13px;font-weight:600;cursor:pointer;border-radius:9px;transition:color .15s,background .15s}"
        ".tab:hover{color:var(--text);background:var(--glass)}"
        ".tab.on{color:var(--accent-text)}"
        ".tab.on::after{content:'';position:absolute;left:16px;right:16px;bottom:-6px;height:3px;border-radius:2px;"
        "background:var(--accent);box-shadow:0 0 10px var(--glow)}"
        // ---- 视图容器 ----
        ".views{flex:1 1 auto;position:relative;min-height:0}"
        ".view{position:absolute;inset:0;display:none;flex-direction:column;padding-bottom:8px}"
        ".view.on{display:flex}"
        // ---- 工具栏 ----
        ".toolbar{flex:0 0 auto;display:flex;flex-wrap:wrap;align-items:center;gap:10px;padding:12px 14px;margin-bottom:10px}"
        ".field{height:38px;border-radius:var(--radius-control);border:1px solid var(--glass-border);"
        "background:var(--field-bg);color:var(--text);padding:0 12px;font-size:13px;outline:none;"
        "transition:border-color .15s,box-shadow .15s}"
        ".field::placeholder{color:var(--text-3)}"
        ".field:focus{border-color:var(--accent);box-shadow:0 0 0 3px var(--accent-tint)}"
        "#address{flex:1 1 220px;min-width:150px}"
        "#preset{-webkit-appearance:none;appearance:none;flex:0 0 auto;min-width:150px;padding-right:30px;cursor:pointer;"
        "background-repeat:no-repeat;background-position:right 10px center;background-size:10px 6px}"
        "#regex{flex:0 1 190px;min-width:120px;font-family:Consolas,'Courier New',monospace}"
        ".btn{height:38px;border-radius:var(--radius-control);border:none;padding:0 16px;font-size:13px;"
        "font-weight:600;cursor:pointer;white-space:nowrap;transition:transform .06s,box-shadow .15s,filter .15s}"
        ".btn:hover{filter:brightness(1.06);box-shadow:0 0 14px var(--glow)}"
        ".btn:active{transform:translateY(1px)}"
        ".btn.primary{background:linear-gradient(180deg,var(--accent-hover),var(--accent-fill));color:var(--on-accent)}"
        ".btn.cap{background:linear-gradient(180deg,var(--accent),var(--accent-fill));color:var(--on-accent)}"
        ".btn.ghost{background:var(--glass);color:var(--text-2);border:1px solid var(--glass-border)}"
        ".btn.ghost:hover{color:var(--text);background:var(--hover);box-shadow:none}"
        ".btn.danger{background:var(--glass);color:var(--danger);border:1px solid var(--glass-border)}"
        ".btn.danger:hover{background:var(--danger-tint);box-shadow:none}"
        // ---- 内容视口孔（Content WebView 覆盖此矩形）----
        ".webhole{flex:1 1 auto;min-height:0;border-radius:var(--radius-card);overflow:hidden;"
        "background:var(--surface-alt);border:1px solid var(--glass-border);position:relative}"
        // ---- 设置页 ----
        ".sheet{flex:1 1 auto;min-height:0;overflow-y:auto;padding:20px 22px;margin:0}"
        ".sheet::-webkit-scrollbar{width:8px}"
        ".sheet::-webkit-scrollbar-thumb{background:var(--knob-off);border-radius:8px}"
        ".crow{display:flex;align-items:center;gap:14px;padding:9px 0}"
        ".crow>label{flex:0 0 88px;color:var(--text-2);font-size:13px}"
        ".crow>*:last-child{flex:1 1 auto;min-width:0}"
        ".crow .field{width:100%}"
        "select.field{-webkit-appearance:none;appearance:none;padding-right:30px;cursor:pointer;"
        "background-repeat:no-repeat;background-position:right 10px center;background-size:10px 6px}"
        ".seg{display:inline-flex;gap:6px;flex-wrap:wrap}"
        ".seg button{height:30px;padding:0 14px;border-radius:999px;border:1px solid var(--glass-border);"
        "background:var(--glass);color:var(--text-2);font-size:12px;cursor:pointer;transition:all .15s}"
        ".seg button:hover{color:var(--text)}"
        ".seg button.on{background:var(--accent-fill);color:var(--on-accent);border-color:transparent;box-shadow:0 0 12px var(--glow)}"
        ".toggles{display:flex;flex-wrap:wrap;gap:10px 26px;padding:6px 0 6px 0}"
        ".tgl{display:flex;align-items:center;gap:9px;cursor:pointer;color:var(--text);font-size:13px}"
        ".tgl input{display:none}"
        ".sw{width:38px;height:20px;border-radius:999px;background:var(--track);position:relative;transition:background .15s;flex:0 0 auto}"
        ".sw::after{content:'';position:absolute;top:2px;left:2px;width:16px;height:16px;border-radius:50%;background:#fff;transition:transform .15s;box-shadow:0 1px 2px rgba(0,0,0,.3)}"
        ".tgl input:checked+.sw{background:var(--accent-fill)}"
        ".tgl input:checked+.sw::after{transform:translateX(18px)}"
        ".dirline{color:var(--text-3);font-size:12px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        ".sbtns{display:flex;gap:12px;margin-top:14px;flex-wrap:wrap}"
        ".hint{margin-top:12px;font-size:12px;color:var(--text-3)}"
        // ---- 说明页 ----
        ".help{flex:1 1 auto;min-height:0;overflow-y:auto;padding:22px 26px}"
        ".help h2{font-size:18px;margin-bottom:16px;color:var(--text)}"
        ".hrow{padding:10px 0;border-bottom:1px solid var(--divider)}"
        ".hrow:last-child{border-bottom:none}"
        ".hk{font-size:13px;font-weight:600;color:var(--accent-text);margin-bottom:4px}"
        ".hb{font-size:12px;color:var(--text-2);line-height:1.7}"
        // ---- 状态栏 ----
        ".statusbar{flex:0 0 auto;height:32px;display:flex;align-items:center;gap:10px;padding:0 14px;margin:0 0 8px}"
        ".dot{width:8px;height:8px;border-radius:50%;background:var(--text-3);flex:0 0 auto}"
        "#stext{font-size:12px;color:var(--text-2);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;max-width:40vw}"
        ".grow{flex:1 1 auto}"
        ".cnt{font-size:12px;color:var(--text-2)}"
        ".cnt b{color:var(--accent-text);font-weight:600;margin-left:3px}"
        "#sdir{font-size:11px;color:var(--text-3);max-width:30vw;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;direction:rtl}"
        // ---- 缩放手柄 ----
        ".rz{position:fixed;z-index:5}"
        ".rz.l{left:0;top:10px;bottom:10px;width:6px;cursor:ew-resize}"
        ".rz.r{right:0;top:10px;bottom:10px;width:6px;cursor:ew-resize}"
        ".rz.t{top:0;left:10px;right:10px;height:6px;cursor:ns-resize}"
        ".rz.b{bottom:0;left:10px;right:10px;height:6px;cursor:ns-resize}"
        ".rz.tl{left:0;top:0;width:12px;height:12px;cursor:nwse-resize}"
        ".rz.tr{right:0;top:0;width:12px;height:12px;cursor:nesw-resize}"
        ".rz.bl{left:0;bottom:0;width:12px;height:12px;cursor:nesw-resize}"
        ".rz.br{right:0;bottom:0;width:12px;height:12px;cursor:nwse-resize}";
    h += "#preset{" + arrowCss + "}";
    h += "select.field{" + arrowCss + "}";
    h += "</style></head><body class=\"" + std::string(p.dark ? "dark" : "light") + "\">";

    // 环境光斑层
    h += "<div class=\"amb a\"></div><div class=\"amb b\"></div><div class=\"amb c\"></div>";

    // ---- 外框 ----
    h += "<div class=\"frame\">";

    // 标题栏
    h += "<div class=\"titlebar\">"
         "<div class=\"brand\"><span class=\"logo\">C</span><span class=\"bt\">cookie抓取工具3.0</span></div>"
         "<div class=\"dragfill\" id=\"drag\"></div>"
         "<div class=\"winbtns\">"
         "<button class=\"wb\" id=\"wmin\" title=\"最小化\">&#xE921;</button>"
         "<button class=\"wb\" id=\"wmax\" title=\"最大化\">&#xE922;</button>"
         "<button class=\"wb close\" id=\"wclose\" title=\"关闭\">&#xE8BB;</button>"
         "</div></div>";

    // 顶部导航
    auto tabOn = [&](int pg) -> std::string { return st.page == pg ? " class=\"tab on\"" : " class=\"tab\""; };
    h += "<div class=\"nav\">";
    h += "<button" + tabOn(0) + " data-p=\"0\">浏览器</button>";
    h += "<button" + tabOn(1) + " data-p=\"1\">设置</button>";
    h += "<button" + tabOn(2) + " data-p=\"2\">说明</button>";
    h += "</div>";

    // 视图
    h += "<div class=\"views\">";

    // 浏览器页
    std::string addrInit = HtmlEsc(util::WideToUtf8(st.address));
    std::string dl;
    for (auto& pr : set.presets) dl += "<option value=\"" + HtmlEsc(util::WideToUtf8(pr.url)) + "\"></option>";
    h += "<section class=\"view" + std::string(st.page == 0 ? " on" : "") + "\" id=\"v0\">";
    h += "<div class=\"toolbar glass\">"
         "<input id=\"address\" class=\"field\" list=\"presets\" placeholder=\"输入网址并回车，或点右侧打开\" value=\"" + addrInit + "\">"
         "<datalist id=\"presets\">" + dl + "</datalist>"
         "<select id=\"preset\" class=\"field\"><option value=\"-1\">自定义（手动输入）</option></select>"
         "<input id=\"regex\" class=\"field\" placeholder=\"筛选正则（按 Cookie 名）\" spellcheck=\"false\">"
         "<button id=\"open\" class=\"btn primary\">打开</button>"
         "<button id=\"refresh\" class=\"btn ghost\">刷新</button>"
         "<button id=\"capCookie\" class=\"btn cap\">抓取 Cookie</button>"
         "<button id=\"capToken\" class=\"btn cap\">抓取 Token</button>"
         "</div>";
    h += "<div class=\"webhole\" id=\"webhole\"></div>";
    h += "</section>";

    // 设置页
    h += "<section class=\"view" + std::string(st.page == 1 ? " on" : "") + "\" id=\"v1\">";
    h += "<div class=\"sheet glass\">";
    h += "<div class=\"crow\"><label>网站预设</label><select id=\"s-preset\" class=\"field\"></select></div>";
    h += "<div class=\"crow\"><label>登录网址</label><input id=\"s-login\" class=\"field\" spellcheck=\"false\"></div>";
    h += "<div class=\"crow\"><label>日志文件</label><input id=\"s-logfile\" class=\"field\" readonly value=\"cookie.log\"></div>";
    h += "<div class=\"crow\"><label>Cookie 格式</label><div class=\"seg\" id=\"s-fmt\">"
         "<button data-v=\"0\">字符串</button><button data-v=\"1\">JSON</button><button data-v=\"2\">两种都写</button></div></div>";
    h += "<div class=\"toggles\">";
    auto tgl = [&](const char* id, bool on, const std::string& label) {
        return std::string("<label class=\"tgl\"><input type=\"checkbox\" id=") + id + (on ? " checked" : "") +
               "><span class=\"sw\"></span>" + HtmlEsc(label) + "</label>";
    };
    h += tgl("t-auto", set.autoCapture, "自动抓取（检测到登录后自动写入）");
    h += tgl("t-clip", set.clipboard, "抓取后复制到剪贴板");
    h += tgl("t-hist", set.history, "记录历史日志（追加保存）");
    h += tgl("t-first", set.firstOpenCapture, "首次打开若已是登录则自动抓取");
    h += "</div>";
    h += "<div class=\"crow\"><label>数据目录</label><span class=\"dirline\">" + HtmlEsc(util::WideToUtf8(st.logDir)) + "</span></div>";
    h += "<div class=\"sbtns\">"
         "<button id=\"s-save\" class=\"btn primary\">保存设置</button>"
         "<button id=\"s-clear\" class=\"btn danger\">清除登录状态</button>"
         "<button id=\"s-openlog\" class=\"btn ghost\">打开日志文件夹</button></div>";
    h += "<div class=\"hint\" id=\"s-hint\"></div>";
    h += "</div></section>";

    // 说明页
    h += "<section class=\"view" + std::string(st.page == 2 ? " on" : "") + "\" id=\"v2\">";
    h += "<div class=\"help glass\"><h2>使用说明</h2>";
    struct HL { const char* k; const char* b; };
    static const HL lines[] = {
        { "浏览器", "地址栏输入网址后点击「打开」；登录网站后即可抓取 Cookie / Token。" },
        { "抓取 Cookie", "仅拉取当前站点的 Cookie 并写入日志（cookie.log / cookie.json / cookie_detail.json）。" },
        { "抓取 Token", "扫描当前页面存储中的 Token（localStorage / sessionStorage / 请求头 / 响应体）。" },
        { "设置", "选择网站预设回填登录网址、切换 Cookie 格式、勾选抓取选项，「保存设置」立即生效。" },
        { "数据与日志", "「打开日志文件夹」定位日志目录；cookie.log 覆盖写，cookie_history.log 追加全部记录。" },
        { "清除登录状态", "删除本程序保存的全部登录数据（Cookie、Token、网站会话），不可恢复，需重新登录。" },
    };
    for (auto& ln : lines)
        h += "<div class=\"hrow\"><div class=\"hk\">" + HtmlEsc(ln.k) + "</div><div class=\"hb\">" + HtmlEsc(ln.b) + "</div></div>";
    h += "</div></section>";

    h += "</div>"; // .views

    // 状态栏
    h += "<div class=\"statusbar glass\">"
         "<span class=\"dot\" id=\"sdot\"></span><span id=\"stext\">就绪</span>"
         "<span class=\"grow\"></span>"
         "<span class=\"cnt\">Cookie<b id=\"ccount\">0</b></span>"
         "<span class=\"cnt\">Token<b id=\"tcount\">0</b></span>"
         "<span id=\"sdir\"></span>"
         "</div>";

    h += "</div>"; // .frame

    // 缩放手柄
    const char* dirs[] = { "tl", "t", "tr", "r", "b", "br", "bl", "l" };
    for (auto d : dirs) h += "<div class=\"rz " + std::string(d) + "\" data-d=\"" + d + "\"></div>";

    // ---- 脚本 ----
    std::string presetsArr = PresetsJsonArray(set.presets);
    int fmtInit = set.logFormat == LogFormat::Json ? 1 : (set.logFormat == LogFormat::Both ? 2 : 0);
    std::string initObj = StateObj(st);

    h += "<script>"
        "(function(){"
        "var PRESETS=" + presetsArr + ";"
        "var FMT=" + std::to_string(fmtInit) + ";"
        "function $(id){return document.getElementById(id);}"
        "function post(o){try{window.chrome.webview.postMessage(JSON.stringify(o));}catch(e){}}"
        "function curRe(){var e=$('regex');return e?(e.value||'').trim():'';}"
        // 地址框：browser 工具栏
        "var addr=$('address'),reEl=$('regex'),presetSel=$('preset');"
        "for(var i=0;i<PRESETS.length;i++){var op=document.createElement('option');op.value=i;op.text=PRESETS[i].name;presetSel.add(op);}"
        "function fillPreset(sel){if(sel>=0&&PRESETS[sel])addr.value=PRESETS[sel].url;addr.focus();}"
        "presetSel.addEventListener('change',function(){fillPreset(parseInt(this.value,10));});"
        "function doOpen(){var u=(addr.value||'').trim();if(u)post({t:'open',url:u});}"
        "$('open').addEventListener('click',doOpen);"
        "addr.addEventListener('keydown',function(e){if(e.key==='Enter')doOpen();});"
        "$('refresh').addEventListener('click',function(){post({t:'reload'});});"
        "$('capCookie').addEventListener('click',function(){post({t:'cap',m:1,re:curRe()});});"
        "$('capToken').addEventListener('click',function(){post({t:'cap',m:2,re:curRe()});});"
        "var reTimer=0;"
        "reEl.addEventListener('input',function(){clearTimeout(reTimer);var v=this.value;reTimer=setTimeout(function(){post({t:'regex',re:v});},300);});"
        // 导航标签
        "var tabs=document.querySelectorAll('.nav .tab');"
        "for(var k=0;k<tabs.length;k++){(function(t){t.addEventListener('click',function(){post({t:'page',p:parseInt(t.getAttribute('data-p'),10)});});})(tabs[k]);}"
        // 窗口按钮 + 拖拽 + 双击最大化
        "$('wmin').addEventListener('click',function(){post({t:'win',a:'min'});});"
        "$('wmax').addEventListener('click',function(){post({t:'win',a:'max'});});"
        "$('wclose').addEventListener('click',function(){post({t:'win',a:'close'});});"
        "var _dLast=0,_dX=0,_dY=0;"
        "$('drag').addEventListener('mousedown',function(e){"
        "if(e.button!==0)return;"
        "var now=Date.now(),dbl=(now-_dLast<500)&&Math.abs(e.clientX-_dX)<=4&&Math.abs(e.clientY-_dY)<=4;"
        "_dLast=now;_dX=e.clientX;_dY=e.clientY;"
        "if(dbl){_dLast=0;e.preventDefault();post({t:'win',a:'max'});}else{post({t:'win',a:'drag'});}"
        "});"
        // 缩放手柄
        "var rzs=document.querySelectorAll('.rz');"
        "for(var m=0;m<rzs.length;m++){(function(r){r.addEventListener('mousedown',function(e){if(e.button===0)post({t:'win',a:'resize',d:r.getAttribute('data-d')});});})(rzs[m]);}"
        // 设置页表单
        "var sp=$('s-preset'),slogin=$('s-login');"
        "for(var n=0;n<PRESETS.length;n++){var o2=document.createElement('option');o2.value=n;o2.text=PRESETS[n].name;sp.add(o2);}"
        "function syncLogin(){var s=parseInt(sp.value,10);if(s>=0&&PRESETS[s])slogin.value=PRESETS[s].url;}"
        "sp.addEventListener('change',syncLogin);"
        "if(PRESETS.length){sp.value=0;syncLogin();}"
        "var fmtBtns=document.querySelectorAll('#s-fmt button');"
        "function setFmt(v){for(var q=0;q<fmtBtns.length;q++)fmtBtns[q].classList.toggle('on',parseInt(fmtBtns[q].getAttribute('data-v'),10)===v);}"
        "for(var f=0;f<fmtBtns.length;f++){(function(b){b.addEventListener('click',function(){setFmt(parseInt(b.getAttribute('data-v'),10));});})(fmtBtns[f]);}"
        "setFmt(FMT);"
        "$('s-save').addEventListener('click',function(){"
            "var sel=parseInt(sp.value,10);var url=(slogin.value||'').trim();"
            "var ps=[];for(var x=0;x<PRESETS.length;x++){ps.push({name:PRESETS[x].name,url:(x===sel&&url)?url:PRESETS[x].url});}"
            "post({t:'cfg',a:'save',data:{"
            "autoCapture:$('t-auto').checked,clipboard:$('t-clip').checked,"
            "history:$('t-hist').checked,firstOpenCapture:$('t-first').checked,"
            "logFormat:FMT,presets:ps}});"
            "});"
        "$('s-clear').addEventListener('click',function(){post({t:'cfg',a:'clearlogin'});});"
        "$('s-openlog').addEventListener('click',function(){post({t:'cfg',a:'openlog'});});"
        // 内容视口孔 rect 上报（ResizeObserver + resize，防抖）
        "function reportLayout(){var el=$('webhole');if(!el)return;var r=el.getBoundingClientRect();"
            "post({t:'layout',x:r.left,y:r.top,w:r.width,h:r.height,dpr:window.devicePixelRatio||1});}"
        "var loTimer=0;function scheduleLayout(){clearTimeout(loTimer);loTimer=setTimeout(reportLayout,40);}"
        "if(window.ResizeObserver){try{new ResizeObserver(scheduleLayout).observe($('webhole'));}catch(e){}}"
        "window.addEventListener('resize',scheduleLayout);"
        // 状态推送入口
        "window.CG={};"
        "window.CG.state=function(s){"
            "if(!s)return;"
            "if(typeof s.page==='number'){for(var i=0;i<3;i++){var v=$('v'+i);if(v)v.classList.toggle('on',s.page===i);"
            "var t=document.querySelectorAll('.nav .tab')[i];if(t)t.classList.toggle('on',s.page===i);}}"
            "var st=$('stext');if(st)st.textContent=s.status&&s.status.length?s.status:'就绪';"
            "var d=$('sdot');if(d){var x=s.status||'';var c='var(--text-3)';"
            "if(x.indexOf('成功')>=0)c='var(--success)';else if(x.indexOf('失败')>=0||x.indexOf('错误')>=0)c='var(--danger)';"
            "else if(x.indexOf('正在')>=0||x.indexOf('抓取中')>=0)c='var(--accent)';d.style.background=c;}"
            "var cc=$('ccount');if(cc)cc.textContent=s.cookieCount||0;"
            "var tc=$('tcount');if(tc)tc.textContent=s.tokenCount||0;"
            "var dir=$('sdir');if(dir)dir.textContent=s.logDir||'';"
            "var hi=$('s-hint');if(hi)hi.textContent=s.hint||'提示：修改后点击「保存设置」立即生效。';"
            "if(typeof s.address==='string'&&addr&&document.activeElement!==addr)addr.value=s.address;"
            "scheduleLayout();"
            "};"
        "post({t:'ready'});"
        "reportLayout();"
        "})();</script>";

    h += "</body></html>";
    return h;
}

} // namespace shell
