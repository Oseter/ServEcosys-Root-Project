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

## 2. 快速构建

```bash
# 构建默认产品（servecosys_qemu）全部产物
make all

# 构建指定产品
make PRODUCT=servecosys_pc all
make PRODUCT=servecosys_mobile DEVICE=qemu-x86_64 all

# 常用目标
make kernel modules initramfs image    # 内核/模块/initramfs/镜像
make qemu                              # QEMU 启动验证
make bootloader keys sign              # 引导程序/密钥/签名
make clean                             # 清理
```

### 关键变量

| 变量 | 值 | 说明 |
|------|-----|------|
| `PRODUCT` | servecosys_qemu / servecosys_pc / servecosys_mobile | 产品 |
| `DEVICE` | qemu-x86_64 / reference-x86_64 | 设备 |
| `LINUX_SRC` | 默认 `kernel/linux-src` | 内核源码树 |
| `OUT_DIR` | 默认 `build` | 输出目录 |

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

构建产物输出至 `build/`：

```
build/
├── vmlinuz                    # 内核映像
├── initramfs.cpio.gz          # initramfs
├── modules/*.ko               # 内核模块
├── servecosys.img             # 可启动磁盘镜像（可选）
└── vmlinuz.sig                # 内核签名（make sign 后）
```

**根本理念：以用户为中心，为用户服务。**