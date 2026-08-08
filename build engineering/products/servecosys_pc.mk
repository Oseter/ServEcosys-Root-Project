#
# ServEcosys 产品定义 - 通用 PC 产品
#
# 被顶层 Makefile 以 -include 方式加载，提供产品级配置变量。
# 用法: make PRODUCT=servecosys_pc <target>
#

# 产品标识
PRODUCT_NAME := servecosys_pc
PRODUCT_MODEL := "ServEcosys PC (通用个人电脑)"

# 产品包含的内核模块集
PRODUCT_MODULES := guard probe pc

# 产品包含的后端/前端组件
PRODUCT_BACKEND := sub_kernel security oipes
PRODUCT_FRONTEND := ui

# 默认设备（可在命令行覆盖）
PRODUCT_DEFAULT_DEVICE := reference-x86_64
DEVICE ?= $(PRODUCT_DEFAULT_DEVICE)

# 产品输出路径段
PRODUCT_OUT := $(OUT_DIR)/$(PRODUCT_NAME)

# 内核 defconfig 选择
KERNEL_DEFCONFIG := servecosys_defconfig

# 产品特性开关
PRODUCT_FEATURES := fingerprint dual-domain permission-ladder snapshot

# 展示信息
define PRODUCT_BANNER
=============================================
 产品: $(PRODUCT_NAME)
 型号: $(PRODUCT_MODEL)
 设备: $(DEVICE)
 特性: $(PRODUCT_FEATURES)
=============================================
endef
