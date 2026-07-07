# 构建与快照管理脚本

## 构建脚本

### build.sh (Linux)

```bash
# 查看选项
./scripts/build.sh --help

# 编译全部
./scripts/build.sh full

# 仅编译内核
./scripts/build.sh kernel

# 仅编译引导程序
./scripts/build.sh bootloader

# 生成密钥
./scripts/build.sh keys

# 签名内核
./scripts/build.sh sign
```

### build.ps1 (Windows)

```powershell
# 编译全部
.\scripts\build.ps1 -Target full

# 仅编译内核
.\scripts\build.ps1 -Target kernel
```

## 快照管理

### snapshot.sh

```bash
# 创建快照
./scripts/snapshot.sh create "Before update"

# 列出快照
./scripts/snapshot.sh list

# 删除快照
./scripts/snapshot.sh delete snap_001

# 回滚到快照
./scripts/snapshot.sh rollback snap_001
```

## QEMU 测试

### qemu_test.sh

```bash
# x86_64 测试
./scripts/qemu_test.sh

# ARM64 测试
./scripts/qemu_test.sh -a arm64

# 调试模式
./scripts/qemu_test.sh -d -g
```

## 文件结构

```
scripts/
├── README.md              # 本文件
├── build.sh               # Linux 构建脚本
├── build.ps1              # Windows 构建脚本
├── generate_keys.sh       # 密钥生成
├── qemu_test.sh           # QEMU 测试
├── snapshot.sh            # 快照管理
└── initramfs/
    └── init               # initramfs 启动脚本
```

## 开发状态

✅ 基础框架已创建
⏳ 完整功能待实现

---

**ServEcosys — 为用户而生，因开源而活。**
