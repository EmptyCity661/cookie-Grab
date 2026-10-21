# 构建辅助脚本（由 build.bat 调用）：处理中文资源编码与最终文件名。
#   -Stage rc  ：把 UTF-8 的 res/app.rc 转成 GBK，供 windres 按系统 ANSI 正确解析中文
#                （windres 默认按本机代码页 GBK 读 .rc，直接读 UTF-8 会乱码）。
#   -Stage exe ：把中间产物 CG.exe 重命名为最终中文名 cookie抓取工具3.0.exe
#                （PowerShell 走 UTF-16 API，文件名正确；g++ 的 -o 建中文名会乱码；
#                 注意 Rename-Item 勿加 -Force，否则对中文目标名报"拒绝访问"）。
param([ValidateSet('rc', 'exe')][string]$Stage)
$root = $PSScriptRoot
$ErrorActionPreference = 'Stop'

if ($Stage -eq 'rc') {
    $gbk = [Text.Encoding]::GetEncoding(936)
    $text = [IO.File]::ReadAllText((Join-Path $root 'res\app.rc'), [Text.Encoding]::UTF8)
    [IO.File]::WriteAllText((Join-Path $root 'res\_app_gbk.rc'), $text, $gbk)
}
elseif ($Stage -eq 'exe') {
    $dst = 'cookie抓取工具3.0.exe'
    $dstPath = Join-Path $root $dst
    if (Test-Path -LiteralPath $dstPath) { Remove-Item -LiteralPath $dstPath -Force }
    Rename-Item -LiteralPath (Join-Path $root 'CG.exe') -NewName $dst
    $f = Get-Item -LiteralPath $dstPath
    Write-Output ('output size ' + $f.Length + ' bytes')
    Write-Output ('build complete: ' + $f.FullName)
}
