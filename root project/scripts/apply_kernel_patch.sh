#!/bin/bash
#
# ServEcosys Kernel Patch - 将内核中央/模块集成进内核源码树
#
# 职责：
#   1. 把 kernel/core 加入内核源码树的递归构建链（Kconfig + Makefile）
#   2. 使 CONFIG_SERVECOSYS_* 生效可被引用
#   3. 通过补丁目录 kernel/patches/ 应用额外的源码级补丁
#
# 用法:
#   ./scripts/apply_kernel_patch.sh <LINUX_SRC> [PATCHES_DIR]
#
# 说明：
#   本脚本以"源码树内目录注入"方式工作，不覆盖内核自带文件，
#   而是建立并挂接 kernel/core 到层级构建链，保证可重复执行。
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

LINUX_SRC="${1:?用法: $0 <LINUX_SRC> [PATCHES_DIR]}"
PATCHES_DIR="${2:-$PROJECT_ROOT/kernel/patches}"

KERNEL_CORE_SRC="$PROJECT_ROOT/kernel/core"
KERNEL_MODULES_SRC="$PROJECT_ROOT/kernel/modules"

echo "[STEP] 集成内核中央与模块到源码树: $LINUX_SRC"

if [ ! -d "$LINUX_SRC" ]; then
    echo "[ERROR] 内核源码树不存在: $LINUX_SRC"
    exit 1
fi

CORE_DST="$LINUX_SRC/kernel/servecosys"

# ---- 1. 同步内核中央源码 ----
echo "  [1/5] 同步内核中央源码 -> $CORE_DST"
mkdir -p "$CORE_DST"
cp "$KERNEL_CORE_SRC"/{main.c,sched.c,mm.c,net.c,core.h} "$CORE_DST/"
cp "$KERNEL_CORE_SRC"/{Kconfig,Makefile} "$CORE_DST/"

# ---- 2. 同步内核模块源码 ----
echo "  [2/5] 同步内核模块源码 -> $CORE_DST/modules"
mkdir -p "$CORE_DST/modules"
for m in guard probe pc mobile; do
    [ -d "$KERNEL_MODULES_SRC/$m" ] || continue
    mkdir -p "$CORE_DST/modules/$m"
    cp "$KERNEL_MODULES_SRC/$m"/*.[ch] "$CORE_DST/modules/$m/" 2>/dev/null || true
    cp "$KERNEL_MODULES_SRC/$m"/{Makefile,Kconfig} "$CORE_DST/modules/$m/" 2>/dev/null || true
done
cp "$KERNEL_MODULES_SRC/Kconfig" "$CORE_DST/modules/Kconfig"
cp "$KERNEL_MODULES_SRC/Makefile" "$CORE_DST/modules/Makefile"

# ---- 3. 挂接 Kconfig（source 进顶层 Kconfig，真正被 kconfig 读取的入口） ----
# 注意：Linux 顶层 Kconfig 并不 source "kernel/Kconfig"（该文件不存在），
#       必须挂到顶层 Kconfig 末尾。
KERNEL_KCONFIG="$LINUX_SRC/Kconfig"
if [ ! -f "$KERNEL_KCONFIG" ]; then
    echo "  [3/5] [ERROR] 顶层 Kconfig 不存在: $KERNEL_KCONFIG"
    exit 1
fi
if ! grep -q "kernel/servecosys/Kconfig" "$KERNEL_KCONFIG" 2>/dev/null; then
    echo "  [3/5] 挂接内核中央 Kconfig -> 顶层 Kconfig"
    echo 'source "kernel/servecosys/Kconfig"' >> "$KERNEL_KCONFIG"
else
    echo "  [3/5] 顶层 Kconfig 已挂接，跳过"
fi

# 在内核中央 Kconfig 中挂接模块 Kconfig 入口
CORE_KCONFIG_DST="$CORE_DST/Kconfig"
if ! grep -q "kernel/servecosys/modules/Kconfig" "$CORE_KCONFIG_DST" 2>/dev/null; then
    echo 'source "kernel/servecosys/modules/Kconfig"' >> "$CORE_KCONFIG_DST"
fi

# ---- 4. 挂接 Makefile（递归构建子目录） ----
KERNEL_MAKEFILE="$LINUX_SRC/kernel/Makefile"
if ! grep -q "obj-\$(CONFIG_SERVECOSYS_CORE)" "$KERNEL_MAKEFILE" 2>/dev/null; then
    echo "  [4/5] 挂接内核中央 Makefile"
    echo 'obj-$(CONFIG_SERVECOSYS_CORE) += servecosys/' >> "$KERNEL_MAKEFILE"
else
    echo "  [4/5] 内核中央 Makefile 已挂接，跳过"
fi

# ---- 5. 应用额外补丁 ----
echo "  [5/5] 应用补丁目录: $PATCHES_DIR"
if [ -d "$PATCHES_DIR" ]; then
    for p in "$PATCHES_DIR"/*.patch; do
        [ -e "$p" ] || continue
        echo "    applying $(basename "$p")"
        git -C "$LINUX_SRC" apply "$p" 2>/dev/null || patch -p1 -d "$LINUX_SRC" < "$p"
    done
else
    echo "    (补丁目录为空，跳过)"
fi

echo "[OK] 内核中央与模块集成完成"

# ---- 6. 诊断输出：确认同步与挂接确实生效 ----
echo "[DIAG] 同步结果（kernel/servecosys 目录树）:"
find "$CORE_DST" -maxdepth 3 -type f | sed "s|$LINUX_SRC/||" | sort
echo "[DIAG] 顶层 Kconfig 末尾 3 行:"
tail -n 3 "$KERNEL_KCONFIG"
echo "[DIAG] kernel/servecosys/Kconfig 末尾 2 行:"
tail -n 2 "$CORE_KCONFIG_DST"
echo "[DIAG] kernel/Makefile 末尾 2 行:"
tail -n 2 "$KERNEL_MAKEFILE"