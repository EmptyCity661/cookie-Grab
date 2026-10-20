#include "webview.h"
#include "util.h"
#include "json.h"
#include "WebView2.h"
#include <shlobj.h>
#include <objbase.h>

// 内嵌加载器资源 ID（见 res/app.rc 与 res_ids.h）
#define RES_LOADER_DLL 101

typedef HRESULT(STDAPICALLTYPE* FnCreateEnv)(PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions*,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);

// ---------------- COM 处理器 ----------------
class EnvHandlerImpl : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
    LONG ref_ = 1;
    WebViewHost* h_;
public:
    EnvHandlerImpl(WebViewHost* h) : h_(h) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)) {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&ref_); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = InterlockedDecrement(&ref_);
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Environment* env) override {
        h_->OnEnvCreated(errorCode, env);
        return S_OK;
    }
};

class ControllerHandlerImpl : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
    LONG ref_ = 1;
    WebViewHost* h_;
public:
    ControllerHandlerImpl(WebViewHost* h) : h_(h) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)) {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&ref_); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = InterlockedDecrement(&ref_);
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Controller* controller) override {
        h_->OnControllerCreated(errorCode, controller);
        return S_OK;
    }
};

class NavCompletedHandlerImpl : public ICoreWebView2NavigationCompletedEventHandler {
    LONG ref_ = 1;
    WebViewHost* h_;
public:
    NavCompletedHandlerImpl(WebViewHost* h) : h_(h) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_ICoreWebView2NavigationCompletedEventHandler)) {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&ref_); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = InterlockedDecrement(&ref_);
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) override {
        BOOL ok = FALSE;
        if (args) args->get_IsSuccess(&ok);
        h_->OnNavCompleted(ok != FALSE);
        return S_OK;
    }
};

class WebMessageHandlerImpl : public ICoreWebView2WebMessageReceivedEventHandler {
    LONG ref_ = 1;
    WebViewHost* h_;
public:
    WebMessageHandlerImpl(WebViewHost* h) : h_(h) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_ICoreWebView2WebMessageReceivedEventHandler)) {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&ref_); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = InterlockedDecrement(&ref_);
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) override {
        if (args) {
            LPWSTR s = nullptr;
            if (SUCCEEDED(args->TryGetWebMessageAsString(&s)) && s) {
                h_->OnWebMessage(util::WideToUtf8(s));
                CoTaskMemFree(s);
            } else {
                LPWSTR j = nullptr;
                if (SUCCEEDED(args->get_WebMessageAsJson(&j)) && j) {
                    h_->OnWebMessage(util::WideToUtf8(j));
                    CoTaskMemFree(j);
                }
            }
        }
        return S_OK;
    }
};

class AddScriptHandlerImpl : public ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler {
    LONG ref_ = 1;
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler)) {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&ref_); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = InterlockedDecrement(&ref_);
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, LPCWSTR id) override {
        return S_OK;
    }
};

class GetCookiesHandlerImpl : public ICoreWebView2GetCookiesCompletedHandler {
    LONG ref_ = 1;
    WebViewHost* h_;
public:
    GetCookiesHandlerImpl(WebViewHost* h) : h_(h) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_ICoreWebView2GetCookiesCompletedHandler)) {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&ref_); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = InterlockedDecrement(&ref_);
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2CookieList* list) override {
        h_->OnCookiesReady(errorCode, list);
        return S_OK;
    }
};

class ExecScriptHandlerImpl : public ICoreWebView2ExecuteScriptCompletedHandler {
    LONG ref_ = 1;
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_ICoreWebView2ExecuteScriptCompletedHandler)) {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&ref_); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = InterlockedDecrement(&ref_);
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, LPCWSTR resultObjectAsJson) override {
        return S_OK; // 结果无需处理
    }
};

// ---------------- 加载器加载 ----------------
// 进程级共享的加载器 DLL：同一进程内多个 WebViewHost（如主浏览区 + HTML 工具栏）
// 复用同一份映射，引用计数归零时才 FreeLibrary——否则一个实例销毁会把另一个
// 仍在使用的 DLL 卸载，导致后续调用崩溃。
static HMODULE  s_loader = nullptr;
static std::wstring s_loaderPath;
static bool     s_loaderFromTemp = false;
static int      s_loaderRefs = 0;

