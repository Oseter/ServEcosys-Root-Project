#
# ServEcosys 产品定义 - 移动产品
#
# 用法: make PRODUCT=servecosys_mobile <target>
#

PRODUCT_NAME := servecosys_mobile
PRODUCT_MODEL := "ServEcosys Mobile (移动设备)"

# 产品包含的内核模块集
PRODUCT_MODULES := guard probe mobile

# 产品包含的后端/前端组件
PRODUCT_BACKEND := sub_kernel security oipes
PRODUCT_FRONTEND := ui

# 默认设备（可在命令行覆盖）
PRODUCT_DEFAULT_DEVICE := reference-mobile
DEVICE ?= $(PRODUCT_DEFAULT_DEVICE)

PRODUCT_OUT := $(OUT_DIR)/$(PRODUCT_NAME)

KERNEL_DEFCONFIG := servecosys_defconfig

# 产品特性开关
PRODUCT_FEATURES := fingerprint dual-domain permission-ladder snapshot

define PRODUCT_BANNER
=============================================
 产品: $(PRODUCT_NAME)
 型号: $(PRODUCT_MODEL)
 设备: $(DEVICE)
 特性: $(PRODUCT_FEATURES)
=============================================
endef