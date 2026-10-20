#include "shellhtml.h"
#include "theme.h"

namespace shellhtml {

std::string Build() {
    std::string html;
    // 头部：先注入同源调色板 CSS 变量（theme::StartPageCss），再叠加外壳样式
    html += "<!DOCTYPE html><html lang=\"zh\"><head><meta charset=\"utf-8\"><style>";
    html += theme::StartPageCss();
    html += R"CSS(
*{box-sizing:border-box;margin:0;padding:0}
:root{--ctl-h:36px}
html,body{height:100%}
body{font-family:'Segoe UI','Microsoft YaHei',sans-serif;background:var(--canvas);color:var(--text);
display:flex;flex-direction:column;height:100vh;overflow:hidden;-webkit-user-select:none;user-select:none}
input,select{user-select:text;-webkit-user-select:text}
button{font-family:inherit}
/* 标题栏 */
#titlebar{flex:0 0 46px;display:flex;align-items:center;padding-left:12px;background:var(--surface);
border-bottom:1px solid var(--divider)}
body.inactive #titlebar{background:var(--surface-alt)}
#appIcon{width:18px;height:18px;border-radius:5px;background:linear-gradient(135deg,var(--accent),var(--accent-fill));
color:var(--on-accent);font-size:11px;font-weight:700;display:flex;align-items:center;justify-content:center;margin-right:10px}
#title{font-size:13px;color:var(--text);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;flex:1}
body.inactive #title{color:var(--text-3)}
#winbtns{display:flex;height:100%}
.tbbtn{width:46px;height:100%;border:none;background:transparent;color:var(--text-2);
font-family:'Segoe MDL2 Assets';font-size:10px;cursor:pointer;display:flex;align-items:center;justify-content:center}
.tbbtn:hover{background:var(--hover)}
#btnClose:hover{background:var(--danger);color:var(--on-accent)}
/* 导航 */
#nav{flex:0 0 44px;display:flex;align-items:center;justify-content:center;gap:6px;background:var(--surface);
border-bottom:1px solid var(--divider)}
.tab{height:32px;min-width:80px;padding:0 14px;border:none;border-radius:9px;background:transparent;
color:var(--text-2);font-size:13px;cursor:pointer;transition:background .15s,color .15s}
.tab:hover{background:var(--hover);color:var(--text)}
.tab.active{background:var(--accent-tint);color:var(--accent-text);font-weight:600;box-shadow:inset 0 0 0 1px var(--border)}
/* 工具栏 */
#toolbar{flex:0 0 auto;display:none;align-items:center;gap:10px;flex-wrap:wrap;padding:12px 24px;background:var(--canvas)}
body[data-page="0"] #toolbar{display:flex}
.field{height:var(--ctl-h);border-radius:var(--radius-control);border:1px solid var(--border);background:var(--field-bg);
color:var(--text);padding:0 12px;font-size:13px;outline:none;transition:border-color .15s,box-shadow .15s}
.field::placeholder{color:var(--text-3)}
.field:focus{border-color:var(--accent);box-shadow:0 0 0 3px var(--accent-tint)}
#address{flex:1 1 240px;min-width:160px}
/* 预设下拉：去掉 UA 箭头，改由 .sel 伪元素画 V 形（可随主题变色）；option/弹出列表靠 color-scheme 跟随深浅色 */
#tbPreset,#setPreset{-webkit-appearance:none;appearance:none;cursor:pointer;padding-right:32px}
.sel{position:relative;display:inline-flex;align-items:center;height:var(--ctl-h)}
.sel select{flex:1;min-width:0;height:100%}
.sel::after{content:'';position:absolute;right:12px;top:50%;width:6px;height:6px;
border-right:1.5px solid var(--text-3);border-bottom:1.5px solid var(--text-3);
transform:translateY(-72%) rotate(45deg);pointer-events:none;transition:border-color .15s}
.sel:hover::after{border-color:var(--accent-text)}
option{background:var(--surface);color:var(--text)}
#regex{flex:0 1 200px;min-width:130px;font-family:Consolas,'Courier New',monospace}
/* 按钮基准：与输入框等高（--ctl-h），柔和浅底 + 细描边，替代原先高饱和渐变与重阴影 */
.btn{height:var(--ctl-h);border-radius:var(--radius-control);border:1px solid var(--border);padding:0 16px;
font-size:13px;cursor:pointer;white-space:nowrap;background:var(--surface);color:var(--text-2);
transition:background .15s,color .15s,border-color .15s}
.btn:hover{background:var(--hover);color:var(--text)}
.btn:active{background:var(--pressed)}
/* 主操作（打开/保存）：主题色淡底柔和强调 */
.primary{background:var(--accent-tint);border-color:transparent;color:var(--accent-text);font-weight:600}
.primary:hover{background:color-mix(in srgb,var(--accent) 16%,var(--surface));color:var(--accent-text)}
.primary:active{background:color-mix(in srgb,var(--accent) 24%,var(--surface))}
/* 抓取按钮：描边式强调，不用大面积实色 */
.cap{background:transparent;border-color:color-mix(in srgb,var(--accent) 45%,var(--border));color:var(--accent-text);font-weight:600}
.cap:hover{background:var(--accent-tint);color:var(--accent-text)}
.cap:active{background:color-mix(in srgb,var(--accent) 22%,var(--surface))}
/* 危险操作（清除登录）：细红边 + 淡红底 hover */
.danger{color:var(--danger);border-color:color-mix(in srgb,var(--danger) 40%,var(--border))}
.danger:hover{background:color-mix(in srgb,var(--danger) 8%,var(--surface));color:var(--danger)}
.danger:active{background:color-mix(in srgb,var(--danger) 14%,var(--surface))}
/* 主区 */
#main{flex:1 1 auto;position:relative;overflow:hidden}
#webSlot{position:absolute;left:24px;right:24px;top:12px;bottom:12px;display:none;border-radius:var(--radius-control);
background:var(--surface);border:1px solid var(--border)}
body[data-page="0"] #webSlot{display:block}
#settings,#help{position:absolute;left:0;right:0;top:0;bottom:0;overflow-y:auto;padding:20px 24px;display:none}
body[data-page="1"] #settings{display:block}
body[data-page="2"] #help{display:block}
/* 卡片与设置表单 */
.card{max-width:760px;margin:0 auto;background:var(--surface);border:1px solid var(--border);
border-radius:var(--radius-card);padding:22px 24px;box-shadow:0 6px 18px rgba(0,0,0,.08)}
.row{display:flex;align-items:flex-start;gap:12px;margin-bottom:16px;flex-wrap:wrap}
.row>.lbl{flex:0 0 88px;font-size:13px;color:var(--text-2);padding-top:8px}
.ctl{flex:1 1 240px;min-width:200px;max-width:460px}
.checks{display:flex;flex-direction:column;gap:10px}
.chk{display:flex;align-items:center;gap:8px;font-size:13px;color:var(--text);cursor:pointer}
.chk input{accent-color:var(--accent);width:16px;height:16px}
input[type=radio]{accent-color:var(--accent)}
.seg{display:inline-flex;border:1px solid var(--border);border-radius:var(--radius-control);overflow:hidden;height:var(--ctl-h)}
.seg label{border-right:1px solid var(--border);cursor:pointer;background:var(--surface);height:100%}
.seg label:last-child{border-right:none}
.seg input{display:none}
.seg span{padding:0 16px;font-size:13px;display:inline-flex;align-items:center;height:100%;color:var(--text-2)}
.seg label:has(input:checked){background:var(--accent-tint)}
.seg label:has(input:checked) span{color:var(--accent-text);font-weight:600}
.dir{font-size:12px;color:var(--text-3);word-break:break-all;padding-top:8px}
.btnrow{gap:10px}
.hint{font-size:12px;color:var(--text-3);margin-top:4px}
/* 说明页 */
.card h2{font-size:16px;margin-bottom:16px;color:var(--text)}
.hrow{display:flex;gap:14px;margin-bottom:12px}
.hh{flex:0 0 110px;color:var(--accent-text);font-size:13px;font-weight:600}
.hb{flex:1;color:var(--text-2);font-size:13px;line-height:1.6}
/* 状态栏 */
#status{flex:0 0 30px;display:flex;align-items:center;gap:10px;padding:0 16px;background:var(--surface);
border-top:1px solid var(--divider);font-size:12px;color:var(--text-2)}
#dot{width:8px;height:8px;border-radius:50%;background:var(--text-3);flex:0 0 8px}
#dot.ok{background:var(--accent)}
#dot.busy{background:var(--accent-fill)}
#dot.err{background:var(--danger)}
#statusText{flex:0 1 auto;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
#counts{margin-left:auto;white-space:nowrap;color:var(--text-2)}
#counts b{color:var(--accent-text);font-weight:600}
#logDir{white-space:nowrap;overflow:hidden;text-overflow:ellipsis;max-width:42ch;color:var(--text-3)}
)CSS";
    html += "</style></head><body data-page=\"0\">";

    // 结构
    html += R"BODY(
