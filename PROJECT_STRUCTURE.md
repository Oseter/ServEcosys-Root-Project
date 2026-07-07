# ServEcosys 项目目录结构蓝图

> 完整的项目结构说明

## 目录树

```
/rootproject/                 # 项目根目录 (Root Project)
│
├── README.md                 # 项目总览
├── PROJECT_STRUCTURE.md      # 本文件
├── INSTALL.md                # 安装指南
├── ROADMAP.md                # 开发路线图
│
├── base/                     # 底基系统源码与构建脚本
│   ├── core/                 # 底基系统核心（最小化运行时）
│   │   ├── base.h           # 底基系统头文件
│   │   └── base.c           # 底基系统实现
│   ├── drivers/              # 底基系统可直接调用的硬件驱动（可选）
│   │   └── .gitkeep
│   └── tools/                # 恢复、刷写、调试工具
│       └── .gitkeep
│
├── boot/                     # 引导启动程序链
│   ├── bootloader.c          # UEFI 引导程序
│   └── Makefile              # 编译配置
│
├── kernel/                   # 主内核文件夹
│   ├── core/                 # 内核中央
│   │   ├── main.c           # 核心代码（进程/内存/网络/安全）
│   │   └── servecosys_defconfig  # 内核配置
│   └── modules/              # 内核模块
│       ├── pc/              # PC 设备适配模块集
│       │   └── pc_modules.c
│       ├── mobile/          # 移动设备适配模块集
│       │   └── mobile_modules.c
│       └── probe/           # 硬件指纹探测模块
│           └── probe_modules.c
│
├── backend/                  # 后端文件夹 (.smle)
│   ├── sub_kernel/          # 后端子内核
│   │   └── README.md
│   ├── security/            # 权限控制台与 SELinux
│   │   └── README.md
│   └── oipes/               # 公共生态服务
│       └── README.md
│
├── frontend/                 # 前端子内核文件夹 (.ssle)
│   ├── ui/                  # 界面层
│   │   └── README.md
│   └── apps/                # 原生第三方应用
│       └── README.md
│
├── restore/                  # 恢复文件区 (factory_image)
│   └── README.md
│
└── scripts/                  # 构建与快照管理脚本
    ├── README.md            # 脚本说明
    ├── build_kernel.sh      # Linux 构建脚本
    ├── build.ps1            # Windows 构建脚本
    ├── generate_keys.sh     # 密钥生成脚本
    ├── qemu_test.sh         # QEMU 测试脚本
    └── initramfs/
        └── init             # initramfs 启动脚本
```

## 核心组件说明

### 1. 底基系统 (base/)

**定位：** UEFI 层最小化运行时

**功能：**
- 直接硬件访问（MMIO、IO 端口）
- 操作系统加载与校验
- 系统恢复（Btrfs 快照、分区修复）
- 可编程引导加载程序

