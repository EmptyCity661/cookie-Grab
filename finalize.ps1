# 构建辅助脚本（由 build.bat 调用）：处理中文资源编码与最终文件名。
#   -Stage rc  ：把 UTF-8 的 res/app.rc 转成 GBK，供 windres 按系统 ANSI 正确解析中文
#                （windres 默认按本机代码页 GBK 读 .rc，直接读 UTF-8 会乱码）。
#   -Stage exe ：把中间产物 CG.exe 重命名为最终中文名 cookie抓取工具3.0.exe。
# 注意：
#   * g++ 的 -o 无法可靠创建中文名文件（UTF-8 字节按 GBK 建文件会乱码），故用 ASCII 中间名 + 本脚本重命名。
#   * 目标名用 Unicode 码点构造（下方码点即 "cookie抓取工具3.0.exe"），避免 .ps1 文件本身编码/Rename-Item 引起乱码。
#   * Rename-Item 勿加 -Force（对中文目标名会报"拒绝访问"）。
param([ValidateSet('rc', 'exe')][string]$Stage)
$root = $PSScriptRoot
$ErrorActionPreference = 'Stop'

if ($Stage -eq 'rc') {
    $gbk = [Text.Encoding]::GetEncoding(936)
    $text = [IO.File]::ReadAllText((Join-Path $root 'res\app.rc'), [Text.Encoding]::UTF8)
    [IO.File]::WriteAllText((Join-Path $root 'res\_app_gbk.rc'), $text, $gbk)
}
elseif ($Stage -eq 'exe') {
    # "cookie抓取工具3.0.exe" 的 Unicode 码点（编码无关）
    $dst = -join ([char[]]@(0x63,0x6F,0x6F,0x6B,0x69,0x65, 0x6293,0x53D6,0x5DE5,0x5177, 0x33,0x2E,0x30, 0x2E,0x65,0x78,0x65))
    $dstPath = Join-Path $root $dst
    if (Test-Path -LiteralPath $dstPath) { Remove-Item -LiteralPath $dstPath -Force }
    Rename-Item -LiteralPath (Join-Path $root 'CG.exe') -NewName $dst
    $f = Get-Item -LiteralPath $dstPath
    Write-Output ('output size ' + $f.Length + ' bytes')
    Write-Output ('build complete: ' + $f.FullName)
}
