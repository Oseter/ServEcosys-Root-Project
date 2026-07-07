# ServEcosys Root Project

> 自由开源，为用户而生

[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](LICENSE)
[![Phase](https://img.shields.io/badge/Phase-0%20(Foundation)-green.svg)](servecosys/ROADMAP.md)
[![Architecture](https://img.shields.io/badge/Arch-x86__64%20%7C%20ARM64-orange.svg)](#核心架构)

---

## 根本纲领

**以用户为中心，为用户服务。**

ServEcosys 是一个基于 Linux 构建、面向 PC / 移动 / IoT 多设备形态的开源根项目（Root Project）。定位类似 AOSP（Android Open Source Project），旨在提供一个自由、开源、集百家之长的通用操作系统底座。其设计理念继承 Linux 的自由与开放精神，从架构到权限、从生态到恢复——都以用户的**掌控权**、**选择权**和**安全感**为出发点，技术机制保障「你的设备你做主、你的数据你掌控、你的生态你选择」。

## 核心架构

```
┌─────────────────────────────────────────────────────────────┐
│                    底基系统 (Base System)                    │
│                    UEFI 层 / 恢复环境                         │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    唯一 Linux 内核                            │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │  内核中央    │  │  设备模块集  │  │  SELinux 安全框架    │  │
│  │  (core)     │  │  (modules)  │  │  (不可降级)          │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┴───────────────┐
              ▼ IPC (binderfs/unix socket)    ▼
┌─────────────────────────┐     ┌─────────────────────────┐
│   后端安全域 (SED)       │     │   前端交互域 (UID)       │
│   sys_dom_t             │     │   uid_dom_t             │
│   - SELinux 策略执行     │◄───►│   - 显示服务             │
│   - 硬件抽象            │ IPC │   - 应用沙箱             │
│   - 权限裁决            │     │   - 系统界面             │
│   - OIPES 客户端        │     │   - 输入管理             │
└─────────────────────────┘     └─────────────────────────┘
```

## 快速开始

```bash
# 克隆项目
git clone https://github.com/Oseter/ServEcosys-Root-Project.git
cd ServEcosys-Root-Project

# 安装工具链（Ubuntu/Debian）
sudo apt install -y build-essential gcc make gnu-efi qemu-system-x86

# 查看构建选项
ls servecosys/scripts/

# 执行构建
./servecosys/scripts/build_kernel.sh full
```

## 项目文档

| 文档 | 说明 |
|------|------|
| [README（完整版）](servecosys/README.md) | 项目总览白皮书 |
| [项目结构](servecosys/PROJECT_STRUCTURE.md) | 目录结构与架构设计 |
| [路线图](servecosys/ROADMAP.md) | 开发阶段与规划 |
| [安装指南](servecosys/INSTALL.md) | 编译与部署 |
| [内核架构](servecosys/kernel/core/ARCHITECTURE.md) | 内核中央设计说明 |

## 核心特性

- **双域隔离**：SED（后端安全域 sys_dom_t）+ UID（前端交互域 uid_dom_t），安全策略集中执行
- **权限阶梯**：0-11 级精细权限体系，8 级（SELinux 控制）和 11 级（启动链控制）由用户完全掌控
- **系统自愈**：Btrfs 三子卷（@system / @data / @snapshots），10 秒快照回滚
- **OIPES 开放生态**：中立非营利服务端，客户端开源，不捆绑任何云服务商
- **可插拔模块**：驱动全部剥离为 modules/{pc,mobile,iot,probe}，内核核心 ~10MB
- **开发者友好**：ELF 不动、libc 不变、ssle-packer 工具链、QEMU 多架构仿真

## 开发状态

当前处于 **Phase 0（基础框架）**，已完成项目结构、引导程序框架、内核核心框架、设备模块框架和构建脚本。详见 [ROADMAP.md](servecosys/ROADMAP.md)。

## 贡献

欢迎贡献！请参阅 [CONTRIBUTING.md](CONTRIBUTING.md) 了解 Fork → PR 流程。

## License

[GPL v2](LICENSE)

---

**ServEcosys Root Project — 为用户而生，因开源而活。**
