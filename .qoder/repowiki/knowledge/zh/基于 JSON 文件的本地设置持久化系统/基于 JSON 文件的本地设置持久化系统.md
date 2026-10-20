---
kind: configuration_system
name: 基于 JSON 文件的本地设置持久化系统
category: configuration_system
scope:
    - '**'
source_files:
    - src/settings.h
    - src/settings.cpp
    - src/util.h
    - src/util.cpp
    - src/json.h
---

## 1. 系统与方案

本项目没有使用外部配置框架（如 YAML/TOML/INI），而是自实现了一个轻量级的 **JSON 文件 + C++ 结构体** 的本地设置持久化系统。核心依赖是项目内嵌的 `json.h`（`mjson` 命名空间）提供的简易 JSON 解析与序列化能力，以及 `util.h` 中的文件 I/O 工具。

## 2. 关键文件

- `src/settings.h` — 定义 `Settings` 结构体、`LogFormat` 枚举、`Preset` 结构体及四个 API：`LoadSettings()` / `SaveSettings()` / `SettingsFilePath()` / `DefaultSettings()`。
- `src/settings.cpp` — 实现加载、保存、默认值生成与路径计算。
- `src/util.h` / `src/util.cpp` — 提供 `LocalAppDir()`（返回 `%LOCALAPPDATA%\CookieGrabber\`）、`ReadFileBytes()`、`WriteFileAtomic()`、`Utf8ToWide()` / `WideToUtf8()` 等基础工具。
- `src/json.h` — 内嵌的微型 JSON 库（`mjson`），用于解析和写入 `settings.json`。

## 3. 架构与约定

### 存储位置
配置文件固定位于 `%LOCALAPPDATA%\CookieGrabber\settings.json`，由 `SettingsFilePath()` 拼接 `util::LocalAppDir()` 与文件名 `kFileName = L"settings.json"` 得到。

### 数据结构
`Settings` 结构体承载所有用户可配置项：
- `autoCapture` — 检测到登录状态变化自动抓取
- `clipboard` — 抓取后复制到剪贴板
- `history` — 追加 `cookie_history.log`
- `firstOpenCapture` — 首次打开即抓取
- `logFormat` — `LogFormat::String | Json | Both`
- `presets` — `std::vector<Preset>`，每个 `Preset` 含 `name` 与 `url`

### 加载流程 (`LoadSettings`)
1. 先调用 `DefaultSettings()` 填充默认值（包含微博、知乎、B站、GitHub 四个预设）。
2. 尝试读取 `settings.json`；若文件不存在或读取失败，直接返回默认设置。
3. 用 `mjson::Parse` 解析为 `Value::Object`，逐项读取布尔字段与字符串字段。
4. `logFormat` 通过字符串映射回枚举（`"string"` / `"json"` / `"both"`）。
5. `presets` 数组中每个对象需同时包含 `name` 与 `url`，且 `url` 非空才会被加入列表。
6. 对缺失字段采用“默认值兜底”策略，保证旧版本/损坏配置不会导致崩溃。

### 保存流程 (`SaveSettings`)
1. 使用 `mjson::Writer` 以 pretty-print 模式输出。
2. 按固定键顺序写入各字段，`logFormat` 从枚举转回字符串。
3. `presets` 在写入前通过 `WideToUtf8` 转为 UTF-8。
4. 最终通过 `util::WriteFileAtomic` 原子写入，避免部分写入导致配置损坏。

### 编码约定
- 内部使用 `std::wstring`（UTF-16）存储中文名称与 URL。
- JSON 文件统一使用 UTF-8 编码，转换通过 `util::Utf8ToWide` / `util::WideToUtf8` 完成。

## 4. 约定与约束

- **文件位置固定**：配置路径由 `SettingsFilePath()` 硬编码为 `%LOCALAPPDATA%\CookieGrabber\settings.json`，无命令行参数或环境变量覆盖机制。
- **向后兼容**：`LoadSettings` 对未知字段忽略、对缺失字段使用默认值，新增字段时只需在结构体和读写逻辑中添加即可。
- **原子写入**：`SaveSettings` 通过 `WriteFileAtomic` 写入，确保进程异常退出时不会留下半写文件。
- **预设不可为空**：`presets` 数组中每个元素必须同时有 `name` 和 `url`，且 `url` 非空，否则该条目会被丢弃（见 `settings.cpp:40-44`）。
- **日志格式白名单**：仅接受 `"string"` / `"json"` / `"both"` 三种字符串值，其他值回退为 `LogFormat::String`。
- **无加密/鉴权**：`settings.json` 以明文形式存储在用户目录，不包含敏感信息（仅开关与站点预设）。
- **无运行时热重载**：当前代码未暴露监听文件变更并重新加载的机制，修改配置后需重启应用生效。