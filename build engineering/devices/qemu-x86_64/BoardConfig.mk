#
# ServEcosys 设备配置 - QEMU x86_64 参考设备
#
# 该文件定义"硬件/虚拟平台"层面的参数，产品层选择设备后加载。
# 对应顶层 Makefile: make DEVICE=qemu-x86_64
#

# 设备标识
DEVICE_NAME := qemu-x86_64
DEVICE_ARCH := x86_64
BOOT_ARCH   := x86_64

# 架构相关内核配置（追加到 servecosys_defconfig）
DEVICE_KERNEL_FRAGMENT := arch/x86/configs/servecosys-qemu.config

# QEMU 启动参数
QEMU_MACHINE  := q35
QEMU_MEM      := 4096
QEMU_SMP      := 4
QEMU_KERNEL   := $(OUT_DIR)/vmlinuz
QEMU_INITRD   := $(OUT_DIR)/initramfs.cpio.gz

# 设备工具链前缀（若需交叉编译，设为如 aarch64-linux-gnu-）
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
 机器: $(QEMU_MACHINE) 内存: $(QEMU_MEM)MB
=============================================
endef