<div id="titlebar">
<div id="appIcon">C</div>
<div id="title">Cookie 抓取工具</div>
<div id="winbtns">
<button id="btnMin" class="tbbtn"></button>
<button id="btnMax" class="tbbtn"></button>
<button id="btnClose" class="tbbtn"></button>
</div>
</div>
<div id="nav">
<button class="tab active" data-go="0">浏览器</button>
<button class="tab" data-go="1">设置</button>
<button class="tab" data-go="2">说明</button>
</div>
<div id="toolbar">
<input id="address" class="field" list="presets" placeholder="输入网址并回车，或点右侧打开">
<datalist id="presets"></datalist>
<span class="sel"><select id="tbPreset" class="field"></select></span>
<button id="open" class="btn primary">打开</button>
<button id="refresh" class="btn">刷新</button>
<input id="regex" class="field" placeholder="筛选正则（按 Cookie 名）" spellcheck="false">
<button id="capCookie" class="btn cap">抓取 Cookie</button>
<button id="capToken" class="btn cap">抓取 Token</button>
</div>
<div id="main">
<div id="webSlot"></div>
<div id="settings">
<div class="card">
<div class="row"><label class="lbl">网站预设</label><span class="sel ctl"><select id="setPreset" class="field"></select></span></div>
<div class="row"><label class="lbl">登录网址</label><input id="loginUrl" class="field ctl" placeholder="选择预设自动填充，或手动输入"></div>
<div class="row"><label class="lbl">日志文件</label><input id="logFile" class="field ctl" readonly></div>
<div class="row"><label class="lbl">Cookie 格式</label><div class="seg">
<label><input type="radio" name="fmt" value="0"><span>字符串</span></label>
<label><input type="radio" name="fmt" value="1"><span>JSON</span></label>
<label><input type="radio" name="fmt" value="2"><span>两种都写</span></label>
</div></div>
<div class="row"><label class="lbl">抓取选项</label><div class="checks ctl">
<label class="chk"><input type="checkbox" id="chkAuto"><span>自动抓取（检测到登录后自动写入）</span></label>
<label class="chk"><input type="checkbox" id="chkClip"><span>抓取后复制到剪贴板</span></label>
<label class="chk"><input type="checkbox" id="chkHist"><span>记录历史日志（追加保存）</span></label>
<label class="chk"><input type="checkbox" id="chkFirst"><span>首次打开若已是登录则自动抓取</span></label>
</div></div>
<div class="row"><label class="lbl">数据目录</label><div id="dataDir" class="ctl dir"></div></div>
<div class="row btnrow">
<button id="saveBtn" class="btn primary">保存设置</button>
<button id="clearBtn" class="btn danger">清除登录状态</button>
<button id="openLogBtn" class="btn">打开日志文件夹</button>
</div>
<div id="hint" class="hint"></div>
</div>
</div>
<div id="help"><div class="card">
<h2>使用说明</h2>
<div class="hrow"><div class="hh">浏览器</div><div class="hb">地址栏输入网址后点击「打开」；登录网站后即可抓取 Cookie / Token。</div></div>
<div class="hrow"><div class="hh">立即抓取Cookie</div><div class="hb">仅拉取当前站点的 Cookie 并写入日志（cookie.log / cookie.json / cookie_detail.json），不扫描页面存储。</div></div>
<div class="hrow"><div class="hh">立即抓取Token</div><div class="hb">仅扫描当前页面存储中的 Token（localStorage / sessionStorage 等），不读取 Cookie。</div></div>
<div class="hrow"><div class="hh">筛选正则</div><div class="hb">在工具栏正则框按 Cookie 名做部分匹配筛选，命中项才写入日志与剪贴板；留空表示不筛选。</div></div>
<div class="hrow"><div class="hh">设置</div><div class="hb">选择网站预设回填登录网址、切换 Cookie 格式（字符串 / JSON / 两种都写）、勾选抓取选项，点击「保存设置」立即生效。</div></div>
<div class="hrow"><div class="hh">数据与日志</div><div class="hb">「打开日志文件夹」定位日志输出目录；cookie.log 每次抓取覆盖写，cookie_history.log 追加保存全部抓取记录。</div></div>
<div class="hrow"><div class="hh">清除登录状态</div><div class="hb">删除本程序保存的全部登录数据（Cookie、Token、网站会话），操作不可恢复，需重新登录。</div></div>
</div></div>
</div>
<div id="status">
<div id="dot"></div>
<div id="statusText">就绪</div>
<div id="counts">Cookie <b id="cCount">0</b>&nbsp;·&nbsp;Token <b id="tCount">0</b></div>
<div id="logDir"></div>
</div>
)BODY";

    // 脚本
    html += R"JS(
