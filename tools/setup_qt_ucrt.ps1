<#
.SYNOPSIS
    为 termchart 的 Qt GUI 后端安装「与构建工具链同 CRT（UCRT）」的 Qt。

.DESCRIPTION
    termchart 核心以 GCC 15.2（纯 UCRT）构建。若链接 Qt.io 提供的 mingw_64（msvcrt）Qt，
    会混用两个 C 运行时 → 跨堆堆损坏崩溃。本脚本通过 MSYS2 安装 UCRT 版 Qt6
    （mingw-w64-ucrt-x86_64-qt6-base），与 GCC 15.2 对齐。

    脚本同时把国内镜像（阿里云/中科大）提到 mingw 镜像列表最前，规避 MSYS2 主镜像
    对新包的 403 / 超时。

    安装后请在构建/运行环境里设置（详见 docs/guide/gui_qt.md）：
      $env:QTDIR = 'C:\msys64\ucrt64'
      $env:PATH  = 'C:\msys64\ucrt64\bin;' + $env:PATH      # Qt bin 必须先于其它 MinGW
      $env:QT_QPA_PLATFORM_PLUGIN_PATH = 'C:\msys64\ucrt64\share\qt6\plugins\platforms'

    说明：Qt 二进制不进版本库（LGPL 动态链接 + 体积大）。本脚本即「可复现地获取二进制」的
    单一真源——钉死 MSYS2 包版本，重新跑一遍即可得到等价的 Qt。
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$msys2Root = 'C:\msys64'
$bash = Join-Path $msys2Root 'usr\bin\bash.exe'
if (-not (Test-Path $bash)) {
    Write-Error "未找到 MSYS2（$bash）。请先安装：https://www.msys2.org/"
}

# 1) 把国内镜像提到 mingw 镜像列表最前（仅当尚未置顶时），规避主镜像对新包的 403/超时。
$mirrorlist = Join-Path $msys2Root 'etc\pacman.d\mirrorlist.mingw'
if (Test-Path $mirrorlist) {
    $lines = Get-Content -Path $mirrorlist
    $aliyun = 'Server = https://mirrors.aliyun.com/msys2/mingw/$repo/'
    $hasAliyun = $lines | Where-Object { $_ -match 'mirrors\.aliyun\.com/msys2' }
    if (-not $hasAliyun) {
        Set-Content -Encoding ascii -Path $mirrorlist -Value (
            $aliyun + [Environment]::NewLine + ($lines -join [Environment]::NewLine)
        )
        Write-Host "已将阿里云镜像置顶：$mirrorlist"
    } else {
        Write-Host "阿里云镜像已存在，跳过。"
    }
} else {
    Write-Warning "未找到 $mirrorlist，跳过镜像调整（若主镜像 403/超时请手动处理）。"
}

# 2) 刷新数据库并安装 UCRT Qt6 base（bash -lc 单引号包裹，避免 PowerShell 展开）。
Write-Host "正在通过 MSYS2 安装 mingw-w64-ucrt-x86_64-qt6-base ..."
& $bash -lc 'pacman -Sy --noconfirm && pacman -S --noconfirm mingw-w64-ucrt-x86_64-qt6-base'
if ($LASTEXITCODE -ne 0) {
    Write-Error "pacman 安装失败（退出码 $LASTEXITCODE）。请检查网络/镜像后重试。"
}

# 3) 校验。
$qtCore = Join-Path $msys2Root 'ucrt64\bin\Qt6Core.dll'
if (-not (Test-Path $qtCore)) {
    Write-Error "Qt6Core.dll 未出现在 $qtCore，安装似乎失败。"
}

Write-Host ""
Write-Host "UCRT Qt 安装完成。构建/运行 Qt GUI 前请设置："
Write-Host "  `$env:QTDIR = 'C:\msys64\ucrt64'"
Write-Host "  `$env:PATH  = 'C:\msys64\ucrt64\bin;' + `$env:PATH"
Write-Host "  `$env:QT_QPA_PLATFORM_PLUGIN_PATH = 'C:\msys64\ucrt64\share\qt6\plugins\platforms'"
Write-Host ""
Write-Host "然后：cmake --preset qt   &&   cmake --build build/qt --target qt_demo"
