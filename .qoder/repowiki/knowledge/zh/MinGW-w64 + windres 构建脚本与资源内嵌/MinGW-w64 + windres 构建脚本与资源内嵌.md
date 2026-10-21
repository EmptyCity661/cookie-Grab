---
kind: build_system
name: MinGW-w64 + windres 构建脚本与资源内嵌
category: build_system
scope:
    - '**'
source_files:
    - build.bat
    - res/app.rc
    - res/app.manifest
    - make_icon.py
---

## 1. 构建系统概览

本项目使用 **MinGW-w64 (GCC)** 作为 C++ 编译器，通过根目录的 `build.bat` 批处理脚本完成编译、资源编译与链接。没有 Makefile、CMake、Dockerfile 或 CI 配置；也没有跨平台/交叉编译目标。构建产物为单个 `cookie抓取工具3.0.exe`（x64），WebView2Loader.dll 被内嵌在资源中并在运行时释放。

## 2. 关键文件

- `build.bat` — 唯一入口的构建脚本，分三步：windres 编译资源 → g++ 编译并链接所有源文件 → 输出文件大小统计。
- `res/app.rc` — Windows 资源脚本，声明图标 (`app.ico`)、清单 (`app.manifest`) 和内嵌的 `WebView2Loader.dll`（RCDATA 资源 ID 101）。
- `res/app.manifest` — 应用清单，启用 Common Controls v6、PerMonitorV2 DPI 感知、UTF-8 代码页。
- `make_icon.py` — 纯 Python 生成 32×32 32bpp ICO 的工具（硬编码路径指向 `D:\itemi\cookie\res\app.ico`，仅用于生成图标）。
- `backup-20261020/build.bat` 与 `backup-20261020/make_icon.py` — 历史备份副本，内容与当前一致。

## 3. 构建流程与约定

### 3.1 MinGW 工具链定位

`build.bat` 从固定路径加载 MinGW-w64：
```
%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64
```
如果 `g++` 不在 PATH，脚本会打印安装提示：`winget install BrechtSanders.WinLibs.POSIX.UCRT`。

### 3.2 资源编译（windres）

```bat
windres --include-dir res --include-dir src res\app.rc -O coff -o res\app.res
```
- 包含目录：`res/` 和 `src/`（使 `#include "ids.h"` 能解析）。
- 输出：`res/app.res`（COFF 格式）。
- 构建前自动把 `include/WebView2Loader.dll` 复制到 `res/WebView2Loader.dll`，以便资源脚本引用。

### 3.3 编译与链接（g++）

```bat
g++ -std=c++17 -O2 -municode -mwindows \
    -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 \
    -Wall \
    -o cookie抓取工具3.0.exe \
    src/main.cpp src/app.cpp src/theme.cpp src/shell.cpp \
    src/webview.cpp src/capture.cpp src/logwriter.cpp \
    src/settings.cpp src/util.cpp res/app.res \
    -Iinclude -Isrc \
    -lole32 -loleaut32 -luuid -lgdi32 -lcomctl32 -lshell32 \
    -lcomdlg32 -ladvapi32 -ldwmapi \
    -static -static-libgcc -static-libstdc++
```

关键编译选项含义：
- `-std=c++17`：语言标准。
- `-municode -mwindows`：Unicode 字符集 + Win32 GUI 子系统（无控制台窗口）。
- `-D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00`：目标 Windows 10 API。
- `-Wall`：开启全部警告。
- `-static -static-libgcc -static-libstdc++`：静态链接 GCC/STL 运行时，配合内嵌 WebView2Loader.dll 实现单文件分发。
- 显式链接的 Win32 库：ole32、oleaut32、uuid、gdi32、comctl32、shell32、comdlg32、advapi32、dwmapi。

### 3.4 已知问题与注释

脚本头注释明确记录了一个无害的 ld 警告：`.rsrc merge failure: multiple non-default manifests`——ld 丢弃 CRT 的空占位清单而保留项目自己的清单，不影响最终产物。

### 3.5 图标生成

`make_icon.py` 是独立工具，用纯 Python（struct + math）按像素绘制一个饼干形状的 32×32 ICO，写入 `res/app.ico`。它不是构建系统的组成部分（构建脚本不调用它），且硬编码了绝对路径 `D:\itemi\cookie\res\app.ico`。

## 4. 约束与规则

- **仅支持 x64 Windows**：manifest 中 `processorArchitecture="amd64"`，编译使用 `-municode`（默认 x64），未提供其他架构目标。
- **依赖 MinGW-w64 via WinGet**：`build.bat` 要求 `BrechtSanders.WinLibs.POSIX.UCRT` 包已安装，否则直接退出。
- **源码文件集合固定**：`build.bat` 显式列出所有 `.cpp` 源文件，新增源文件需手动编辑脚本，没有自动扫描机制。
- **WebView2Loader.dll 必须存在**：构建前会从 `include/WebView2Loader.dll` 复制到 `res/`，缺失会导致 windres 失败。
- **无版本化/发布流水线**：没有版本号管理、签名、安装包打包或 CI 步骤；`build.bat` 只产出 `cookie抓取工具3.0.exe`。
- **无 Docker/容器化**：仓库中没有 Dockerfile 或 docker-compose 配置。
- **无 Make/CMake/Ninja**：整个仓库不存在这些构建系统文件。

## 5. 结论

这是一个极简的本地构建方案：一个 `build.bat` 驱动 windres + g++，将 Win32 源文件、Windows 资源（图标、清单、WebView2Loader.dll）静态链接成单个可执行文件。构建逻辑集中在脚本中而非外部构建系统中，适合单人开发场景。