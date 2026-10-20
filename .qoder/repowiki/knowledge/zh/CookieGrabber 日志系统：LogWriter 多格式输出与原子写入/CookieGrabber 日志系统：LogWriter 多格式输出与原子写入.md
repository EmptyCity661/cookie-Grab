---
kind: logging_system
name: CookieGrabber 日志系统：LogWriter 多格式输出与原子写入
category: logging_system
scope:
    - '**'
source_files:
    - src/logwriter.h
    - src/logwriter.cpp
    - src/settings.h
    - src/util.h
---

## 1. 使用的系统/方案

本项目没有引入第三方日志框架，而是自实现了一个轻量级的 `LogWriter` 类（`src/logwriter.{h,cpp}`），配合 `util` 模块的文件 I/O 工具函数完成日志落盘。日志输出分为两类：
- **结构化 JSON**：`cookie.json`（扁平简洁型）、`cookie_detail.json`（结构化明细型）。
- **人类可读文本**：`cookie.log`（两行：时间 + Cookie 串）、`cookie_history.log`（追加式历史摘要）。

所有文件均使用 UTF-8 无 BOM 编码，通过 `util::WideToUtf8` 进行宽字符转码。

## 2. 关键文件

| 文件 | 作用 |
|---|---|
| `src/logwriter.h` | `LogWriter` 类、`CookieItem` / `TokenItem` 数据结构定义；头注释声明了文件命名、编码、目录回退等规则 |
| `src/logwriter.cpp` | 具体写入逻辑：按 `mask` 位掩码控制输出哪些文件，使用 `std::mutex` 做进程内互斥 |
| `src/settings.h` | `LogFormat` 枚举（`String` / `Json` / `Both`）及 `Settings::logFormat`、`Settings::history` 字段 |
| `src/util.h` | `ResolveLogDir()`、`WriteFileAtomic()`、`AppendFile()`、`NowLogTimeA()` 等底层工具函数声明 |
| `backup-20261020/src/logwriter.{h,cpp}` | 早期备份版本，结构与当前一致，可作为演进参考 |

## 3. 架构与约定

### 3.1 日志目录解析
`LogWriter::Init()` 调用 `util::ResolveLogDir()` 确定日志目录，策略为：**优先 exe 所在目录，不可写时回退到 `%LOCALAPPDATA%\CookieGrabber\logs`**。该行为由头文件注释明确描述。

### 3.2 输出文件与格式
`LogWriter::Write()` 根据 `mask` 参数和 `Settings::logFormat` 决定输出哪些文件：

- `mask & 1`：Cookie 类文件（`cookie.log`、`cookie.json`、`cookie_detail.json`）
- `mask & 2`：Token 历史（仅写入 `cookie_history.log` 的 token 部分）
- `mask = 3`：两者都写（默认值）

各文件格式约定：

| 文件 | 格式 | 说明 |
|---|---|---|
| `cookie.log` | 两行文本 | 第1行 ISO8601 UTC 时间，第2行 `name=value; name=value` 形式的 Cookie 串 |
| `cookie.json` | 扁平 JSON | `{ time, domain, url, cookie, token }`，token 单值为字符串，多值时为对象 |
| `cookie_detail.json` | 结构化 JSON | `{ time, domain, url, cookies[], tokens[] }`，每个 cookie/token 均为完整对象 |
| `cookie_history.log` | 追加文本 | `[time] domain=... url=... cookies=N tokens=M` 摘要行 + Cookie/Token 串行 |

JSON 字段类型约定：
- `sameSite` 在 `cookie_detail.json` 中映射为字符串：`0→"None"`、`1→"Lax"`、`2→"Strict"`（对应 `COREWEBVIEW2_COOKIE_SAME_SITE_KIND`）。
- `expires` 经 `util::CookieExpiresToIso()` 转为 ISO8601 字符串。
- `capturedAt` 为 `TokenItem.capturedAt`，类型为 `std::string`，注释标注为 ISO8601 UTC。

### 3.3 线程安全
`LogWriter` 内部持有 `std::mutex mu_`，`Write()` 以 `std::lock_guard<std::mutex>` 加锁，保证同一进程内并发写入不会交错。这是进程内互斥，不涉及跨进程同步。

### 3.4 写入策略
- 覆盖型文件（`cookie.log`、`cookie.json`、`cookie_detail.json`）使用 `util::WriteFileAtomic()`，通过「临时文件 + 原子替换」避免部分写入。
- 历史文件（`cookie_history.log`）使用 `util::AppendFile()` 追加写入。
- 所有 JSON 输出末尾追加换行符（`w.out += "\n"`）。

### 3.5 错误处理
`Write()` 返回 `bool`，失败时通过引用参数 `errOut` 累积错误信息（多个失败用 `; ` 分隔）。单个文件失败不会中断其他文件的写入，最终返回值表示「全部成功」。

## 4. 约定与约束

以下规则来自代码或头文件注释的直接表述：

- **编码**：所有日志文件使用 UTF-8 无 BOM（`src/logwriter.h` 头注释）。
- **文件名**：固定为 `cookie.log` / `cookie.json` / `cookie_detail.json` / `cookie_history.log`（`src/logwriter.h` 头注释）。
- **原子写入**：覆盖型日志文件必须通过「临时文件 + 原子替换」写入（`src/logwriter.h` 头注释）。
- **追加模式**：`cookie_history.log` 使用追加模式（`src/logwriter.h` 头注释）。
- **目录优先级**：日志目录优先 exe 目录，不可写时回退到 `%LOCALAPPDATA%\CookieGrabber\logs`（`src/logwriter.h` 头注释）。
- **进程内互斥**：日志写入使用进程内互斥（`src/logwriter.h` 头注释）。
- **日志格式选择**：通过 `Settings::logFormat`（`LogFormat::String` / `Json` / `Both`）控制输出格式，`cookie.log` 仅在 `String` 或 `Both` 时写入，JSON 文件仅在 `Json` 或 `Both` 时写入（`src/logwriter.cpp` 条件分支）。
- **历史记录开关**：`cookie_history.log` 仅在 `Settings::history == true` 时写入（`src/logwriter.cpp` L126）。
- **Cookie 串构造**：`CookieString()` / `TokenString()` 以 `name=value; name=value` 形式拼接，空集合返回空字符串（`src/logwriter.cpp` L9-L28）。

未观察到全局日志级别管理、日志轮转、外部 sink（如 syslog、文件滚动）等机制——这是一个面向单次抓取结果输出的轻量日志子系统。