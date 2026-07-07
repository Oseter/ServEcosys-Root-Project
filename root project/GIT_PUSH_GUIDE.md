# ServEcosys Root Project - Git 推送指南

## 前置条件

需要将项目推送到 GitHub 仓库：
- **仓库 URL**: `https://github.com/Oseter/ServEcosys-Root-Project.git`
- **目标分支**: `main`
- **目标路径**: `root project/` 子目录

---

## 方式一：使用 Git for Windows（推荐）

### 1. 安装 Git for Windows

下载安装：https://git-scm.com/download/win

安装后重启 PowerShell。

### 2. 初始化并推送

```powershell
# 进入项目目录
cd C:\Users\TT\lobsterai\project\rootproject

# 初始化 Git 仓库
git init

# 添加所有文件
git add .

# 首次提交
git commit -m "Initial commit: ServEcosys Root Project structure

- base/: 底基系统核心（UEFI 层运行时）
- boot/: UEFI 引导程序（签名验证/硬件指纹/Btrfs 快照）
- kernel/core/: 内核中央（最小化：调度/内存/网络/LSM）
- kernel/modules/: 可插拔设备模块集（PC/Mobile/Probe）
- backend/: 后端安全域（SED/.smle）
- frontend/: 前端交互域（UID/.ssle）
- restore/: 系统自愈机制（Btrfs 原子快照）
- scripts/: 构建与快照管理脚本

架构特点：
- 最小化内核核心（~10MB）
- 设备驱动全部模块化（可插拔）
- 硬件指纹快速启动
- 权限阶梯系统（0-11 级）
- SELinux 强制集成
- 双域隔离架构（SED + UID）

Signed-off-by: ServEcosys Project <noreply@servecosys.org>"

# 关联远程仓库
git remote add origin https://github.com/Oseter/ServEcosys-Root-Project.git

# 推送到 main 分支
git branch -M main
git push -u origin main
```

---

## 方式二：使用 GitHub Desktop

### 1. 安装 GitHub Desktop

下载：https://desktop.github.com/

### 2. 添加项目

1. 打开 GitHub Desktop
2. File → Add Local Repository
3. 选择 `C:\Users\TT\lobsterai\project\rootproject`
4. 点击 "Initialize Git Repository"

### 3. 提交并推送

1. 在 Changes 标签页，输入提交信息：
   ```
   Initial commit: ServEcosys Root Project structure
   
   完整的项目目录结构和核心代码框架
   ```

2. 点击 "Commit to main"

3. 点击 "Publish repository"

4. 选择现有仓库：`Oseter/ServEcosys-Root-Project`

5. 点击 "Publish"

---

## 方式三：使用 WSL（如果已安装）

### 1. 在 WSL 中执行

```bash
# 进入项目目录
cd /mnt/c/Users/TT/lobsterai/project/rootproject

# 初始化 Git
git init
git add .

# 提交
git commit -m "Initial commit: ServEcosys Root Project structure"

# 关联远程
git remote add origin https://github.com/Oseter/ServEcosys-Root-Project.git

# 推送
git branch -M main
git push -u origin main
```

---

## 方式四：使用 GitHub CLI

### 1. 安装 GitHub CLI

```powershell
# 使用 winget
winget install GitHub.cli

# 或使用 Chocolatey
choco install gh
```

### 2. 认证并推送

```powershell
# 认证
gh auth login

# 进入项目目录
cd C:\Users\TT\lobsterai\project\rootproject

# 初始化并提交
git init
git add .
git commit -m "Initial commit: ServEcosys Root Project"

# 推送到现有仓库
git remote add origin https://github.com/Oseter/ServEcosys-Root-Project.git
git branch -M main
git push -u origin main
```

---

## 推送后验证

### 1. 检查 GitHub 仓库

访问：https://github.com/Oseter/ServEcosys-Root-Project/tree/main

应该看到以下文件结构：

```
├── README.md
├── PROJECT_STRUCTURE.md
├── INSTALL.md
├── ROADMAP.md
├── base/
├── boot/
├── kernel/
├── backend/
├── frontend/
├── restore/
└── scripts/
```

### 2. 检查文件内容

确认关键文件已上传：
- [`README.md`](file:///C:/Users/TT/lobsterai/project/rootproject/README.md) - 项目总览
- [`kernel/core/main.c`](file:///C:/Users/TT/lobsterai/project/rootproject/kernel/core/main.c) - 内核核心代码
- [`kernel/core/servecosys_defconfig`](file:///C:/Users/TT/lobsterai/project/rootproject/kernel/core/servecosys_defconfig) - 内核配置
- [`boot/bootloader.c`](file:///C:/Users/TT/lobsterai/project/rootproject/boot/bootloader.c) - UEFI 引导程序
- [`scripts/build_kernel.sh`](file:///C:/Users/TT/lobsterai/project/rootproject/scripts/build_kernel.sh) - 构建脚本

---

## 常见问题

### Q1: 推送失败 - "Permission denied"

**解决：**
```bash
# 检查 SSH 密钥
gh auth setup-git

# 或使用 HTTPS + Token
git remote set-url origin https://<TOKEN>@github.com/Oseter/ServEcosys-Root-Project.git
```

### Q2: 仓库已存在内容

**解决：**
```bash
# 拉取现有内容
git pull origin main --allow-unrelated-histories

# 解决冲突后推送
git push -u origin main
```

### Q3: 文件太大无法推送

**解决：**
```bash
# 检查大文件
git rev-list --objects --all | git cat-file --filter-object-size=0 -s --all

# 使用 Git LFS
git lfs install
git lfs track "*.iso"
git lfs track "*.img"
```

---

## 推荐：使用 Git for Windows

最简单的方式是安装 **Git for Windows**，然后执行方式一的命令。

安装后，在 PowerShell 中运行：

```powershell
cd C:\Users\TT\lobsterai\project\rootproject
git init
git add .
git commit -m "Initial commit: ServEcosys Root Project"
git remote add origin https://github.com/Oseter/ServEcosys-Root-Project.git
git branch -M main
git push -u origin main
```

---

## 推送完成后的下一步

1. **完善 GitHub 仓库页面**
   - 添加项目徽章（Badge）
   - 完善 README.md
   - 添加 LICENSE 文件
   - 添加 CONTRIBUTING.md

2. **设置 GitHub Pages**（可选）
   - 用于项目文档网站

3. **启用 GitHub Actions**
   - 自动化构建和测试

4. **添加项目标签**
   - Topics: `operating-system`, `linux`, `selinux`, `uefi`, `open-source`

---

**ServEcosys — 为用户而生，因开源而活。**
