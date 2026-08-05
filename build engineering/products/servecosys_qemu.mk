#
# ServEcosys 产品定义 - QEMU 虚拟化产品（默认，用于开发验证）
#
# 用法: make PRODUCT=servecosys_qemu <target>   (默认)
#

PRODUCT_NAME := servecosys_qemu
PRODUCT_MODEL := "ServEcosys QEMU (虚拟化开发验证)"

# 产品包含的内核模块集
PRODUCT_MODULES := guard probe pc

# 产品包含的后端/前端组件
PRODUCT_BACKEND := sub_kernel security oipes
PRODUCT_FRONTEND := ui

# 默认设备
PRODUCT_DEFAULT_DEVICE := qemu-x86_64
DEVICE ?= $(PRODUCT_DEFAULT_DEVICE)

PRODUCT_OUT := $(OUT_DIR)/$(PRODUCT_NAME)

KERNEL_DEFCONFIG := servecosys_defconfig

# 产品特性开关
PRODUCT_FEATURES := fingerprint dual-domain permission-ladder snapshot qemu

define PRODUCT_BANNER
=============================================
 产品: $(PRODUCT_NAME)
 型号: $(PRODUCT_MODEL)
 设备: $(DEVICE)
 特性: $(PRODUCT_FEATURES)
=============================================
endef