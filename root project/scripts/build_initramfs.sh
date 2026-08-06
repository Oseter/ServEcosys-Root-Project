#!/bin/bash
#
# ServEcosys - 组装 initramfs
#
# 用法:
#   ./build_initramfs.sh <OUT_DIR> [INITRAMFS_SRC]
#
# 说明：
#   - 以 scripts/initramfs/ 为骨架，打入 /sbin/init
#   - 若存在已构建的内核模块，一并打入 /lib/modules/
#   - 若存在已构建的系统集成层（system/），一并打入
#   - 产物: <OUT_DIR>/initramfs.cpio.gz
#

set -e

OUT_DIR="${1:?用法: $0 <OUT_DIR> [INITRAMFS_SRC]}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
INITRAMFS_SRC="${2:-$SCRIPT_DIR/initramfs}"
STAGING="$(mktemp -d)"

echo "[initramfs] 组装 initramfs..."
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"   # make OUT_DIR absolute so subshell cd (cpio/gzip) cannot break it

# 1. 复制骨架
cp -a "$INITRAMFS_SRC/." "$STAGING/"
chmod +x "$STAGING/init" 2>/dev/null || true
mkdir -p "$STAGING/bin" "$STAGING/sbin" "$STAGING/lib/modules" "$STAGING/dev" "$STAGING/proc" "$STAGING/sys" "$STAGING/tmp"

# 1.1 嵌入静态 busybox（提供 sh/mount/cp/switch_root 等必备工具）
BUSYBOX_BIN="$(command -v busybox 2>/dev/null || echo /bin/busybox)"
if [ -f "$BUSYBOX_BIN" ]; then
    echo "[initramfs]   嵌入 busybox: $BUSYBOX_BIN"
    cp -L "$BUSYBOX_BIN" "$STAGING/bin/busybox"
    chmod +x "$STAGING/bin/busybox"
    ( cd "$STAGING/bin" && ln -sf busybox sh mount mountpoint cp mv ln mkdir mknod cat grep cut sed ls rm echo test sleep setsid cttyhack switch_root chroot modprobe insmod rmmod poweroff reboot dmesg find hexdump readlink readahead 2>/dev/null || "$STAGING/bin/busybox" --install -s "$STAGING/bin" )
    # /sbin/init 保留骨架 init；busybox init 需要的 applets 也放 /sbin
    ( cd "$STAGING/sbin" && for a in init switch_root modprobe; do ln -sf ../bin/busybox "$a" 2>/dev/null || true; done )
else
    echo "[initramfs]   [WARN] 未找到 busybox，initramfs 将缺少用户态工具"
fi

# 1.2 骨架根目录符号链接（/sbin/init -> ../bin/... 由 init 内部处理）

# 2. 打入系统集成层（live OS 模式依赖 /system）
if [ -d "$PROJECT_ROOT/system" ]; then
    echo "[initramfs]   集成 system/ 层"
    cp -a "$PROJECT_ROOT/system" "$STAGING/system"
    chmod +x "$STAGING/system/sysinit" "$STAGING/system/"*.sh 2>/dev/null || true
fi

# 3. 打入已构建的内核模块
if [ -d "$OUT_DIR/modules" ]; then
    echo "[initramfs]   集成内核模块"
    cp -a "$OUT_DIR/modules/." "$STAGING/lib/modules/"
fi

# 4. 打包
( cd "$STAGING" && find . | cpio -H newc -o 2>/dev/null | gzip > "$OUT_DIR/initramfs.cpio.gz" )

rm -rf "$STAGING"
echo "[initramfs] 完成: $OUT_DIR/initramfs.cpio.gz"
