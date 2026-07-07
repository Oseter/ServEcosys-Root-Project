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
├── OIPES root project/       # OIPES 生态服务（独立文件夹）
│
└── root project/             # ServEcosys 核心项目
    ├── base/                 # 底基系统源码与构建脚本
    │   ├── core/             # 底基系统核心（最小化运行时）
    │   ├── drivers/          # 底基系统可直接调用的硬件驱动（可选）
    │   └── tools/            # 恢复、刷写、调试工具
    ├── boot/                 # 引导启动程序链
    ├── kernel/               # 主内核文件夹
    │   ├── core/             # 内核中央
    │   └── modules/          # 内核模块 (pc/, mobile/, probe/, ...)
    ├── backend/              # 后端文件夹 (.smle)
    │   ├── sub_kernel/       # 后端子内核
    │   ├── security/         # 权限控制台与 SELinux
    │   └── oipes/            # 公共生态服务
    ├── frontend/             # 前端子内核文件夹 (.ssle)
    │   ├── ui/               # 界面层
    │   └── apps/             # 原生第三方应用
    ├── restore/              # 恢复文件区 (factory_image)
    └── scripts/              # 构建与快照管理脚本
```

---

## 权限阶梯详解

| 级别 | 名称 | 说明 | 控制权 |
|------|------|------|--------|
| 0 | READONLY | 只读/只写/只执行 | 系统 |
| 1 | SANDBOX | 应用沙盒 | 系统 |
| 2 | USER | 普通用户/系统应用 | 系统 |
| 3 | DEBUG | 进阶调试 | 系统 |
| 4 | BL_UNLOCK | BL 解锁/特权文件 | 用户 |
| 5 | ROOT_SPLIT | Root 分能力/自定义恢复 | 用户 |
| 6 | MODULE_ROOT | 模块加载 Root | 系统 |
| 7 | KERNEL_ROOT | 内核加载 Root | 系统 |
| **8** | **SELINUX** | **SELinux 控制** | **用户** ⭐ |
| 9 | KMOD_LOAD | 内核模块加载 | 系统 |
| 10 | CUSTOM_KERNEL | 自定义内核 | 用户 |
| **11** | **BOOTLOADER** | **引导加载程序/启动链** | **用户** ⭐ |

### ⭐ 用户主权原则

**8 级和 11 级由用户完全控制：**

- **8 级（SELinux 控制）**
  - 用户可以自签 SELinux 策略
  - 不需要项目维护者许可
  - 不需要独立审计方批准

- **11 级（引导加载程序/启动链）**
  - 用户可以自签引导加载程序
  - 可以在自己的硬件上运行任何自己信任的代码
  - 完全的控制权

**核心理念：**
> 你的设备，你做主。

---

**ServEcosys Root Project — 为用户而生，因开源而活。**