static HMODULE LoadLoaderDll(std::wstring& outPath, bool& fromTemp) {
    outPath.clear();
    fromTemp = false;
    // 1) exe 同目录
    std::wstring p = util::ModuleDir() + L"WebView2Loader.dll";
    HMODULE h = LoadLibraryW(p.c_str());
    if (h) { outPath = p; return h; }
    // 2) 内嵌资源释放到 %TEMP%\CookieGrabber
    HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(RES_LOADER_DLL), RT_RCDATA);
    if (!r) return nullptr;
    HGLOBAL g = LoadResource(nullptr, r);
    DWORD sz = SizeofResource(nullptr, r);
    if (!g || !sz) return nullptr;
    const void* data = LockResource(g);
    if (!data) return nullptr;
    wchar_t tmpDir[MAX_PATH];
    GetTempPathW(MAX_PATH, tmpDir);
    std::wstring dir = std::wstring(tmpDir) + L"CookieGrabber";
    CreateDirectoryW(dir.c_str(), nullptr);
    std::wstring target = dir + L"\\WebView2Loader.dll";
    HANDLE f = CreateFileW(target.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return nullptr;
    DWORD written = 0;
    WriteFile(f, data, sz, &written, nullptr);
    CloseHandle(f);
    if (written != sz) { DeleteFileW(target.c_str()); return nullptr; }
    h = LoadLibraryW(target.c_str());
    if (h) { outPath = target; fromTemp = true; }
    return h;
}

// ---------------- WebViewHost ----------------
bool WebViewHost::Create(HWND parent, const std::wstring& userDataDir) {
    parent_ = parent;
    userDataDir_ = userDataDir;
    CreateDirectoryW(userDataDir_.c_str(), nullptr);

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (hr == RPC_E_CHANGED_MODE || hr == S_FALSE) { /* 已是 STA，可接受 */ }
    else if (FAILED(hr)) return false;

    // 注意：重建（Recreate）时复用已加载的加载器，不可重复释放或覆盖写
    // ——该文件仍处于 LoadLibrary 映射中，覆盖写会失败
    if (!loader_) {
        if (!s_loader) s_loader = LoadLoaderDll(s_loaderPath, s_loaderFromTemp);
        loader_ = s_loader;
        loaderPath_ = s_loaderPath;        // 仅作记录，释放由进程级计数统一管理
        loaderFromTemp_ = s_loaderFromTemp;
        if (!loader_) return false;
        s_loaderRefs++;
    }
    auto fn = (FnCreateEnv)GetProcAddress(loader_, "CreateCoreWebView2EnvironmentWithOptions");
    if (!fn) return false;

    auto* handler = new EnvHandlerImpl(this);
    hr = fn(nullptr, userDataDir_.c_str(), nullptr, handler);
    handler->Release();
    return SUCCEEDED(hr);
}

void WebViewHost::Destroy() {
    if (webview2_) { webview2_->Release(); webview2_ = nullptr; }
    if (webview_) { webview_->Release(); webview_ = nullptr; }
    if (cookieManager_) { cookieManager_->Release(); cookieManager_ = nullptr; }
    if (controller_) { controller_->Close(); controller_->Release(); controller_ = nullptr; }
    if (env_) { env_->Release(); env_ = nullptr; }
    // 加载器按引用计数归还：最后一个实例销毁时才卸载并清理临时文件
    if (loader_) {
        loader_ = nullptr;
        if (s_loaderRefs > 0) s_loaderRefs--;
        if (s_loaderRefs == 0 && s_loader) {
            FreeLibrary(s_loader);
            if (s_loaderFromTemp && !s_loaderPath.empty()) DeleteFileW(s_loaderPath.c_str());
            s_loader = nullptr;
            s_loaderPath.clear();
            s_loaderFromTemp = false;
        }
    }
}

void WebViewHost::OnEnvCreated(HRESULT hr, ICoreWebView2Environment* env) {
    if (FAILED(hr) || !env) return;
    env_ = env;
    env_->AddRef();
    auto* handler = new ControllerHandlerImpl(this);
    hr = env_->CreateCoreWebView2Controller(parent_, handler);
    handler->Release();
}

