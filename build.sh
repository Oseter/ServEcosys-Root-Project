#!/bin/bash
#
# 概念OS (ConceptOS) - ServEcosys 系列系统标准构建脚本
# 一键构建脚本
#
# 用法:
#   ./build.sh         安装依赖并构建 ISO
#   ./build.sh qemu    构建后直接启动 QEMU 验证
#   ./build.sh clean   清理构建产物
#
# 要求: Ubuntu (VMware/真机均可), 有 sudo 权限
#

set -e

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_ENG="$ROOT/build engineering"
SETUP_SH="$ROOT/root project/scripts/setup_ubuntu.sh"

MODE="${1:-build}"
PRODUCT="${PRODUCT:-servecsys_qemu}"

echo "============================================="
echo " 概念OS (ConceptOS) 一键构建"
echo " 模式: $MODE | 产品: $PRODUCT"
echo "============================================="

if [ ! -d "$BUILD_ENG" ]; then
    echo "[ERROR] 未找到 build engineering，请确认在仓库根目录运行: $ROOT"
    exit 1
fi

case "$MODE" in
    clean)
        cd "$BUILD_ENG"
        make clean
        echo "[OK] 清理完成"
        exit 0
        ;;
    build|qemu)
        ;;
    *)
        echo "用法: $0 [build|qemu|clean]"
        exit 1
        ;;
esac

# 1) 安装构建依赖（幂等，可重复运行，安全）
echo ""
echo "[STEP 1/3] 检查并安装构建依赖..."
if [ "$(id -u)" -ne 0 ]; then
    sudo "$SETUP_SH"
else
    "$SETUP_SH"
fi

# 2) 一键构建（内核 + 模块 + initramfs + 可启动 ISO）
echo ""
echo "[STEP 2/3] 构建系统 (内核 + 模块 + initramfs + ISO)..."
cd "$BUILD_ENG"
make full PRODUCT="$PRODUCT"

# 3) 输出
ISO="$ROOT/root project/build/$PRODUCT/ConceptOS.iso"
echo ""
echo "============================================="
echo " 构建完成!"
echo " 产品 ISO: $ISO"
echo "============================================="

if [ "$MODE" = "qemu" ]; then
    echo ""
    echo "[STEP 3/3] 启动 QEMU 验证..."
    echo "  退出 QEMU: Ctrl+A 然后 X"
    echo ""
    make qemu PRODUCT="$PRODUCT"
fi

echo ""
echo "[OK] 概念OS 构建完成"
