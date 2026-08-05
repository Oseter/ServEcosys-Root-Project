#
# ServEcosys Root Project - 顶层统一构建入口
#
# 用法:
#   make help                          # 查看所有目标
#   make all                           # 构建默认产品 (servecosys_qemu)
#   make PRODUCT=servecosys_pc all     # 构建指定产品
#   make bootloader                    # 仅编译 UEFI 引导程序
#   make kernel                        # 仅编译内核
#   make modules                       # 仅编译内核模块 (guard/pc/mobile/probe)
#   make initramfs                     # 组装 initramfs
#   make image                         # 生成可启动磁盘镜像
#   make qemu                          # QEMU 快速启动验证
#   make keys                          # 生成签名密钥
#   make clean                         # 清理构建产物
#
# 关键变量:
#   PRODUCT   = servecosys_pc | servecosys_mobile | servecosys_qemu
#   DEVICE    = qemu-x86_64 | reference-x86_64
#   LINUX_SRC = 内核源码树路径 (默认 kernel/linux-src)
#   OUT_DIR   = 构建输出目录 (默认 build)
#   ARCH      = x86_64 | arm64
#

# ---- 默认配置 ----
SHELL := /bin/bash
PRODUCT ?= servecosys_qemu
DEVICE  ?= qemu-x86_64

# 操作系统底层架构源码根（与工程组织并列，位于上层）
ROOT_PROJECT ?= ../root project
LINUX_SRC ?= $(ROOT_PROJECT)/kernel/linux-src
OUT_DIR ?= build

# 产品定义文件
PRODUCTS_DIR := products
DEVICES_DIR  := devices
-include $(PRODUCTS_DIR)/$(PRODUCT).mk

# ---- 内部变量 ----
KERNEL_MODULES := $(ROOT_PROJECT)/kernel/modules
KERNEL_CORE    := $(ROOT_PROJECT)/kernel/core
BOOT_DIR       := $(ROOT_PROJECT)/boot
INITRAMFS_DIR  := $(ROOT_PROJECT)/scripts/initramfs
QEMU_TEST      := $(ROOT_PROJECT)/scripts/qemu_test.sh
PATCHES_DIR    := $(ROOT_PROJECT)/kernel/patches

# ---- 帮助 ----
.PHONY: help
help:
	@echo "ServEcosys Root Project 构建系统"
	@echo ""
	@echo "用法: make [PRODUCT=servecosys_pc|servecosys_mobile|servecosys_qemu] [DEVICE=qemu-x86_64] <target>"
	@echo ""
	@echo "目标:"
	@echo "  all         构建默认产品全部产物 (内核+模块+initramfs+镜像)"
	@echo "  bootloader  仅编译 UEFI 引导程序"
	@echo "  kernel      仅编译 Linux 内核"
	@echo "  modules     仅编译内核模块 (guard/pc/mobile/probe)"
	@echo "  initramfs   组装 initramfs"
	@echo "  image       生成可启动磁盘镜像"
	@echo "  qemu        在 QEMU 中启动验证"
	@echo "  keys        生成签名密钥"
	@echo "  sign        对内核/引导程序签名"
	@echo "  clean       清理构建产物"
	@echo ""
	@echo "变量:"
	@echo "  PRODUCT   产品名   (默认 servecosys_qemu)"
	@echo "  DEVICE    设备名   (默认 qemu-x86_64)"
	@echo "  LINUX_SRC 内核源码树 (默认 kernel/linux-src)"
	@echo "  OUT_DIR   输出目录 (默认 build)"

# ---- 检查内核源码存在性 ----
.PHONY: check-kernel-src
check-kernel-src:
	@if [ ! -d "$(LINUX_SRC)" ]; then \
		echo "[ERROR] 内核源码树不存在: $(LINUX_SRC)"; \
		echo "  请先运行: make kernel-src  或  git clone https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git $(LINUX_SRC)"; \
		exit 1; \
	fi

# ---- 内核源码获取 ----
KERNEL_VER ?= 6.6
.PHONY: kernel-src
kernel-src:
	@echo "[STEP] 获取 Linux $(KERNEL_VER) LTS 源码..."
	@if [ ! -d "$(LINUX_SRC)" ]; then \
		mkdir -p "$(dir $(LINUX_SRC))"; \
		git clone --depth 1 --branch "v$(KERNEL_VER).y" \
			https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git "$(LINUX_SRC)"; \
		if [ ! -d "$(LINUX_SRC)" ]; then \
			echo "[WARN] git 克隆失败，尝试 tarball 下载..."; \
			wget -c "https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$(KERNEL_VER).tar.xz" -O /tmp/linux-$(KERNEL_VER).tar.xz && \
			tar -xJf /tmp/linux-$(KERNEL_VER).tar.xz -C "$(dir $(LINUX_SRC))" && \
			mv "$(dir $(LINUX_SRC))linux-$(KERNEL_VER)" "$(LINUX_SRC)"; \
		fi; \
	fi
	@echo "[OK] 内核源码就绪: $(LINUX_SRC)"

