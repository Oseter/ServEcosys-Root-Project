#!/bin/bash
#
# ServEcosys - 打包 initramfs（产品驱动）
#
# 用法:
#   ./build_initramfs.sh <OUT_DIR> [INITRAMFS_SRC] [PRODUCT_MODULES] [PRODUCT]
#
# 说明:
#   - 以 scripts/initramfs/ 为骨架，组装 /sbin/init
#   - 嵌入静态 busybox，提供 sh/mount/switch_root 等必备工具
#   - 嵌入按产品过滤后的内核模块（来自 <OUT_DIR>/modules/，Makefile 已按 PRODUCT_MODULES 收集）
#   - 嵌入 system 再生层（system/）到 /system
#   - 生成 /etc/servecosys/product.conf：记录产品名与模块集，驱动 /init 的模块加载
#   - 输出: <OUT_DIR>/initramfs.cpio.gz
#

set -e

OUT_DIR="${1:?用法: $0 <OUT_DIR> [INITRAMFS_SRC] [PRODUCT_MODULES] [PRODUCT]}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
INITRAMFS_SRC="${2:-$SCRIPT_DIR/initramfs}"
PRODUCT_MODULES="${3:-guard probe}"
PRODUCT="${4:-servecsys_qemu}"
STAGING="$(mktemp -d)"

echo "[initramfs] 打包 initramfs (product: $PRODUCT, modules: $PRODUCT_MODULES)..."
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"   # make OUT_DIR absolute so subshell cd (cpio/gzip) cannot break it

# 1. 复制骨架
cp -a "$INITRAMFS_SRC/." "$STAGING/"
chmod +x "$STAGING/init" 2>/dev/null || true
mkdir -p "$STAGING/bin" "$STAGING/sbin" "$STAGING/lib/modules" "$STAGING/dev" "$STAGING/proc" "$STAGING/sys" "$STAGING/tmp" "$STAGING/etc/servecosys"

# 1.1 嵌入静态 busybox（提供 sh/mount/cp/switch_root 等必备工具）
BUSYBOX_BIN="$(command -v busybox 2>/dev/null || echo /bin/busybox)"
if [ -f "$BUSYBOX_BIN" ]; then
    echo "[initramfs]   嵌入 busybox: $BUSYBOX_BIN"
    cp -L "$BUSYBOX_BIN" "$STAGING/bin/busybox"
    chmod +x "$STAGING/bin/busybox"
    ( cd "$STAGING/bin" && for a in sh mount mountpoint cp mv ln mkdir mknod cat grep cut sed ls rm echo test sleep setsid cttyhack switch_root chroot modprobe insmod rmmod poweroff reboot dmesg find hexdump readlink readahead; do ln -sf busybox "$a"; done )
    # /sbin/init 指向骨架 init（busybox init 所需的 applets 也放 /sbin）
    ( cd "$STAGING/sbin" && for a in init switch_root modprobe; do ln -sf ../bin/busybox "$a" 2>/dev/null || true; done )
else
    echo "[initramfs]   [WARN] 未找到 busybox，initramfs 会缺少用户态工具"
fi

# 1.2 生成产品配置：product.conf 驱动 /init 按产品加载内核模块
{
    echo "# ServEcosys 产品配置（驱动 /init 的模块加载与启动流程）"
    echo "PRODUCT=$PRODUCT"
    echo "PRODUCT_MODULES=$PRODUCT_MODULES"
} > "$STAGING/etc/servecosys/product.conf"

# 2. 嵌入系统再生层（live OS 模式）到 /system
if [ -d "$PROJECT_ROOT/system" ]; then
    echo "[initramfs]   嵌入 system/ 层"
    cp -a "$PROJECT_ROOT/system" "$STAGING/system"
    chmod +x "$STAGING/system/sysinit" "$STAGING/system/"*.sh 2>/dev/null || true
fi

# 3. 嵌入已按产品过滤的内核模块（Makefile 的 modules 目标已收集到 <OUT_DIR>/modules/）
if [ -d "$OUT_DIR/modules" ]; then
    echo "[initramfs]   嵌入内核模块 (已按 $PRODUCT_MODULES 过滤)"
    cp -a "$OUT_DIR/modules/." "$STAGING/lib/modules/"
fi

# 3.5 嵌入 SED 后端 .smle 服务 + SELinux 策略（sysinit 在 /system/backend 下启动它们）
if [ -d "$OUT_DIR/sed" ] && [ -n "$(ls "$OUT_DIR/sed/"*.smle 2>/dev/null)" ]; then
    echo "[initramfs]   嵌入 SED 后端 .smle"
    mkdir -p "$STAGING/system/backend/bin"
    cp "$OUT_DIR/sed/"*.smle "$STAGING/system/backend/bin/"
    chmod +x "$STAGING/system/backend/bin/"*.smle 2>/dev/null || true
else
    echo "[initramfs]   [WARN] 未找到 $OUT_DIR/sed/*.smle，SED 后端将全部 DOWN"
fi

if [ -f "$OUT_DIR/selinux/servecosys.pp" ]; then
    echo "[initramfs]   嵌入 SELinux 策略 servecosys.pp"
    mkdir -p "$STAGING/system/backend/etc/selinux"
    cp "$OUT_DIR/selinux/servecosys.pp" "$STAGING/system/backend/etc/selinux/"
else
    echo "[initramfs]   [WARN] 未找到 $OUT_DIR/selinux/servecosys.pp"
fi

# 3.6 嵌入 UID 前端 .ssle
if [ -d "$OUT_DIR/uid" ] && [ -n "$(ls "$OUT_DIR/uid/"*.ssle 2>/dev/null)" ]; then
    echo "[initramfs]   嵌入 UID 前端 .ssle"
    mkdir -p "$STAGING/system/frontend/bin"
    cp "$OUT_DIR/uid/"*.ssle "$STAGING/system/frontend/bin/"
    chmod +x "$STAGING/system/frontend/bin/"*.ssle 2>/dev/null || true
fi

# 4. 打包
( cd "$STAGING" && find . | cpio -H newc -o 2>/dev/null | gzip > "$OUT_DIR/initramfs.cpio.gz" )

rm -rf "$STAGING"
echo "[initramfs] 输出: $OUT_DIR/initramfs.cpio.gz"