void WebViewHost::OnControllerCreated(HRESULT hr, ICoreWebView2Controller* controller) {
    if (FAILED(hr) || !controller) return;
    controller_ = controller;
    controller_->AddRef();
    if (FAILED(controller_->get_CoreWebView2(&webview_)) || !webview_) return;
    webview_->QueryInterface(IID_ICoreWebView2_2, (void**)&webview2_);
    controller_->put_IsVisible(TRUE);

    RECT rc;
    GetClientRect(parent_, &rc);
    controller_->put_Bounds(rc);

    auto* navH = new NavCompletedHandlerImpl(this);
    webview_->add_NavigationCompleted(navH, &navToken_);
    navH->Release();

    auto* msgH = new WebMessageHandlerImpl(this);
    webview_->add_WebMessageReceived(msgH, &msgToken_);
    msgH->Release();

    auto* scrH = new AddScriptHandlerImpl();
    webview_->AddScriptToExecuteOnDocumentCreated(TokenHookScript(), scrH);
    scrH->Release();

    if (!pendingNav_.empty()) {
        std::wstring url = pendingNav_;
        pendingNav_.clear();
        webview_->Navigate(url.c_str());
    } else if (!pendingNavToString_.empty()) {
        std::wstring html = pendingNavToString_;
        pendingNavToString_.clear();
        webview_->NavigateToString(html.c_str());
    }

    // 控制器已就绪：宿主窗口此前的「正在初始化」过渡画面必须失效重绘，
    // 否则 Web 卡片区域会残留启动时的那一帧（回收后还会露出一块空白）
    if (parent_) InvalidateRect(parent_, nullptr, FALSE);
    if (onReady_) onReady_();
}

void WebViewHost::Navigate(const std::wstring& url) {
    if (webview_) webview_->Navigate(url.c_str());
    else pendingNav_ = url;
}

void WebViewHost::NavigateToString(const std::wstring& html) {
    if (webview_) webview_->NavigateToString(html.c_str());
    else pendingNavToString_ = html;
}

void WebViewHost::Reload() {
    if (webview_) webview_->Reload();
}

void WebViewHost::DeleteAllCookies() {
    if (!webview2_) return;
    if (!cookieManager_) {
        if (FAILED(webview2_->get_CookieManager(&cookieManager_)) || !cookieManager_) return;
    }
    cookieManager_->DeleteAllCookies();
}

bool WebViewHost::Recreate(const std::wstring& userDataDir) {
    // 1) 注销事件并关闭现有会话
    if (webview_) {
        if (navToken_.value) { webview_->remove_NavigationCompleted(navToken_); navToken_ = {}; }
        if (msgToken_.value) { webview_->remove_WebMessageReceived(msgToken_); msgToken_ = {}; }
    }
    if (controller_) { controller_->Close(); controller_->Release(); controller_ = nullptr; }
    if (webview2_) { webview2_->Release(); webview2_ = nullptr; }
    if (webview_) { webview_->Release(); webview_ = nullptr; }
    if (cookieManager_) { cookieManager_->Release(); cookieManager_ = nullptr; }
    if (env_) { env_->Release(); env_ = nullptr; }
    pendingNav_.clear();
    pendingNavToString_.clear();
    userDataDir_ = userDataDir;
    // 2) 删除数据目录（浏览器进程退出需要时间，重试数次）
    for (int i = 0; i < 6; i++) {
        util::DeleteDirRecursive(userDataDir_);
        if (!util::DirExists(userDataDir_)) break;
        Sleep(300);
    }
    // 3) 重新创建环境与控制器（异步）
    return Create(parent_, userDataDir_);
}

void WebViewHost::ExecuteScript(const std::wstring& js) {
    if (!webview_) return;
    auto* h = new ExecScriptHandlerImpl();
    webview_->ExecuteScript(js.c_str(), h);
    h->Release();
}