<script>
(function(){
var body=document.body;
function post(o){try{window.chrome.webview.postMessage(JSON.stringify(o));}catch(e){}}
document.getElementById('btnMin').textContent='\uE921';
document.getElementById('btnMax').textContent='\uE922';
document.getElementById('btnClose').textContent='\uE8BB';
var addr=document.getElementById('address');
var regexEl=document.getElementById('regex');
function curRe(){return (regexEl.value||'').trim();}
var PRESETS=[];
function optEsc(s){return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');}
function buildPresets(list){
PRESETS=list||[];
var tb=document.getElementById('tbPreset'),sp=document.getElementById('setPreset'),dl=document.getElementById('presets');
var opts='<option value="-1">自定义（手动输入）</option>',dat='';
for(var i=0;i<PRESETS.length;i++){
opts+='<option value="'+i+'">'+optEsc(PRESETS[i].name)+'</option>';
dat+='<option value="'+optEsc(PRESETS[i].url)+'"></option>';
}
tb.innerHTML=opts;tb.selectedIndex=0;sp.innerHTML=opts;dl.innerHTML=dat;
}
function fillForm(f){
document.getElementById('chkAuto').checked=!!f.auto;
document.getElementById('chkClip').checked=!!f.clip;
document.getElementById('chkHist').checked=!!f.hist;
document.getElementById('chkFirst').checked=!!f.first;
if('loginUrl' in f)document.getElementById('loginUrl').value=f.loginUrl||'';
if('dataDir' in f)document.getElementById('dataDir').textContent=f.dataDir||'';
if('logFile' in f)document.getElementById('logFile').value=f.logFile||'';
var seg=f.seg|0,radios=document.getElementsByName('fmt');
for(var i=0;i<radios.length;i++)radios[i].checked=(parseInt(radios[i].value,10)===seg);
if('presetIdx' in f)document.getElementById('setPreset').selectedIndex=f.presetIdx|0;
if('hint' in f)document.getElementById('hint').textContent=f.hint||'';
}
function setStatus(s){
var t=s||'就绪';document.getElementById('statusText').textContent=t;
var d=document.getElementById('dot');d.className='';
if(t.indexOf('成功')>=0||t.indexOf('已抓取')>=0)d.className='ok';
else if(t.indexOf('失败')>=0||t.indexOf('错误')>=0)d.className='err';
else if(t.indexOf('正在')>=0||t.indexOf('刷新')>=0||t.indexOf('抓取中')>=0)d.className='busy';
}
function setPage(n){body.setAttribute('data-page',String(n));
var tabs=document.querySelectorAll('.tab');
for(var i=0;i<tabs.length;i++)tabs[i].classList.toggle('active',parseInt(tabs[i].getAttribute('data-go'),10)===n);}
window.__cgUpdate=function(o){
if(!o)return;
if('page' in o)setPage(o.page);
if('active' in o)body.classList.toggle('inactive',!o.active);
if('maximized' in o){body.classList.toggle('max',!!o.maximized);document.getElementById('btnMax').textContent=o.maximized?'\uE923':'\uE922';}
if('title' in o)document.getElementById('title').textContent=o.title;
if('status' in o)setStatus(o.status);
if('cookieCount' in o)document.getElementById('cCount').textContent=o.cookieCount;
if('tokenCount' in o)document.getElementById('tCount').textContent=o.tokenCount;
if('logDir' in o)document.getElementById('logDir').textContent=o.logDir;
if('address' in o)addr.value=o.address||'';
if(o.presets)buildPresets(o.presets);
if(o.form)fillForm(o.form);
reportSlot();
};
var tbEl=document.getElementById('titlebar');
tbEl.addEventListener('mousedown',function(e){if(e.button!==0)return;if(e.target.closest('.tbbtn'))return;post({sh:'drag'});});
tbEl.addEventListener('dblclick',function(e){if(e.target.closest('.tbbtn'))return;post({sh:'togglemax'});});
document.getElementById('btnMin').addEventListener('click',function(){post({sh:'min'});});
document.getElementById('btnMax').addEventListener('click',function(){post({sh:'togglemax'});});
document.getElementById('btnClose').addEventListener('click',function(){post({sh:'close'});});
Array.prototype.forEach.call(document.querySelectorAll('.tab'),function(t){
t.addEventListener('click',function(){post({sh:'page',idx:parseInt(t.getAttribute('data-go'),10)});});});
document.getElementById('tbPreset').addEventListener('change',function(){
var i=parseInt(this.value,10);if(i>=0&&PRESETS[i])addr.value=PRESETS[i].url;addr.focus();});
function doOpen(){var u=(addr.value||'').trim();if(u)post({tb:'open',url:u});}
document.getElementById('open').addEventListener('click',doOpen);
addr.addEventListener('keydown',function(e){if(e.key==='Enter')doOpen();});
document.getElementById('refresh').addEventListener('click',function(){post({tb:'refresh'});});
document.getElementById('capCookie').addEventListener('click',function(){post({tb:'capcookie',re:curRe()});});
document.getElementById('capToken').addEventListener('click',function(){post({tb:'captoken',re:curRe()});});
var reTimer=0;
regexEl.addEventListener('input',function(){clearTimeout(reTimer);var v=this.value;
reTimer=setTimeout(function(){post({tb:'regex',re:v});},300);});
document.getElementById('setPreset').addEventListener('change',function(){post({sh:'preset',idx:parseInt(this.value,10)});});
document.getElementById('loginUrl').addEventListener('input',function(){post({sh:'loginurl',text:this.value});});
[['chkAuto','auto'],['chkClip','clip'],['chkHist','hist'],['chkFirst','first']].forEach(function(p){
document.getElementById(p[0]).addEventListener('change',function(){post({sh:'toggle',field:p[1],on:this.checked});});});
Array.prototype.forEach.call(document.getElementsByName('fmt'),function(r){
r.addEventListener('change',function(){if(r.checked)post({sh:'seg',idx:parseInt(r.value,10)});});});
document.getElementById('saveBtn').addEventListener('click',function(){post({sh:'save'});});
document.getElementById('openLogBtn').addEventListener('click',function(){post({sh:'openlog'});});
document.getElementById('clearBtn').addEventListener('click',function(){post({sh:'clearlogin'});});
function reportSlot(){
var el=document.getElementById('webSlot');
if(body.getAttribute('data-page')!=='0'||!el||el.offsetWidth<=0||el.offsetHeight<=0){post({sh:'slot',x:0,y:0,w:0,h:0});return;}
var r=el.getBoundingClientRect(),d=window.devicePixelRatio||1;
post({sh:'slot',x:Math.round(r.left*d),y:Math.round(r.top*d),w:Math.round(r.width*d),h:Math.round(r.height*d)});
}
window.addEventListener('resize',reportSlot);
try{var ro=new ResizeObserver(function(){reportSlot();});
ro.observe(document.getElementById('webSlot'));ro.observe(document.getElementById('main'));}catch(e){}
window.__cgReportSlot=reportSlot;
post({sh:'ready'});
})();
</script>
)JS";

    html += "</body></html>";
    return html;
}

} // namespace shellhtml
