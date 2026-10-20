// 通用工具：字符串转换、时间格式、原子文件写、剪贴板、路径解析
#pragma once
#include <string>
#include <vector>
#include <windows.h>

namespace util {

std::wstring Utf8ToWide(const std::string& s);
std::string WideToUtf8(const std::wstring& s);
std::string WideToAnsi(const std::wstring& s);  // 仅用于 MessageBox 等 ANSI 接口

// "2026.10.13 14:52"（本地时间）
std::wstring NowLogTime();
std::string NowLogTimeA();
// ISO8601 UTC "2026-10-19T15:30:05Z"
std::string NowIsoUtc();
// Cookie 的 double Unix 秒 -> ISO8601；isSession 或 <=0 返回 "session"
std::string CookieExpiresToIso(double expires, bool isSession);
// 读整个文件为字节（宽路径安全）
bool ReadFileBytes(const std::wstring& path, std::string& out);

// 模块（exe）所在目录，带结尾反斜杠
std::wstring ModuleDir();
// %LOCALAPPDATA%\CookieGrabber（确保存在）
std::wstring LocalAppDir();
// 递归删除目录（静默无界面）
void DeleteDirRecursive(const std::wstring& path);
// 目录是否存在
bool DirExists(const std::wstring& path);
// 解析日志目录：优先 exe 目录，不可写则回退 %LOCALAPPDATA%\CookieGrabber\logs
std::wstring ResolveLogDir();

// 原子写（临时文件 + MoveFileEx 替换），data 为 UTF-8 字节
bool WriteFileAtomic(const std::wstring& path, const std::string& data);
// 追加写（单次调用）
bool AppendFile(const std::wstring& path, const std::string& data);

bool SetClipboardText(const std::wstring& text, HWND owner);

std::wstring Trim(const std::wstring& s);
std::wstring ToLower(const std::wstring& s);
bool StartsWithI(const std::wstring& s, const std::wstring& prefix);

} // namespace util
