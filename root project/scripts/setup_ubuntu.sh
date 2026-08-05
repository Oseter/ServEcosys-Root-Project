#!/bin/bash
#
# ServEcosys - Ubuntu 构建环境准备脚本
#
# 作用：在 Ubuntu/VMware Ubuntu 上安装构建一个可启动 ServEcosys 系统
#       所需的全部依赖（内核编译、initramfs、ISO 打包、QEMU 验证）。
#
# 用法:
#   sudo ./setup_ubuntu.sh
#
# 支持: Ubuntu 20.04/22.04/24.04 (x86_64)
#

set -e

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'
log_info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
log_step()  { echo -e "${BLUE}[STEP]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }

# 检查 root
if [ "$(id -u)" -ne 0 ]; then
    log_error "请用 root 运行: sudo ./setup_ubuntu.sh"
    exit 1
fi

echo "============================================="
echo " ServEcosys Ubuntu 构建环境准备"
echo "============================================="

log_step "更新 apt 索引..."
apt-get update

# ============ 构建工具链 ============
log_step "安装构建工具链 (gcc/make/binutils)..."
apt-get install -y \
    build-essential \
    gcc \
    make \
    binutils \
    bc \
    bison \
    flex \
    gawk \
    kmod \
    cpio \
    file \
    libelf-dev \
    libssl-dev \
    libncurses-dev \
    libfdt-dev \
    device-tree-compiler \
    rsync

# ============ git / 内核源码获取 ============
log_step "安装 git 与 wget..."
apt-get install -y git wget curl

# ============ initramfs ============
log_step "安装 initramfs / busybox 工具..."
apt-get install -y busybox-static

# ============ ISO 打包 ============
log_step "安装 ISO 打包工具 (xorriso/mtools/dosfstools)..."
apt-get install -y \
    xorriso \
    mtools \
    dosfstools \
    grub-pc-bin \
    grub-efi-amd64-bin \
    grub-common

# ============ QEMU 验证 ============
log_step "安装 QEMU 与 UEFI 固件..."
apt-get install -y \
    qemu-system-x86 \
    qemu-system-arm \
    ovmf

# ============ 签名 / 加密 ============
log_step "安装签名与加密工具..."
apt-get install -y openssl sbsigntool efitools

echo ""
echo "============================================="
log_info "构建环境准备完成！"
echo ""
echo "下一步构建方法（在项目根目录）:"
echo "  1. 获取内核源码:      make kernel-src"
echo "  2. 一键完整构建:      make full"
echo "  3. QEMU 启动验证:      make qemu"
echo "============================================="