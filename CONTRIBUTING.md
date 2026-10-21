# 贡献指南

先说声谢谢，有人愿意来一起折腾这个小工具挺好的。提 PR 就代表你同意按 Apache-2.0（版权 © 空.城）并入。

## 交流
有话好好说，对事不对人。不管你水平咋样都欢迎。

## 报 Bug / 提需求
- 报 Bug 尽量带上：Windows 版本、装没装 WebView2 Runtime、复现步骤、期望和实际的差别、相关日志。日志里注意**脱敏**，真实的 Cookie / Token 千万别贴。
- 想要新功能的话，说下使用场景和你图个啥。

> 这个项目是用来抓你自己账号的登录态的，所以在 Issue / PR / 日志里不要泄露任何真实的 Cookie、Token 或者站点凭据。

## 开发环境
- Windows 10 及以上（x64）
- MinGW-w64（g++）：`winget install BrechtSanders.WinLibs.POSIX.UCRT`
- 系统装好 Microsoft Edge WebView2 Runtime
- 可选：Python 3（生成图标、跑本地测试服务器用）

## 构建
```powershell
cd d:\itemi\cookie
.\build.bat        # 产出 cookie抓取工具3.0.exe
```

## 代码规范
- **架构边界**：界面一律走 WebView2 玻璃外壳（`src/shell.*` 用 HTML/CSS 画），别重新引入原生 GDI 自绘控件；被访问站点由独立的 Content WebView2 承载，两边靠 `postMessage` / `ExecuteScript` 通信。**Cookie / Token 值别进外壳的 DOM、`state` 或页面日志。**
- **配色和尺寸**：颜色统一取 `theme::Current()` 的语义色，别写裸 `RGB()`；原生绘制尺寸按 96dpi 逻辑像素写，再用 `App::S()` 缩放。
- **错误处理**：同步 API 返回 `bool`，WebView2 异步操作把 `HRESULT` 透传；运行期错误统一写状态栏，弹窗能省就省。
- **加新文件**：`build.bat` 把全部 `.cpp` 列出来了，新文件必须同时加进 g++ 那行参数，不然编不进去。
- **注释**：文件头用一行 `// 模块：职责` 说明；关键逻辑就近写注释，改了代码顺手更新注释。

## 验证
- 提交前跑一遍 `.\build.bat`，确认编得过、产物正常。
- 动了抓取相关逻辑，用本地测试页做端到端验证：

```powershell
python test\serve.py     # 监听 http://127.0.0.1:8899
# 在应用地址栏打开这个地址，点页面上的「登 录」，看 Cookie/Token 三条链路通不通
```

## 提交信息
写清楚你干了啥，建议用这些前缀：`feat:` / `fix:` / `docs:` / `refactor:` / `chore:` / `test:`，顺便交代下动机和影响面。比如：

```
feat: 支持自定义 Token 抓取正则
- 设置页新增正则输入，抓取时按正则过滤
- 同步更新 README 与设置持久化知识卡片
```

## 提 PR 的流程
1. Fork 仓库，从 `master` 拉一个特性分支。
2. 提交保持聚焦、信息清楚；必要的话拆成几个提交。
3. 确认构建通过、没有调试残留、没有夹带无关文件。
4. 改到的文档（README、`.qoder/repowiki/knowledge` 知识卡片）一起更新。
5. 提 PR，描述清楚你为什么改、怎么实现的、怎么测的。

## 文档同步
动了外壳 / 主题 / 抓取逻辑，记得同步更新 README 和 `.qoder/repowiki/knowledge` 里对应的知识卡片；新增或调整 `settings.json` 字段时，同步更新 README 的"配置说明"。
