#
# ServEcosys Kernel Build Script - PowerShell
#
# Windows 侧统一构建入口。实际构建由顶层 Makefile（make）执行，
# 本脚本负责参数解析、依赖检查与 make 调用（经 WSL2 或 MSYS2）。
#
# 用法:
#   .\build.ps1 -Target all                    # 构建默认产品
#   .\build.ps1 -Target kernel
#   .\build.ps1 -Product servecosys_pc -Target all
#   .\build.ps1 -Target qemu
#

param(
    [ValidateSet('all', 'kernel', 'modules', 'bootloader', 'initramfs',
                 'image', 'qemu', 'keys', 'sign', 'clean', 'help')]
    [string]$Target = 'all',

    [ValidateSet('servecosys_qemu', 'servecosys_pc', 'servecosys_mobile')]
    [string]$Product = 'servecosys_qemu',

    [ValidateSet('qemu-x86_64', 'reference-x86_64')]
    [string]$Device = 'qemu-x86_64',

    [string]$LinuxSrc = 'kernel/linux-src',
    [string]$OutDir = 'build'
)

$ErrorActionPreference = 'Stop'

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir          # root project (操作系统底层架构)
$TopRoot = Split-Path -Parent $ProjectRoot            # ServEcosys-Root-Project (顶层)
$BuildEng = Join-Path $TopRoot 'build engineering'    # 工程组织 (统一构建入口)

function Write-LogInfo {
    param([string]$Message)
    Write-Host "[INFO] $Message" -ForegroundColor Green
}

function Write-LogWarn {
    param([string]$Message)
    Write-Host "[WARN] $Message" -ForegroundColor Yellow
}

function Write-LogError {
    param([string]$Message)
    Write-Host "[ERROR] $Message" -ForegroundColor Red
}

# 检查 make 可用性（WSL2 或 MSYS2/MinGW）
function Check-Dependencies {
    Write-LogInfo "Checking build tools..."

    $makeCmd = Get-Command make -ErrorAction SilentlyContinue
    if (-not $makeCmd) {
        # 尝试 WSL2
        $wsl = Get-Command wsl -ErrorAction SilentlyContinue
        if ($wsl) {
            Write-LogInfo "Using WSL2 for make..."
            $script:UseWsl = $true
            return
        }
        Write-LogError "未找到 make。请安装 MSYS2/MinGW 或启用 WSL2。"
        exit 1
    }
    $script:UseWsl = $false
    Write-LogInfo "make: $($makeCmd.Source)"
}

$script:UseWsl = $false

function Invoke-Make {
    param([string]$MakeArgs)
    if ($script:UseWsl) {
        $wslPath = ($BuildEng -replace '\', '/').Replace('C:', '/mnt/c')
        & wsl bash -lc "cd '$wslPath' && make $MakeArgs" 
        if ($LASTEXITCODE -ne 0) { throw "make 失败: $MakeArgs" }
    } else {
        Push-Location $BuildEng
        try {
            & make $MakeArgs.Split(' ')
            if ($LASTEXITCODE -ne 0) { throw "make 失败: $MakeArgs" }
        } finally {
            Pop-Location
        }
    }
}

function Main {
    Write-LogInfo "ServEcosys 构建系统 (Windows)"
    Write-LogInfo "目标: $Target | 产品: $Product | 设备: $Device"

    Check-Dependencies

    $baseVars = "PRODUCT=$Product DEVICE=$Device LINUX_SRC=$LinuxSrc OUT_DIR=$OutDir"

    switch ($Target) {
        'help' {
            Invoke-Make "help $baseVars"
        }
        'clean' {
            Invoke-Make "clean $baseVars"
        }
        default {
            Invoke-Make "$Target $baseVars"
        }
    }

    Write-LogInfo "构建完成！产物目录: $OutDir/"
}

Main