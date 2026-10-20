---
kind: dependency_management
name: 依赖管理：无包管理器，静态链接 Win32/WebView2 的裸 C++ 工程
category: dependency_management
scope:
    - '**'
source_files:
    - build.bat
    - .gitignore
---

## 1. 使用的系统/方法

本项目是一个纯 C++ (C++17) Win32 + WebView2 桌面应用，**没有使用任何第三方包管理器或依赖声明文件**。仓库根目录不存在 `package.json`、`go.mod`、`Cargo.toml`、`CMakeLists.txt`、`vcpkg.json`、`conanfile.*`、`.nuget.config`、`*.csproj` 等任何依赖清单。构建完全通过根目录的 `build.bat` 调用 MinGW-w64 工具链完成。

- 编译器与工具链：MinGW-w64 GCC（由 `winget install BrechtSanders.WinLibs.POSIX.UCRT` 安装），路径硬编码在 `build.bat` 第 10 行。
- 运行时依赖：Microsoft Edge WebView2 Runtime（由操作系统提供）+ 随程序分发的 `WebView2Loader.dll`。
- 系统库：通过 `-lole32 -loleaut32 -luuid -lgdi32 -lcomctl32 -lshell32 -lcomdlg32 -ladvapi32 -ldwmapi` 直接链接 Windows SDK 提供的 DLL。
- 标准库：通过 `-static-libgcc -static-libstdc++` 静态链接 MinGW 的 libgcc/libstdc++。

## 2. 关键文件

- `build.bat` — 唯一构建入口，负责 windres 资源编译、g++ 链接、以及把 `include\WebView2Loader.dll` 复制到 `res\` 后打包进最终 exe。
- `.gitignore` — 排除 `CookieGrabber.exe`、`res/app.res`、`*.log`、`logs/`、`backup-20261020/`、`.codebuddy/`（仅本机插件开关）、编辑器临时文件（`*.swp`、`*~`、`.vscode/`）；**未**忽略 `include/WebView2Loader.dll` 与 `res/WebView2Loader.dll`，说明 WebView2Loader.dll 作为二进制依赖被纳入版本控制；`.qoder/repowiki/` 的中文 Wiki 与知识卡片同样随仓库分发。
- `include/WebView2.h` — WebView2 头文件（NuGet 包 `Microsoft.Web.WebView2` 中同名文件的副本），作为本地 vendored 的头文件供编译期引用，`build.bat` 用 `-Iinclude` 指向它。
- `res/app.rc` / `res/app.manifest` — Windows 资源与清单，其中 `app.rc` 以 `RCDATA` 自定义资源（ID 101）嵌入 `WebView2Loader.dll`，运行时由 `webview.cpp` 释放到临时目录。
- `make_icon.py` — 图标生成脚本，仅用 Python 标准库（`struct`/`math`），不引入 Pillow 等第三方包。

## 3. 架构与约定

- **Vendoring 策略**：WebView2 的头文件和 `WebView2Loader.dll` 以“源码树内附带”的方式分发，不通过 NuGet、vcpkg 或任何远程包源获取。`build.bat` 第 23 行 `copy /y include\WebView2Loader.dll res\WebView2Loader.dll >nul` 在每次构建时复制 loader dll 到资源目录，再由 windres 打包进 exe。
- **无锁文件**：仓库中没有 `*.lock`、`*.sum`、`*.json.lock` 等锁定文件，因为不存在需要版本锁定的依赖图。
- **版本来源**：依赖版本由开发者手动维护——头文件来自 WebView2 SDK，loader dll 来自 WebView2 安装包，编译器版本由 `winget` 包名 `BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64` 固定。
- **静态链接**：除 Windows 系统 API（ole32、gdi32、shell32 等）外，所有 C/C++ 运行时均静态链接，目标是生成单个 `CookieGrabber.exe` 可执行文件（见 `build.bat` 注释“single file, WebView2Loader.dll embedded”）。

## 4. 约定与约束

- 新增 C++ 源文件需同时添加到 `build.bat` 第 28 行 g++ 命令的参数列表，否则不会被编译（当前列表：`main.cpp`、`app.cpp`、`theme.cpp`、`ui.cpp`、`webview.cpp`、`capture.cpp`、`logwriter.cpp`、`settings.cpp`、`util.cpp`）。
- 单文件分发目标决定了不能引入需要额外运行时 DLL 的第三方库；只允许静态链接 MinGW 运行时 + 系统 API。
- WebView2 相关头文件应放在 `include/`，loader dll 应放在 `include/` 与 `res/` 两个位置（构建脚本会复制一份到 `res/`）。
- 构建环境要求：用户必须先运行 `winget install BrechtSanders.WinLibs.POSIX.UCRT` 安装 MinGW-w64，且 `LOCALAPPDATA\Microsoft\WinGet\...\mingw64\bin\g++.exe` 必须存在，否则 `build.bat` 在第 15-19 行报错退出。
- 项目未引入任何第三方 C++ 库（如 nlohmann/json、fmt、spdlog 等），仅依赖 Windows SDK 与 WebView2 Runtime。
- 由于没有包管理器，不存在“升级依赖”的自动化流程；更新 WebView2 头文件或 loader dll 需要手动替换并重新提交。