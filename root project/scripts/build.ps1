#
# ServEcosys Kernel Build Script - PowerShell
# 
# 编译主内核和设备模块集 (Windows 版本)
# 
# 用法:
#   .\build.ps1 -Target pc
#   .\build.ps1 -Target mobile
#   .\build.ps1 -Target all
#

param(
    [ValidateSet('pc', 'mobile', 'all', 'initramfs', 'sign')]
    [string]$Target = 'all'
)

$ErrorActionPreference = 'Stop'

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$KernelDir = Join-Path $ProjectRoot 'kernel'
$OutputDir = Join-Path $ProjectRoot 'build'

# 颜色输出
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

# 检查依赖
function Check-Dependencies {
    Write-LogInfo "Checking dependencies..."
    
    $deps = @('make', 'gcc', 'git')
    $missing = @()
    
    foreach ($dep in $deps) {
        if (-not (Get-Command $dep -ErrorAction SilentlyContinue)) {
            $missing += $dep
        }
    }
    
    if ($missing.Count -gt 0) {
        Write-LogError "Missing dependencies: $($missing -join ', ')"
        Write-LogInfo "Please install WSL2 or MinGW with Linux kernel build tools"
        exit 1
    }
    
    Write-LogInfo "All dependencies OK"
}

# 编译主内核
function Build-KernelCore {
    Write-LogInfo "Building kernel core..."
    
    $coreDir = Join-Path $KernelDir 'core'
    
    # TODO: 配置内核编译
    # make -C /lib/modules/$(uname -r)/build M=$coreDir modules
    
    Write-LogInfo "Kernel core build complete"
}

# 编译 PC 模块集
function Build-PCModules {
    Write-LogInfo "Building PC modules..."
    
    $pcDir = Join-Path $KernelDir 'modules' 'pc'
    
    # TODO: 编译 PC 设备驱动模块
    
    Write-LogInfo "PC modules build complete"
}

# 编译移动模块集
function Build-MobileModules {
    Write-LogInfo "Building mobile modules..."
    
    $mobileDir = Join-Path $KernelDir 'modules' 'mobile'
    
    # TODO: 编译移动设备驱动模块
    
    Write-LogInfo "Mobile modules build complete"
}

# 编译硬件探测模块
function Build-ProbeModules {
    Write-LogInfo "Building probe modules..."
    
    $probeDir = Join-Path $KernelDir 'modules' 'probe'
    
    # TODO: 编译硬件指纹探测模块
    
    Write-LogInfo "Probe modules build complete"
}

# 生成 initramfs
function Generate-Initramfs {
    Write-LogInfo "Generating initramfs..."
    
    $initramfsSrc = Join-Path $ScriptsDir 'initramfs'
    $initramfsOut = Join-Path $OutputDir 'initramfs.cpio.gz'
    
    New-Item -ItemType Directory -Force -Path $initramfsSrc | Out-Null
    
    # TODO: 创建 initramfs 内容
    
    Write-LogInfo "initramfs generated: $initramfsOut"
}

# 主函数
function Main {
    Write-LogInfo "ServEcosys Kernel Build System"
    Write-LogInfo "Target: $Target"
    Write-LogInfo "Output: $OutputDir"
    
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
    
    Check-Dependencies
    
    switch ($Target) {
        'pc' {
            Build-KernelCore
            Build-PCModules
            Build-ProbeModules
        }
        'mobile' {
            Build-KernelCore
            Build-MobileModules
            Build-ProbeModules
        }
        'all' {
            Build-KernelCore
            Build-PCModules
            Build-MobileModules
            Build-ProbeModules
        }
        'initramfs' {
            Generate-Initramfs
        }
        'sign' {
            Write-LogInfo "Signing kernel image..."
            # TODO: 使用共管密钥签名
        }
    }
    
    Write-LogInfo "Build complete!"
}

Main