# ---- 应用内核补丁 (guard/pc/mobile/probe 集成) ----
.PHONY: kernel-patch
kernel-patch: check-kernel-src
	@echo "[STEP] 应用 ServEcosys 内核补丁..."
	@$(ROOT_PROJECT)/scripts/apply_kernel_patch.sh "$(LINUX_SRC)" "$(PATCHES_DIR)" || { \
		echo "[WARN] 补丁应用失败（源码版本不匹配时正常），继续..."; \
	}
	@echo "[OK] 内核补丁处理完成"

# ---- 编译 bootloader ----
.PHONY: bootloader
bootloader:
	@echo "[STEP] 编译 UEFI 引导程序..."
	@$(MAKE) -C "$(BOOT_DIR)" ARCH=$(BOOT_ARCH)
	@echo "[OK] bootloader 完成: $(BOOT_DIR)/bootloader.efi"

# ---- 编译内核 ----
.PHONY: kernel
kernel: check-kernel-src kernel-patch
	@echo "[STEP] 配置内核 ($(KERNEL_DEFCONFIG))..."
	@cp "$(KERNEL_CORE)/servecosys_defconfig" "$(LINUX_SRC)/.config"
	@$(MAKE) -C "$(LINUX_SRC)" olddefconfig
	@echo "[STEP] 编译内核 bzImage..."
	@$(MAKE) -C "$(LINUX_SRC)" -j$$(nproc) bzImage
	@mkdir -p "$(OUT_DIR)"
	@cp "$(LINUX_SRC)/arch/x86/boot/bzImage" "$(OUT_DIR)/vmlinuz"
	@echo "[OK] 内核完成: $(OUT_DIR)/vmlinuz"

# ---- 编译内核模块（仅产品指定的模块集） ----
.PHONY: modules
modules: check-kernel-src
	@echo "[STEP] 编译内核模块 (产品: $(PRODUCT), 模块集: $(PRODUCT_MODULES))..."
	@for m in $(PRODUCT_MODULES); do \
		if [ -f "$(KERNEL_MODULES)/$$m/Makefile" ]; then \
			echo "  -> $$m"; \
			$(MAKE) -C "$(LINUX_SRC)" M="$(KERNEL_MODULES)/$$m" modules || exit 1; \
		else \
			echo "  [WARN] 模块 $$m 无 Makefile，跳过"; \
		fi; \
	done
	@mkdir -p "$(OUT_DIR)/modules"
	@find "$(KERNEL_MODULES)" -name '*.ko' -exec cp {} "$(OUT_DIR)/modules/" \;
	@echo "[OK] 内核模块完成: $(OUT_DIR)/modules/"

# ---- 组装 initramfs ----
.PHONY: initramfs
initramfs:
	@echo "[STEP] 组装 initramfs..."
	@$(ROOT_PROJECT)/scripts/build_initramfs.sh "$(OUT_DIR)" "$(INITRAMFS_DIR)"
	@echo "[OK] initramfs 完成: $(OUT_DIR)/initramfs.cpio.gz"

# ---- 生成可启动磁盘镜像 ----
.PHONY: image
image: kernel initramfs
	@echo "[STEP] 生成磁盘镜像..."
	@$(ROOT_PROJECT)/scripts/build_image.sh "$(OUT_DIR)" "$(DEVICE)"
	@echo "[OK] 镜像完成: $(OUT_DIR)/servecosys.img"

# ---- QEMU 启动验证 ----
.PHONY: qemu
qemu: image
	@echo "[STEP] QEMU 启动验证..."
	@$(QEMU_TEST) -k "$(OUT_DIR)/vmlinuz" -i "$(OUT_DIR)/initramfs.cpio.gz"

# ---- 密钥与签名 ----
.PHONY: keys
keys:
	@$(ROOT_PROJECT)/scripts/generate_keys.sh

.PHONY: sign
sign: keys
	@echo "[STEP] 签名内核镜像..."
	@if [ -f "$(OUT_DIR)/vmlinuz" ]; then \
		openssl dgst -sha256 -sign scripts/keys/maintainer.key \
			-out "$(OUT_DIR)/vmlinuz.sig" "$(OUT_DIR)/vmlinuz" && \
		echo "[OK] 签名完成: $(OUT_DIR)/vmlinuz.sig"; \
	else \
		echo "[WARN] 内核镜像不存在，跳过签名"; \
	fi

# ---- 默认构建 ----
.PHONY: all
all: kernel modules initramfs
	@echo ""
	@echo "============================================="
	@echo " ServEcosys 构建完成 (产品: $(PRODUCT), 设备: $(DEVICE))"
	@echo " 产物目录: $(OUT_DIR)/"
	@echo "============================================="

# ---- 清理 ----
.PHONY: clean
clean:
	@echo "[STEP] 清理构建产物..."
	@rm -rf "$(OUT_DIR)"
	@$(MAKE) -C "$(BOOT_DIR)" clean 2>/dev/null || true
	@for m in guard probe pc mobile; do \
		$(MAKE) -C "$(KERNEL_MODULES)/$$m" clean 2>/dev/null || true; \
	done
	@echo "[OK] 清理完成"
