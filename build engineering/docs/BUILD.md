# ServEcosys 构建系统手册

本手册说明如何把 ServEcosys 构建成一个可启动的系统。构建入口统一为
顶层 `Makefile`，支持产品化配置（产品 / 设备两层）。

## 1. 目录分层

```
┌─────────────────────────────┐
│  products/  产品配置层        │  产品包含哪些能力、模块集
│  ┌─────────┐ ┌─────────────┐ │
│  │ servecosys_pc │ qemu   │ ...
│  └─────────┘ └─────────────┘ │
├─────────────────────────────┤
│  devices/   设备配置层        │  设备/硬件平台参数
│  ┌─────────────────────────┐ │
│  │ qemu-x86_64 │ reference │ ...
│  └─────────────────────────┘ │
├─────────────────────────────┤
│  kernel/      内核
│   ├── core/   内核中央（原子化）
│   └── modules/ 可插拔模块（guard/probe/pc/mobile）
├─────────────────────────────┤
│  boot/        UEFI 引导程序
│  backend/     后端安全域   (SED)
│  frontend/    前端交互域   (UID)
│  system/      系统集成层
└─────────────────────────────┘
```

### 内核中央（原子化）

内核中央只包含内核最基础的功能，每个基础功能是一个"原子"：

| 原子 | 文件 | 职责 |
|------|------|------|
| 入口 | `kernel/core/main.c` | 初始化序列编排 |
| 进程调度 | `kernel/core/sched.c` | CFS + RT |
| 内存管理 | `kernel/core/mm.c` | 分页/THP/KSM |
| 网络协议栈 | `kernel/core/net.c` | IPv4/IPv6/TCP/UDP |

设备驱动与安全能力由 `kernel/modules/` 可插拔模块承载。

## 2. 快速构建（Ubuntu）

在 VMware Ubuntu 上构建一个可启动操作系统，两条命令即可：

```bash
# 1) 进入构建入口（该仓库为 git 全局仓库，多数场景需先在本仓库外 clone 后进入 build engineering）
#    确保已在此仓库根目录下
cd "build engineering"

# 2) 安装全部构建依赖（gcc/make/kernel 编译工具/busybox/xorriso/qemu/ovmf）
sudo ../root\ project/scripts/setup_ubuntu.sh

# 3) 一键完整构建（获取内核源码 → 集成 → 编译内核/模块 → initramfs → 可启动 ISO）
make full

# 4) 启动验证
make qemu
```

> 说明：由于仓库路径含空格，在 `make` 内已使用引号处理；`ROOT_PROJECT` 默认指向
> `../root project`。若从仓库根目录直接运行，可 `cd "build engineering"` 后再执行上面的 make 目标。

### 顶层构建目标

| 目标 | 作用 |
|------|------|
| `make full` | 一键完整构建（密钥 + 内核 + 模块 + initramfs + ISO） |
| `make all` | 内核 + 模块 + initramfs（不含 ISO） |
| `make kernel-src` | 获取 Linux 6.6 LTS 内核源码 |
| `make kernel` | 集成并编译内核 |
| `make modules` | 编译内核模块（guard/probe/pc/mobile） |
| `make initramfs` | 组装可启动 initramfs（内置 busybox） |
| `make iso` | 生成可启动 ISO（grub-mkrescue / xorriso） || `make qemu` | 在 QEMU 中启动验证 |
| `make keys` / `make sign` | 生成签名密钥 / 对内核签名 |
| `make clean` | 清理构建产物 |

### 关键变量

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `PRODUCT` | `servecosys_qemu` | 产品（pc / mobile / qemu） |
| `DEVICE` | `qemu-x86_64` | 设备 |
| `LINUX_SRC` | `../root project/kernel/linux-src` | 内核源码树 |
| `OUT_DIR` | `../root project/build` | 输出目录 |

## 3. 内核集成说明

内核中央与模块通过 `scripts/apply_kernel_patch.sh` 集成进内核源码树：

1. 把 `kernel/core`（main/sched/mm/net）同步到源码树 `kernel/servecosys/`
2. 把 `kernel/modules/*` 同步到 `kernel/servecosys/modules/`
3. 挂接 Kconfig（`CONFIG_SERVECOSYS_*` 生效）
4. 挂接 Makefile（`obj-$(CONFIG_SERVECOSYS_CORE) += servecosys/`）

```bash
# 手动集成
scripts/apply_kernel_patch.sh kernel/linux-src

# 或通过 make 目标自动执行
make kernel-src    # 获取内核源码
make kernel        # 集成 + 编译
```

## 4. 依赖清单

`manifest.xml` 列出所有上游依赖（内核、busybox、OVMF）及获取方式，
保证构建可复现。

## 5. 产物

构建产物统一输出至 `../root project/build/`（与各构建脚本默认位置一致）：

```
build/
├── vmlinuz                    # 内核映像 (bzImage)
├── initramfs.cpio.gz          # 可启动 initramfs（内置静态 busybox 与 system/ 层）
├── modules/*.ko               # 内核模块（guard/probe 等）
├── ConceptOS.iso              # 概念OS 可启动 ISO（grub-mkrescue / xorriso，UEFI）
└── vmlinuz.sig                # 内核签名（make sign 后）
```

**概念OS (ConceptOS)：** 是 ServEcosys 系操作系统的标准与概念呈现。ISO 采用"live
模式"——initramfs 内自带完整 `system/` 集成层与静态 busybox，找不到磁盘根文件
系统时自动在 tmpfs 上重建根并切换到 `/system/sysinit`，因此无需外部磁盘即可从
ISO 真启动进入概念OS控制台。

**根本理念：以用户为中心，为用户服务。**