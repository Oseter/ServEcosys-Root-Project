# 内核中央架构说明

> 最小化核心 + 可插拔设备模块

## 内核中央（原子化）

**内核中央只包含内核最基础的功能，每个基础功能是一个原子：**

| 原子 | 文件 | 职责 |
|------|------|------|
| 入口 | `main.c` | 内核中央初始化序列编排 |
| 进程调度 | `sched.c` | CFS + RT 调度基础 |
| 内存管理 | `mm.c` | 分页 / 虚拟内存 / THP / KSM |
| 网络协议栈 | `net.c` | IPv4/IPv6 双栈、TCP/UDP |

**原则：**
1. 每个基础功能是一个原子（独立源文件），原子间通过 `core.h` 暴露的
   `servecsys_<atom>_init()` 接口衔接。
2. `main.c` 是内核中央入口，负责在 `early_initcall` 中按依赖顺序
   编排各原子（内存 → 调度 → 网络）。
3. 设备驱动、安全防护等能力由可插拔模块承载，不入内核中央。

**设备驱动全部剥离为可插拔模块：**
- 📦 `kernel/modules/pc/` - PC 设备适配模块集
- 📦 `kernel/modules/mobile/` - 移动设备适配模块集
- 📦 `kernel/modules/probe/` - 硬件指纹探测模块
- 📦 `kernel/modules/guard/` - 0day 检测与拦截模块
- 📦 `kernel/modules/iot/` - IoT 设备（待实现）
- 📦 `kernel/modules/embedded/` - 嵌入式设备（待实现）

---

## 结构

### ✅ ServEcosys 架构

```
┌─────────────────────────────────────┐
│        ServEcosys Kernel Core       │
│  ┌─────────────────────────────┐    │
│  │ 原子[sched] 进程调度         │    │
│  │ 原子[mm]    内存管理         │    │
│  │ 原子[net]   网络协议栈       │    │
│  └─────────────────────────────┘    │
└─────────────────────────────────────┘
              │
              │ 模块加载接口
              ▼
┌─────────────────────────────────────┐
│      可插拔设备模块集               │
│  ┌─────────────┐ ┌───────────────┐  │
│  │ PC 模块集    │ │ Mobile 模块集  │  │
│  │ - PCIe/GPU  │ │ - SoC/触控    │  │
│  │ - USB/网络  │ │ - 传感器/电源 │  │
│  │ - 音频      │ │ - 蜂窝网络    │  │
│  └─────────────┘ └───────────────┘  │
│  ┌─────────────┐ ┌───────────────┐  │
│  │ Probe 模块   │ │ IoT 模块       │  │
│  │ - 硬件指纹  │ │ - 待实现      │  │
│  └─────────────┘ └───────────────┘  │
└─────────────────────────────────────┘
```

**优势：**
- 内核精简（~10MB）
- 启动快（基于硬件指纹跳过全量探测）
- 安全边界清晰（核心 vs 驱动隔离）
- 多设备适配（按需加载模块集）

---

## 原子详解

> 内核中央只包含内核最基础的功能，每个基础功能是一个原子。

### 原子 1. 进程调度（`sched.c`）

**基于 CFS (Completely Fair Scheduler)**

```c
// 支持特性
- 多核调度（SMP）
- 实时任务优先级（SCHED_FIFO, SCHED_RR）
- 控制组调度（cgroup）
- 动态优先级调整
```

**代码位置：** [`kernel/core/sched.c`](sched.c)

---

### 原子 2. 内存管理（`mm.c`）

**分页虚拟内存系统**

```c
// 支持特性
- 4KB 标准页 + 2MB/1GB 大页
- 匿名页 + 文件映射
- 内存控制组（cgroup）
- 透明大页（THP）
- 内核同页合并（KSM）
```

**代码位置：** [`kernel/core/mm.c`](mm.c)

---

### 原子 3. 网络协议栈（`net.c`）

**最小化双栈支持**

```c
// 内置协议
- IPv4 / IPv6
- TCP / UDP / SCTP
- 基本路由
- 网络过滤（netfilter）

// 不包含（剥离到模块）
- 无线协议（802.11）→ kernel/modules/pc/
- 蓝牙 → kernel/modules/pc/
- 特殊协议（IPX, Appletalk）
```

**代码位置：** [`kernel/core/net.c`](net.c)

---

