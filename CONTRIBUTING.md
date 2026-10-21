# 贡献指南

感谢你愿意为 **cookie抓取工具3.0** 贡献力量！本指南说明如何参与开发与提交贡献。提交 Pull Request 或 Contribution 即表示你同意其以 [Apache-2.0](LICENSE) 许可证并入本项目。

## 行为准则

请保持尊重、友善、建设性的交流。对事不对人，欢迎不同经验水平的参与者。

## 提交 Issue

- 报告 Bug 时，请尽量附上：Windows 版本、是否安装 WebView2 Runtime、复现步骤、期望与实际行为、相关日志（注意**脱敏**，切勿粘贴真实 Cookie / Token）。
- 提出新功能时，请说明使用场景与预期收益。

> 安全提醒：本项目用于抓取用户自有账号的登录态。请勿在 Issue / PR / 日志中泄露任何真实的 Cookie、Token 或站点凭据。

## 开发环境

- Windows 10 及以上（x64）
- MinGW-w64（g++）：`winget install BrechtSanders.WinLibs.POSIX.UCRT`
- 系统已安装 Microsoft Edge WebView2 Runtime
- 可选：Python 3（用于生成图标、运行本地测试服务器）

## 构建

```powershell
cd d:\itemi\cookie
.\build.bat        # 产出 cookie抓取工具3.0.exe
```

## 代码规范

- **架构边界**：界面一律走 WebView2 玻璃外壳（`src/shell.*` 以 HTML/CSS 渲染），不要重新引入原生 GDI 自绘控件；被访问站点由独立的 Content WebView2 承载，二者经 `postMessage` / `ExecuteScript` 通信。**Cookie / Token 值绝不进入外壳 DOM、`state` 或页面日志**。
- **配色与尺寸**：颜色统一取 `theme::Current()` 语义色，禁止裸 `RGB()` 字面量；原生绘制尺寸以 96dpi 逻辑像素书写并经 `App::S()` 缩放。
- **错误处理**：同步 API 返回 `bool`，WebView2 异步操作用 `HRESULT` 透传；运行期错误统一写到状态行，慎用弹窗。
- **新增源文件**：`build.bat` 显式列出全部 `.cpp`，新文件必须同时加入 g++ 参数行，否则不参与编译。
- **注释**：文件头用一行 `// 模块：职责` 说明；关键逻辑就近注释，注释随代码同步更新。

## 验证

- 提交前运行 `.\build.bat` 确认可编译、产物正常。
- 抓取相关改动，用本地测试页做端到端验证：

```powershell
python test\serve.py     # 监听 http://127.0.0.1:8899
# 在应用地址栏打开该地址并点击页面上的「登 录」，验证 Cookie/Token 三条链路
```

## 提交信息

使用清晰的行为描述，建议采用以下前缀之一：`feat:` / `fix:` / `docs:` / `refactor:` / `chore:` / `test:`，并说明动机与影响范围。示例：

```
feat: 支持自定义 Token 抓取正则
- 设置页新增正则输入，抓取时按正则过滤
- 同步更新 README 与设置持久化知识卡片
```

## Pull Request 流程

1. Fork 仓库并基于 `master` 创建特性分支。
2. 保持提交聚焦、信息清晰；必要时拆分为多个提交。
3. 确保构建通过、无调试残留、无关文件未提交。
4. 更新受影响的文档（README、`.qoder/repowiki/knowledge` 知识卡片）。
5. 提交 PR 并描述改动动机、实现方式与测试情况。

## 文档同步

改动外壳 / 主题 / 抓取逻辑时，请同步更新 README 与 `.qoder/repowiki/knowledge` 下对应的知识卡片；新增或调整 `settings.json` 字段时，同步更新 README 的「配置说明」。
