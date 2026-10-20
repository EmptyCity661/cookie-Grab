// 入口：支持命令行传入初始网址（可选）
#include "app.h"
#include <shellapi.h>
#include <string>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow) {
    std::wstring initial;
    if (lpCmdLine && *lpCmdLine) {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv && argc > 1) initial = argv[1];
        if (argv) LocalFree(argv);
    }
    if (!App::Get().Init(hInstance, initial)) {
        MessageBoxW(nullptr, L"程序初始化失败（WebView2 运行时可能未安装）", L"Cookie 抓取工具", MB_ICONERROR);
        return 1;
    }
    return App::Get().Run();
}