### 入口（`main.c`）

**内核中央初始化序列编排**

```c
// 初始化顺序（按依赖关系）
servecsys_mm_init()      // 内存管理（最早）
servecsys_sched_init()   // 进程调度
servecsys_net_init()     // 网络协议栈
```

**代码位置：** [`kernel/core/main.c`](main.c)

---

> 说明：核心安全钩子（LSM/SELinux 强制集成）、硬件指纹管理、权限阶梯
> 等能力不属于"内核最基础功能"，由可插拔模块与上层承载，不在内核中央。
> 详见 `kernel/modules/` 与项目顶层文档。

---

## 模块加载接口

### 设备驱动加载流程

```c
// 1. 用户空间请求（udev/systemd）
modprobe e1000e

// 2. 内核模块加载器（由上层安全策略承载）
servecsys_module_load("e1000e")
  ├─ 检查权限（level >= 6）
  ├─ 验证模块签名
  ├─ 检查硬件指纹匹配
  └─ request_module("e1000e")

// 3. 模块初始化
module_init(e1000e_init)
  └─ 注册 PCI 驱动、网络接口
```

> 说明：模块加载接口的权限检查由上层安全策略模块承载，
> 不在内核中央（内核中央只包含最基础功能）。

---

## 编译配置

### 最小化内置选项

```kconfig
# 内核中央原子（内置，obj-y）
CONFIG_SERVECOSYS_CORE=y        # 内核中央
CONFIG_SERVECOSYS_CORE_SCHED=y  # 原子: 进程调度
CONFIG_SERVECOSYS_CORE_MM=y     # 原子: 内存管理
CONFIG_SERVECOSYS_CORE_NET=y    # 原子: 网络协议栈

# 内核基础能力（内核原生）
CONFIG_SCHED_MC=y          # 多核调度
CONFIG_TRANSPARENT_HUGEPAGE=y  # 透明大页
CONFIG_INET=y              # IPv4
CONFIG_IPV6=y              # IPv6
CONFIG_SECURITY_SELINUX=y  # SELinux

# 可插拔模块（模块）
CONFIG_SERVECOSYS_GUARD=m  # 0day 检测与拦截
CONFIG_SERVECOSYS_PROBE=m  # 硬件指纹探测
CONFIG_SERVECOSYS_PC_MODULES=m   # PC 设备适配
CONFIG_SERVECOSYS_MOBILE_MODULES=m # 移动设备适配
CONFIG_DRM=m               # 显卡
CONFIG_E1000E=m            # Intel 网络
CONFIG_USB_XHCI_HCD=m      # USB 3.0
CONFIG_SATA_AHCI=m         # SATA
CONFIG_NVME=m              # NVMe
```

**配置文件：** [`kernel/core/servecosys_defconfig`](servecosys_defconfig)

---

## 启动流程

```
1. Bootloader 传递硬件指纹
        ↓
2. 内核早期初始化（内核中央入口 main.c）
   - 原子[mm]   内存管理
   - 原子[sched] 进程调度
   - 原子[net]  网络协议栈
        ↓
3. 内核后期初始化
   - 加载安全策略模块（guard 等）
   - 启动 SED（后端安全域）
   - 启动 UID（前端交互域）
        ↓
4. 用户空间启动（systemd）
        ↓
5. udev 探测硬件
        ↓
6. 按需加载设备模块
   - kernel/modules/pc/
   - kernel/modules/mobile/
        ↓
7. 系统就绪
```

---

## 性能目标

| 指标 | ServEcosys 目标 |
|------|-----------------|
| 内核大小 | ~10MB |
| 启动时间（冷启动） | ~10s |
| 启动时间（热启动*） | ~5s |
| 内存占用（空闲） | ~200MB |

*热启动：使用缓存的硬件指纹，跳过全量探测

---

## 下一步开发

### Phase 1 (Q2 2026)
- [ ] 集成 Linux 6.6 LTS 源码
- [ ] 编译通过，QEMU 可启动
- [ ] 硬件指纹模块工作
- [ ] SELinux 策略加载成功

### Phase 2 (Q3 2026)
- [ ] 设备模块集完善（PC/Mobile）
- [ ] 模块签名验证
- [ ] 热启动优化（<3 秒）

---

**ServEcosys — 为用户而生，因开源而活。**

**根本理念：以用户为中心，为用户服务。**