**关键文件：**
- [`base/core/base.h`](file:///C:/Users/TT/lobsterai/project/rootproject/base/core/base.h) - 接口定义
- [`base/core/base.c`](file:///C:/Users/TT/lobsterai/project/rootproject/base/core/base.c) - 实现

---

### 2. 引导程序 (boot/)

**定位：** UEFI 应用，系统启动入口

**功能：**
- 内核签名验证（RSA/ECDSA）
- 硬件指纹生成与传递
- Btrfs 快照配置解析
- 跳转到内核入口

**关键文件：**
- [`boot/bootloader.c`](file:///C:/Users/TT/lobsterai/project/rootproject/boot/bootloader.c) - 主代码
- [`boot/Makefile`](file:///C:/Users/TT/lobsterai/project/rootproject/boot/Makefile) - 编译配置

---

### 3. 内核中央 (kernel/core/)

**定位：** Linux 内核最小化核心

**功能：**
- 进程调度
- 内存管理
- 网络协议栈
- SELinux LSM 钩子
- 硬件指纹管理
- 权限阶梯系统（0-11 级）

**关键文件：**
- [`kernel/core/main.c`](file:///C:/Users/TT/lobsterai/project/rootproject/kernel/core/main.c) - 核心代码
- [`kernel/core/servecosys_defconfig`](file:///C:/Users/TT/lobsterai/project/rootproject/kernel/core/servecosys_defconfig) - 内核配置

---

### 4. 设备模块集 (kernel/modules/)

**定位：** 可插拔设备驱动

**分类：**
- **PC 模块**：PCIe、USB、GPU、网络、音频
- **Mobile 模块**：SoC、触控、传感器、电源
- **Probe 模块**：硬件指纹探测

**特点：**
- 按需加载
- 基于硬件指纹跳过全量探测
- 编译为独立.ko 模块

---

### 5. 后端安全域 (backend/)

**定位：** SELinux `sys_dom_t`，安全策略执行点

**组件：**
- **sub_kernel/** - 后端子内核
  - SELinux 策略管理器
  - 硬件抽象层
  - 权限裁决服务
- **security/** - 权限控制台
  - 0-11 级权限阶梯
  - 共管密钥管理
- **oipes/** - 公共生态服务客户端
  - 身份认证
  - AI 模型推理
  - 推送网关

---

### 6. 前端交互域 (frontend/)

**定位：** SELinux `uid_dom_t`，用户界面层

**组件：**
- **ui/** - 界面层
  - 显示服务
  - 输入管理（PC/移动统一）
  - 窗口合成器
  - 状态栏、通知中心
- **apps/** - 原生第三方应用
  - .ssle 格式应用沙箱
  - 权限动态授予

---

### 7. 恢复系统 (restore/)

**定位：** 系统自愈机制

**三层恢复：**
1. **轻症** - Btrfs 快照回滚（10 秒）
2. **中症** - Recovery 分区重装
3. **重症** - 底基系统终极恢复

**Btrfs 子卷：**
- `@system` - 只读系统基准
- `@data` - 独立用户数据
- `@snapshots` - 自动快照

---

### 8. 构建脚本 (scripts/)

**定位：** 自动化构建和测试

**脚本：**
- **build_kernel.sh** - Linux 完整构建
- **build.ps1** - Windows PowerShell 版本
- **generate_keys.sh** - 共管密钥生成
- **qemu_test.sh** - QEMU 测试
- **initramfs/init** - initramfs 启动脚本

---

## 权限阶梯详解

| 级别 | 名称 | 说明 | 签名要求 |
|------|------|------|----------|
| 0 | READONLY | 只读/只写/只执行 | 无 |
| 1 | SANDBOX | 应用沙盒 | 应用签名 |
| 2 | USER | 普通用户/系统应用 | 系统签名 |
| 3 | DEBUG | 进阶调试 | 开发者签名 |
| 4 | BL_UNLOCK | BL 解锁/特权文件 | 用户自签 |
| 5 | ROOT_SPLIT | Root 分能力 | 自签 + 审计 |
| 6 | MODULE_ROOT | 模块加载 Root | 模块签名 |
| 7 | KERNEL_ROOT | 内核加载 Root | 内核签名 |
| 8 | SELINUX | SELinux 控制 | **共管密钥** |
| 9 | KMOD_LOAD | 内核模块加载 | 模块签名 |
| 10 | CUSTOM_KERNEL | 自定义内核 | 自签链 |
| 11 | BOOTLOADER | 启动链控制 | **共管密钥** |

**共管密钥：** 级别 8 和 11 需要项目维护者 + 独立审计方双方签名，单方无法操作。

---

## 开发优先级

### Phase 0 (当前) ✅
- [x] 项目结构创建
- [x] Bootloader 框架
- [x] 内核核心框架
- [x] 设备模块框架
- [x] 构建脚本框架
- [x] 文档

### Phase 1 (Q2 2026)
- [ ] Linux 6.6 LTS 集成
- [ ] QEMU 可启动原型
- [ ] SELinux 策略编译
- [ ] Btrfs 快照工具

### Phase 2 (Q3 2026)
- [ ] 双域隔离实现
- [ ] IPC 总线（binderfs）
- [ ] 后端安全域服务
- [ ] 前端沙箱运行时

---

## 快速开始

```bash
# 1. 安装工具链
sudo apt install -y build-essential gcc make gnu-efi qemu-system-x86

# 2. 进入项目
cd rootproject

# 3. 编译
./scripts/build_kernel.sh full

# 4. QEMU 测试
./scripts/qemu_test.sh -d
```

---

**ServEcosys Root Project — 为用户而生，因开源而活。**
