#
# ServEcosys Root Project - 统一构建系统
#
# 用法:
#   make help                          # 查看所有目标
#   make full                          # 一键构建 (密钥+内核+模块+initramfs+ISO)
#   make clean                         # 清理所有构建
#
# 关键参数:
#   PRODUCT   = servecosys_pc | servecosys_mobile | servecosys_qemu
#   DEVICE    = qemu-x86_64 | reference-x86_64
#   LINUX_SRC = 内核源码路径 (默认 ../kernel-src, 无空格路径)
#   OUT_DIR   = 输出目录 (默认 ../root project/build/$(PRODUCT)，按产品分目录)
#   ARCH      = x86_64 | arm64
#

# ---- 默认参数 ----
SHELL := /bin/bash
PRODUCT ?= servecosys_qemu
DEVICE  ?= qemu-x86_64

# 系统底座（源目录与工程组织协调，位于上层）
ROOT_PROJECT ?= ../root project
REPO_ROOT    ?= ..
# 内核源码（不能含空格的路径，Linux kbuild 拒绝在含空格的目录下编译）
LINUX_SRC ?= $(REPO_ROOT)/kernel-src
# 输出目录统一到 root project/build/$(PRODUCT)：按产品分目录，互不干扰
OUT_DIR ?= $(ROOT_PROJECT)/build/$(PRODUCT)

# 产品定义文件
PRODUCTS_DIR := products
DEVICES_DIR  := devices
-include $(PRODUCTS_DIR)/$(PRODUCT).mk

# ---- 内部路径 ----
KERNEL_MODULES := $(ROOT_PROJECT)/kernel/modules
KERNEL_CORE    := $(ROOT_PROJECT)/kernel/core
BOOT_DIR       := $(ROOT_PROJECT)/boot
INITRAMFS_DIR  := $(ROOT_PROJECT)/scripts/initramfs
QEMU_TEST      := $(ROOT_PROJECT)/scripts/qemu_test.sh
PATCHES_DIR    := $(ROOT_PROJECT)/kernel/patches

# ---- 目标 ----
.PHONY: help
help:
	@echo "ServEcosys Root Project 构建系统"
	@echo ""
	@echo "用法: make [PRODUCT=servecsys_pc|servecsys_mobile|servecsys_qemu] [DEVICE=qemu-x86_64] <target>"
	@echo ""
	@echo "目标:"
	@echo "  full       一键构建 (密钥+内核+模块+initramfs+ISO)"
	@echo "  all        构建内核+模块+initramfs (不含 ISO)"
	@echo "  bootloader 构建 UEFI 引导加载器"
	@echo "  kernel     构建 Linux 内核"
	@echo "  modules    按产品模块集构建内核模块 (guard/probe/pc/mobile)"
	@echo "  initramfs  打包 initramfs"
	@echo "  iso        生成可启动 ISO 镜像"
	@echo "  qemu       用 QEMU 启动验证"
	@echo "  kernel-src 获取 Linux 内核源码"
	@echo "  keys       生成签名密钥"
	@echo "  sign       给内核签名"
	@echo "  clean      清理所有构建"
	@echo ""
	@echo "参数:"
	@echo "  PRODUCT  产品名   (默认 servecosys_qemu)"
	@echo "  DEVICE   设备名   (默认 qemu-x86_64)"
	@echo "  LINUX_SRC 内核源码路径 (默认 ../kernel-src, 无空格)"
	@echo "  OUT_DIR  输出目录 (默认 ../root project/build/$(PRODUCT))"

# ---- 检查内核源码 ----
.PHONY: check-kernel-src
check-kernel-src:
	@if [ ! -d "$(LINUX_SRC)" ]; then \
		echo "[ERROR] 内核源码不存在: $(LINUX_SRC)"; \
		echo "  请先运行: make kernel-src"; \
		exit 1; \
	fi

# ---- 内核源码获取 ----
KERNEL_VER ?= 6.6
.PHONY: kernel-src
kernel-src:
	@echo "[STEP] 获取 Linux $(KERNEL_VER) LTS 源码..."
	@if [ ! -d "$(LINUX_SRC)" ]; then \
		mkdir -p "$(dir $(LINUX_SRC))"; \
		git clone --depth 1 --branch "linux-$(KERNEL_VER).y" \
			https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git "$(LINUX_SRC)"; \
		if [ ! -d "$(LINUX_SRC)" ]; then \
			echo "[WARN] git 克隆失败，改用 tarball 方式..."; \
			wget -c "https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$(KERNEL_VER).tar.xz" -O /tmp/linux-$(KERNEL_VER).tar.xz && \
			tar -xJf /tmp/linux-$(KERNEL_VER).tar.xz -C "$(dir $(LINUX_SRC))" && \
			mv "$(dir $(LINUX_SRC))linux-$(KERNEL_VER)" "$(LINUX_SRC)"; \
		fi; \
	fi
	@echo "[OK] 内核源码就绪: $(LINUX_SRC)"

