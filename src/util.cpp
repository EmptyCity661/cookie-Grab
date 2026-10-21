#include "util.h"
#include <shlobj.h>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace util {

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

std::string WideToAnsi(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_ACP, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_ACP, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

static void LocalTimeFields(SYSTEMTIME& st) {
    GetLocalTime(&st);
}

std::wstring NowLogTime() {
    SYSTEMTIME st; LocalTimeFields(st);
    wchar_t b[32];
    swprintf(b, 32, L"%04d.%02d.%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    return b;
}

std::string NowLogTimeA() {
    SYSTEMTIME st; LocalTimeFields(st);
    char b[32];
    snprintf(b, sizeof(b), "%04d.%02d.%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    return b;
}

std::string NowIsoUtc() {
    SYSTEMTIME st; GetSystemTime(&st);
    char b[40];
    snprintf(b, sizeof(b), "%04d-%02d-%02dT%02d:%02d:%02dZ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return b;
}

std::string CookieExpiresToIso(double expires, bool isSession) {
    if (isSession || expires <= 0) return "session";
    time_t t = (time_t)expires;
    tm* gp = gmtime(&t);
    if (!gp) return "session";
    tm g = *gp;
    char b[40];
    snprintf(b, sizeof(b), "%04d-%02d-%02dT%02d:%02d:%02dZ",
             g.tm_year + 1900, g.tm_mon + 1, g.tm_mday, g.tm_hour, g.tm_min, g.tm_sec);
    return b;
}

bool ReadFileBytes(const std::wstring& path, std::string& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD sz = GetFileSize(h, nullptr);
    if (sz == INVALID_FILE_SIZE || sz > 64u * 1024 * 1024) { CloseHandle(h); return false; }
    out.resize((size_t)sz);
    DWORD rd = 0;
    BOOL ok = ReadFile(h, &out[0], sz, &rd, nullptr);
    CloseHandle(h);
    return ok && rd == sz;
}

std::wstring ModuleDir() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) *(slash + 1) = L'\0';
    return path;
}

bool DirExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

void DeleteDirRecursive(const std::wstring& path) {
    if (path.empty()) return;
    // SHFileOperation 需要双 null 结尾的路径
    std::vector<wchar_t> buf(path.begin(), path.end());
    buf.push_back(L'\0');
    buf.push_back(L'\0');
    SHFILEOPSTRUCTW op = {};
    op.wFunc = FO_DELETE;
    op.pFrom = buf.data();
    op.fFlags = FOF_NO_UI | FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI;
    SHFileOperationW(&op);
}

std::wstring LocalAppDir() {
    PWSTR p = nullptr;
    std::wstring dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p)) && p) {
        dir = p;
        CoTaskMemFree(p);
    }
    if (dir.empty()) dir = L"C:\\Users\\Public";
    dir += L"\\cookie抓取工具3.0";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

std::wstring ResolveLogDir() {
    std::wstring dir = ModuleDir();
    // 探测可写：尝试创建目录并写一个小文件
    CreateDirectoryW(dir.c_str(), nullptr);
    std::wstring probe = dir + L".cgwrite.tmp";
    HANDLE h = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
        DeleteFileW(probe.c_str());
        return dir;
    }
    return LocalAppDir() + L"\\logs";
}

bool WriteFileAtomic(const std::wstring& path, const std::string& data) {
    std::wstring tmp = path + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = WriteFile(h, data.data(), (DWORD)data.size(), &written, nullptr);
    if (ok) FlushFileBuffers(h);
    CloseHandle(h);
    if (!ok || written != data.size()) { DeleteFileW(tmp.c_str()); return false; }
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}

bool AppendFile(const std::wstring& path, const std::string& data) {
    HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = WriteFile(h, data.data(), (DWORD)data.size(), &written, nullptr);
    CloseHandle(h);
    return ok && written == data.size();
}

bool SetClipboardText(const std::wstring& text, HWND owner) {
    if (!OpenClipboard(owner)) return false;
    EmptyClipboard();
    bool ok = false;
    int bytes = (int)((text.size() + 1) * sizeof(wchar_t));
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (h) {
        void* p = GlobalLock(h);
        if (p) {
            memcpy(p, text.c_str(), bytes);
            GlobalUnlock(h);
            ok = SetClipboardData(CF_UNICODETEXT, h) != nullptr;
        }
        if (!ok) GlobalFree(h);
    }
    CloseClipboard();
    return ok;
}

std::wstring Trim(const std::wstring& s) {
    size_t a = 0, b = s.size();
    while (a < b && iswspace(s[a])) a++;
    while (b > a && iswspace(s[b - 1])) b--;
    return s.substr(a, b - a);
}

std::wstring ToLower(const std::wstring& s) {
    std::wstring r = s;
    for (auto& c : r) c = towlower(c);
    return r;
}

bool StartsWithI(const std::wstring& s, const std::wstring& prefix) {
    if (s.size() < prefix.size()) return false;
    return ToLower(s.substr(0, prefix.size())) == ToLower(prefix);
}

} // namespace util
