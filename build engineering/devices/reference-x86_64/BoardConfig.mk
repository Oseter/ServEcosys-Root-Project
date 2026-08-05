#
# ServEcosys 设备配置 - 参考实体 PC 设备
#
# 用法: make DEVICE=reference-x86_64 <target>
#

# 设备标识
DEVICE_NAME := reference-x86_64
DEVICE_ARCH := x86_64
BOOT_ARCH   := x86_64

# 架构相关内核配置片段
DEVICE_KERNEL_FRAGMENT := arch/x86/configs/servecosys-reference.config

# 实体机器参数
QEMU_MACHINE  := q35
QEMU_MEM      := 16384
QEMU_SMP      := 8

# 设备工具链前缀（实体平台常需交叉编译）
TOOLCHAIN_PREFIX :=

# 设备输出路径段
DEVICE_OUT := $(PRODUCT_OUT)/$(DEVICE_NAME)

# 设备特性
DEVICE_MODULES := guard probe pc

# 展示信息
define DEVICE_BANNER
=============================================
 设备: $(DEVICE_NAME)
 架构: $(DEVICE_ARCH)
 类型: 实体参考 PC
=============================================
endef