# ---- 应用内核补丁 (guard/pc/mobile/probe 等) ----
.PHONY: kernel-patch
kernel-patch: check-kernel-src
	@echo "[STEP] 应用 ServEcosys 内核补丁..."
	@"$(ROOT_PROJECT)/scripts/apply_kernel_patch.sh" "$(LINUX_SRC)" "$(PATCHES_DIR)" || { \
		echo "[WARN] 补丁应用失败，源码版本不匹配时继续构建..."; \
	}
	@echo "[OK] 内核补丁应用完毕"

# ---- 构建 bootloader ----
.PHONY: bootloader
bootloader:
	@echo "[STEP] 构建 UEFI 引导加载器..."
	@$(MAKE) -C "$(BOOT_DIR)" ARCH=$(BOOT_ARCH)
	@echo "[OK] bootloader 输出: $(BOOT_DIR)/bootloader.efi"

# ---- 构建内核 ----
.PHONY: kernel
kernel: kernel-src check-kernel-src kernel-patch
	@echo "[STEP] 构建内核 ($(KERNEL_DEFCONFIG))..."
	@cp "$(KERNEL_CORE)/servecosys_defconfig" "$(LINUX_SRC)/.config"
	@$(MAKE) -C "$(LINUX_SRC)" olddefconfig
	@echo "  [DIAG] .config 中的 SERVECOSYS 符号（若未配置）:"
	@grep -E '^#? ?CONFIG_SERVECOSYS' "$(LINUX_SRC)/.config" || true
	@test "$$(grep -c '^CONFIG_SERVECOSYS_CORE=y' "$(LINUX_SRC)/.config")" -ge 1 || { \
		echo "[ERROR] CONFIG_SERVECOSYS_CORE 未配置。Kconfig 交互未生效？请查看上方 apply_kernel_patch 输出"; \
		exit 1; \
	}
	@echo "[STEP] 构建内核 bzImage..."
	@$(MAKE) -C "$(LINUX_SRC)" -j$$(nproc) bzImage
	@mkdir -p "$(OUT_DIR)"
	@cp "$(LINUX_SRC)/arch/x86/boot/bzImage" "$(OUT_DIR)/vmlinuz"
	@echo "[OK] 内核输出: $(OUT_DIR)/vmlinuz"

# ---- 按产品模块集构建内核模块（guard 为内建进内核，probe/pc/mobile 为可加载 .ko）----
# 模块目录: kernel/servecosys/modules/<guard|probe|pc|mobile>/
# PRODUCT_MODULES 决定收集哪些模块（如 qemu: guard probe pc / mobile: guard probe mobile）
.PHONY: modules
modules: kernel
	@echo "[STEP] Build kernel modules (product: $(PRODUCT), set: $(PRODUCT_MODULES))..."
	@$(MAKE) -C "$(LINUX_SRC)" -j$$(nproc) modules || exit 1
	@rm -rf "$(OUT_DIR)/modules"; mkdir -p "$(OUT_DIR)/modules"
	@for m in $(PRODUCT_MODULES); do \
		if [ -d "$(LINUX_SRC)/kernel/servecosys/modules/$$m" ]; then \
			find "$(LINUX_SRC)/kernel/servecosys/modules/$$m" -name '*.ko' -exec cp {} "$(OUT_DIR)/modules/" \; 2>/dev/null; \
		fi; \
	done
	@n=$$(find "$(OUT_DIR)/modules" -name '*.ko' 2>/dev/null | wc -l); \
	echo "[OK] kernel modules for $(PRODUCT): $(OUT_DIR)/modules/ ($$n .ko, set: $(PRODUCT_MODULES))"; \
	if [ "$$n" -eq 0 ]; then \
		echo "[WARN] no .ko in this product module set (guard may be built-in only)"; \
	fi

# ---- 打包 initramfs ----
.PHONY: initramfs
initramfs:
	@echo "[STEP] 打包 initramfs (product: $(PRODUCT), set: $(PRODUCT_MODULES))..."
	@"$(ROOT_PROJECT)/scripts/build_initramfs.sh" "$(OUT_DIR)" "$(INITRAMFS_DIR)" "$(PRODUCT_MODULES)" "$(PRODUCT)"
	@echo "[OK] initramfs 输出: $(OUT_DIR)/initramfs.cpio.gz"