void WebViewHost::GetCookies(const std::wstring& uri, CookiesFn cb) {
    cookiesCb_ = std::move(cb);
    if (!webview2_) return;
    if (!cookieManager_) {
        HRESULT hr = webview2_->get_CookieManager(&cookieManager_);
        if (FAILED(hr) || !cookieManager_) return;
    }
    auto* h = new GetCookiesHandlerImpl(this);
    cookieManager_->GetCookies(uri.c_str(), h);
    h->Release();
    // 注意：cookieManager_ 长驻保留（异步操作期间必须存活），Destroy 时释放
}

void WebViewHost::Resize(int x, int y, int w, int h) {
    if (!controller_) return;
    RECT rc = { x, y, x + w, y + h };
    controller_->put_Bounds(rc);
}

std::wstring WebViewHost::CurrentUrl() {
    if (!webview_) return {};
    LPWSTR s = nullptr;
    std::wstring url;
    if (SUCCEEDED(webview_->get_Source(&s)) && s) {
        url = s;
        CoTaskMemFree(s);
    }
    return url;
}

void WebViewHost::Focus() {
    if (controller_) controller_->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
}

void WebViewHost::OnNavCompleted(bool success) {
    if (onNav_) onNav_(success, CurrentUrl());
}

void WebViewHost::OnWebMessage(const std::string& json) {
    if (onMessage_) onMessage_(json);
}

void WebViewHost::OnCookiesReady(HRESULT hr, void* cookieListVoid) {
    auto* list = (ICoreWebView2CookieList*)cookieListVoid;
    std::vector<CookieItem> items;
    if (SUCCEEDED(hr) && list) {
        UINT32 count = 0;
        if (SUCCEEDED(list->get_Count(&count))) {
            for (UINT32 i = 0; i < count; i++) {
                ICoreWebView2Cookie* c = nullptr;
                if (FAILED(list->GetValueAtIndex(i, &c)) || !c) continue;
                CookieItem item;
                LPWSTR s = nullptr;
                auto grabStr = [&](std::wstring& dst, HRESULT(STDMETHODCALLTYPE ICoreWebView2Cookie::*getter)(LPWSTR*)) {
                    dst.clear();
                    if (SUCCEEDED((c->*getter)(&s)) && s) { dst = s; CoTaskMemFree(s); s = nullptr; }
                };
                grabStr(item.name, &ICoreWebView2Cookie::get_Name);
                grabStr(item.value, &ICoreWebView2Cookie::get_Value);
                grabStr(item.domain, &ICoreWebView2Cookie::get_Domain);
                grabStr(item.path, &ICoreWebView2Cookie::get_Path);
                if (SUCCEEDED(c->get_Expires(&item.expires)) && item.expires > 0) {
                    // 保留 Unix 秒
                } else {
                    item.expires = 0;
                }
                BOOL b = FALSE;
                item.httpOnly = SUCCEEDED(c->get_IsHttpOnly(&b)) && b;
                item.secure = SUCCEEDED(c->get_IsSecure(&b)) && b;
                item.isSession = SUCCEEDED(c->get_IsSession(&b)) && b;
                COREWEBVIEW2_COOKIE_SAME_SITE_KIND ss = COREWEBVIEW2_COOKIE_SAME_SITE_KIND_NONE;
                if (SUCCEEDED(c->get_SameSite(&ss))) item.sameSite = (int)ss;
                items.push_back(item);
                c->Release();
            }
        }
    }
    // 注意：cookieList 不归本处理器所有（WebView2 回调约定），不得 Release
    if (cookiesCb_) cookiesCb_(items);
}

