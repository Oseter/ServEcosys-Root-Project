#
# ServEcosys Root Project - GitHub 推送脚本
#
# 用法:
#   .\push_to_github.ps1              # 交互式
#   .\push_to_github.ps1 -Auto        # 自动模式
#

param(
    [string]$CommitMessage = "Initial commit: ServEcosys Root Project structure",
    [switch]$Auto = $false
)

$ErrorActionPreference = 'Stop'

# 颜色输出
function Write-Info {
    param([string]$Message)
    Write-Host "[INFO] $Message" -ForegroundColor Green
}

function Write-Warn {
    param([string]$Message)
    Write-Host "[WARN] $Message" -ForegroundColor Yellow
}

function Write-Error {
    param([string]$Message)
    Write-Host "[ERROR] $Message" -ForegroundColor Red
}

function Write-Step {
    param([string]$Message)
    Write-Host "[STEP] $Message" -ForegroundColor Cyan
}

# 检查 Git 是否安装
function Test-GitInstalled {
    try {
        $null = Get-Command git -ErrorAction Stop
        return $true
    } catch {
        return $false
    }
}

# 主函数
function Main {
    Write-Info "ServEcosys Root Project - GitHub 推送工具"
    Write-Info "==========================================="
    Write-Host ""
    
    # 检查 Git
    if (-not (Test-GitInstalled)) {
        Write-Error "Git 未安装！"
        Write-Host ""
        Write-Info "请先安装 Git for Windows:"
        Write-Info "  下载地址：https://git-scm.com/download/win"
        Write-Host ""
        Write-Info "或者使用 winget 安装:"
        Write-Host "  winget install Git.Git" -ForegroundColor Cyan
        Write-Host ""
        pause
        exit 1
    }
    
    Write-Info "Git 已安装 ✓"
    Write-Host ""
    
    # 进入项目目录
    $projectPath = "C:\Users\TT\lobsterai\project\rootproject"
    
    if (-not (Test-Path $projectPath)) {
        Write-Error "项目目录不存在：$projectPath"
        exit 1
    }
    
    Set-Location $projectPath
    Write-Step "项目目录：$projectPath"
    Write-Host ""
    
    # 显示将要推送的文件
    Write-Info "将要推送的文件:"
    $files = Get-ChildItem -Recurse -File | Where-Object { $_.Name -ne ".git" }
    Write-Host "  共 $($files.Count) 个文件"
    Write-Host ""
    
    if (-not $Auto) {
        $confirm = Read-Host "继续推送到 GitHub? (y/N)"
        if ($confirm -notmatch '^[Yy]$') {
            Write-Info "已取消"
            return
        }
    }
    
    Write-Host ""
    
    # 步骤 1: 初始化 Git
    Write-Step "初始化 Git 仓库..."
    if (-not (Test-Path ".git")) {
        git init
        Write-Info "Git 仓库已初始化 ✓"
    } else {
        Write-Info "Git 仓库已存在 ✓"
    }
    Write-Host ""
    
    # 步骤 2: 添加文件
    Write-Step "添加所有文件..."
    git add .
    Write-Info "文件已添加到暂存区 ✓"
    Write-Host ""
    
    # 步骤 3: 提交
    Write-Step "提交更改..."
    $fullCommitMessage = @"
$CommitMessage

项目结构：
- base/: 底基系统核心（UEFI 层运行时）
- boot/: UEFI 引导程序
- kernel/core/: 内核中央（最小化核心）
- kernel/modules/: 可插拔设备模块集
- backend/: 后端安全域（SED）
- frontend/: 前端交互域（UID）
- restore/: 系统自愈机制
- scripts/: 构建脚本

架构特点：
- 最小化内核核心（调度/内存/网络/LSM）
- 设备驱动全部模块化
- 硬件指纹快速启动
- 权限阶梯系统（0-11 级）
- SELinux 强制集成
- 双域隔离架构

Signed-off-by: ServEcosys Project
"@
    
    git commit -m $fullCommitMessage
    Write-Info "提交完成 ✓"
    Write-Host ""
    
    # 步骤 4: 关联远程仓库
    Write-Step "关联远程仓库..."
    $remoteUrl = "https://github.com/Oseter/ServEcosys-Root-Project.git"
    
    $existingRemote = git remote get-url origin 2>$null
    if ($existingRemote) {
        Write-Info "远程仓库已存在：$existingRemote"
        $change = Read-Host "是否更改为 $remoteUrl? (y/N)"
        if ($change -match '^[Yy]$') {
            git remote set-url origin $remoteUrl
        }
    } else {
        git remote add origin $remoteUrl
        Write-Info "远程仓库已关联 ✓"
    }
    Write-Host ""
    
    # 步骤 5: 设置分支
    Write-Step "设置主分支..."
    git branch -M main
    Write-Info "主分支已设置为 'main' ✓"
    Write-Host ""
    
    # 步骤 6: 推送
    Write-Step "推送到 GitHub..."
    Write-Warn "需要 GitHub 凭据（用户名/密码或 Token）"
    Write-Host ""
    
    git push -u origin main
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host ""
        Write-Info "==========================================="
        Write-Info "推送成功！✓"
        Write-Info "==========================================="
        Write-Host ""
        Write-Info "查看仓库:"
        Write-Host "  https://github.com/Oseter/ServEcosys-Root-Project" -ForegroundColor Cyan
        Write-Host ""
        Write-Info "下一步:"
        Write-Info "  1. 完善 GitHub 仓库页面（添加徽章、LICENSE 等）"
        Write-Info "  2. 启用 GitHub Actions（自动化构建）"
        Write-Info "  3. 设置 GitHub Pages（项目文档网站）"
        Write-Host ""
    } else {
        Write-Host ""
        Write-Warn "推送失败，请检查:"
        Write-Warn "  1. GitHub 凭据是否正确"
        Write-Warn "  2. 是否有仓库写入权限"
        Write-Warn "  3. 网络连接是否正常"
        Write-Host ""
        Write-Info "详细指南请参阅：GIT_PUSH_GUIDE.md"
        Write-Host ""
    }
    
    pause
}

Main