# ---- 构建 SED 后端 .smle 服务（backend/sub_kernel）----
# 依赖库 backend/deps 编译到 $(OUT_DIR)/backend-deps，.smle 输出到 $(OUT_DIR)/sed/
SED_DIR     := $(ROOT_PROJECT)/backend/sub_kernel
SED_DEPS_BUILD := $(OUT_DIR)/backend-deps
SED_OUT     := $(OUT_DIR)/sed
.PHONY: sed sed-deps
sed-deps:
	@echo "[STEP] Build SED dependency libraries (.a)..."
	@$(MAKE) -C "$(ROOT_PROJECT)/backend/deps" all OUTPUT_DIR="$(SED_DEPS_BUILD)" || exit 1
	@echo "[OK] SED deps -> $(SED_DEPS_BUILD)"

sed: sed-deps
	@echo "[STEP] Build SED backend daemons (.smle, $(PRODUCT))..."
	@$(MAKE) -C "$(SED_DIR)" all OUTPUT_DIR="$(SED_OUT)" DEPS_BUILD="$(SED_DEPS_BUILD)" || exit 1
	@echo "[OK] SED services: $(SED_OUT)/*.smle"

# ---- 编译 SELinux 策略（backend/security -> servecosys.pp）----
SELINUX_DIR := $(ROOT_PROJECT)/backend/security
SELINUX_OUT := $(OUT_DIR)/selinux
.PHONY: selinux-policy
selinux-policy:
	@echo "[STEP] Build SELinux policy (servecosys.pp)..."
	@$(MAKE) -C "$(SELINUX_DIR)" all OUTPUT_DIR="$(SELINUX_OUT)" || { \
		echo "[WARN] checkpolicy/checkmodule not installed? run: apt install selinux-policy-devel"; \
		exit 1; \
	}
	@test -f "$(SELINUX_OUT)/servecosys.pp" || { echo "[ERROR] servecosys.pp missing"; exit 1; }
	@echo "[OK] SELinux policy: $(SELINUX_OUT)/servecosys.pp"

# ---- 构建 UID 前端 (.ssle) ----
UI_DIR := $(ROOT_PROJECT)/frontend/ui
UID_OUT := $(OUT_DIR)/uid
.PHONY: uid
uid:
	@echo "[STEP] Build UID frontend daemons (.ssle)..."
	@$(MAKE) -C "$(UI_DIR)" all OUTPUT_DIR="$(UID_OUT)" || { \
		echo "  [WARN] UID .ssle build skipped/failed (display may be limited)"; \
	}
	@echo "[OK] UID frontend: $(UID_OUT)/*.ssle (if buildable)"

# ---- QEMU 启动验证 ----
QEMU_ARCH ?= x86_64
.PHONY: qemu
qemu: iso
	@echo "[STEP] QEMU 启动验证..."
	@ARCH="$(QEMU_ARCH)" BUILD_DIR="$(OUT_DIR)" "$(QEMU_TEST)" -a "$(QEMU_ARCH)"

# ---- 一键构建（含密钥 + 内核 + 模块 + SED + 策略 + initramfs + ISO）----
.PHONY: full
full: keys kernel modules sed selinux-policy initramfs iso
	@echo ""
	@echo "============================================="
	@echo " ServEcosys 构建完成"
	@echo " 产品: $(PRODUCT) | 设备: $(DEVICE)"
	@echo " ISO:          $(OUT_DIR)/ConceptOS.iso"
	@echo " 内核:         $(OUT_DIR)/vmlinuz"
	@echo " 模块:         $(OUT_DIR)/modules/"
	@echo " SED服务:      $(OUT_DIR)/sed/*.smle"
	@echo " SELinux策略:  $(OUT_DIR)/selinux/servecosys.pp"
	@echo " 模块集:       $(PRODUCT_MODULES)"
	@echo " 启动验证:     make qemu"
	@echo "============================================="

# ---- 密钥与签名 ----
.PHONY: keys
keys:
	@"$(ROOT_PROJECT)/scripts/generate_keys.sh" --force

.PHONY: sign
sign: keys
	@echo "[STEP] 签名内核镜像..."
	@if [ -f "$(OUT_DIR)/vmlinuz" ]; then \
		openssl dgst -sha256 -sign "$(ROOT_PROJECT)/scripts/keys/maintainer.key" \
			-out "$(OUT_DIR)/vmlinuz.sig" "$(OUT_DIR)/vmlinuz" && \
		echo "[OK] 签名输出: $(OUT_DIR)/vmlinuz.sig"; \
	else \
		echo "[WARN] 内核镜像不存在，跳过签名"; \
	fi

# ---- 默认构建 ----
.PHONY: all
all: kernel modules initramfs
	@echo ""
	@echo "============================================="
	@echo " ServEcosys 构建完成 (产品: $(PRODUCT), 设备: $(DEVICE))"
	@echo " 输出目录: $(OUT_DIR)/"
	@echo " 构建完整产品请运行: make full"
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
	@echo "[OK] 清理完毕"
