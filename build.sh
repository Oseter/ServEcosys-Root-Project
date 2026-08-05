#!/bin/bash
#
# 概念OS (ConceptOS) - ServEcosys 系操作系统标准与概念呈现
# 一键构建脚本
#
# 用法:
#   ./build.sh          安装依赖并完整构建可启动 ISO
#   ./build.sh qemu     构建完成后直接在 QEMU 中启动验证
#   ./build.sh clean    清理构建产物
#
# 要求: Ubuntu (VMware/实体机均可), 需 sudo 权限
#

set -e

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_ENG="$ROOT/build engineering"
SETUP_SH="$ROOT/root project/scripts/setup_ubuntu.sh"

MODE="${1:-build}"

echo "============================================="
echo " 概念OS (ConceptOS) 一键构建"
echo " 模式: $MODE"
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

# 1) 安装构建依赖（仅首次需要，重复运行安全）
echo ""
echo "[STEP 1/3] 检查并安装构建依赖..."
if [ "$(id -u)" -ne 0 ]; then
    sudo "$SETUP_SH"
else
    "$SETUP_SH"
fi

# 2) 一键完整构建（内核源码 → 集成 → 编译 → initramfs → 可启动 ISO）
echo ""
echo "[STEP 2/3] 完整构建 (内核 + 模块 + initramfs + ISO)..."
cd "$BUILD_ENG"
make full

# 3) 完成
ISO="$ROOT/root project/build/ConceptOS.iso"
echo ""
echo "============================================="
echo " 构建完成!"
echo " 可启动 ISO: $ISO"
echo "============================================="

if [ "$MODE" = "qemu" ]; then
    echo ""
    echo "[STEP 3/3] 启动 QEMU 验证..."
    echo "  退出 QEMU: Ctrl+A 然后 X"
    echo ""
    make qemu
fi

echo ""
echo "[OK] 概念OS 构建完成"