// ---------------- Token 捕获 JS ----------------
const wchar_t* WebViewHost::TokenHookScript() {
    return LR"JS(
(function(){
  if (window.__cgInstalled) return;
  window.__cgInstalled = true;
  var TOKEN_KEY_RE = /token|jwt/i;
  var TOKEN_HEADERS = ['authorization','x-token','x-auth-token','x-access-token','x-api-token','x-session-token','x-jwt','token','x-auth','proxy-authorization'];
  var seen = {};
  function post(o){
    try {
      var k = o.source + '|' + o.name + '|' + o.value;
      if (seen[k]) return;
      if (Object.keys(seen).length > 800) seen = {};
      seen[k] = 1;
      window.chrome.webview.postMessage(JSON.stringify(o));
    } catch(e) {}
  }
  function looksToken(v){ return typeof v === 'string' && v.length >= 6 && v.length < 8192; }
  function scanObject(obj, path, url, source){
    if (!obj || typeof obj !== 'object') return;
    try {
      if (Array.isArray(obj)) { for (var i=0;i<obj.length;i++) scanObject(obj[i], path+'['+i+']', url, source); return; }
      for (var k in obj) {
        if (!Object.prototype.hasOwnProperty.call(obj,k)) continue;
        var v = obj[k], p = path ? path+'.'+k : k;
        if (v && typeof v === 'object') scanObject(v, p, url, source);
        else if (TOKEN_KEY_RE.test(k) && looksToken(v)) post({type:'token', name:p, value:v, source:source, url:url});
      }
    } catch(e){}
  }
  function scanStorage(store, source){
    try {
      for (var i=0;i<store.length;i++){
        var k = store.key(i);
        if (TOKEN_KEY_RE.test(k)) {
          var v = store.getItem(k);
          if (looksToken(v)) post({type:'token', name:k, value:v, source:source, url:location.href});
        }
      }
    } catch(e){}
  }
  window.__cgScanStorage = function(){
    try { scanStorage(localStorage,'localStorage'); } catch(e){}
    try { scanStorage(sessionStorage,'sessionStorage'); } catch(e){}
  };
  if (window.fetch) {
    var ofetch = window.fetch;
    window.fetch = function(){
      try {
        var a = arguments, inp = a[0], init = a[1];
        var url = typeof inp === 'string' ? inp : (inp && inp.url) || '';
        var hs = init && init.headers;
        if (hs) {
          var getter = (typeof Headers !== 'undefined' && hs instanceof Headers)
            ? function(n){ try { return hs.get(n); } catch(e){ return null; } }
            : function(n){
                if (Array.isArray(hs)) { for (var i=0;i<hs.length;i++) if (String(hs[i][0]).toLowerCase()===n) return hs[i][1]; return null; }
                for (var k in hs) if (k.toLowerCase()===n) return hs[k];
                return null;
              };
          for (var i=0;i<TOKEN_HEADERS.length;i++){
            var v = getter(TOKEN_HEADERS[i]);
            if (v && looksToken(v)) post({type:'token', name:TOKEN_HEADERS[i], value:v, source:'request-header', url:url});
          }
        }
      } catch(e){}
      var r = ofetch.apply(this, arguments);
      try {
        r.then(function(res){
          try {
            res.clone().text().then(function(t){
              if (t.length > 4*1024*1024) return;
              try { var j = JSON.parse(t); scanObject(j,'',url,'response-body'); } catch(e){}
            });
          } catch(e){}
        });
      } catch(e){}
      return r;
    };
  }
  var xo = XMLHttpRequest.prototype.open, xs = XMLHttpRequest.prototype.setRequestHeader, xd = XMLHttpRequest.prototype.send;
  XMLHttpRequest.prototype.open = function(m,u){ this.__cgUrl = u; return xo.apply(this, arguments); };
  XMLHttpRequest.prototype.setRequestHeader = function(n,v){
    try { var ln = String(n).toLowerCase(); if (TOKEN_HEADERS.indexOf(ln)>=0 && looksToken(v)) post({type:'token', name:ln, value:v, source:'request-header', url:this.__cgUrl||''}); } catch(e){}
    return xs.apply(this, arguments);
  };
  XMLHttpRequest.prototype.send = function(){
    var self = this;
    try {
      self.addEventListener('load', function(){
        try {
          var t = self.responseText;
          if (t && t.length < 4*1024*1024) { var j = JSON.parse(t); scanObject(j,'',self.__cgUrl||self.responseURL||'','response-body'); }
        } catch(e){}
      });
    } catch(e){}
    return xd.apply(this, arguments);
  };
  window.addEventListener('storage', function(){ window.__cgScanStorage(); });
  document.addEventListener('DOMContentLoaded', function(){ window.__cgScanStorage(); });
  setInterval(function(){ window.__cgScanStorage(); }, 3000);
})();
)JS";
}
