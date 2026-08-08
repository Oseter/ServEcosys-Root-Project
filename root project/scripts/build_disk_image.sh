#!/bin/bash
#
# ServEcosys 可安装磁盘镜像构建器 (ConceptOS.img)
#
# 借鉴 Ubuntu / Android 镜像布局：
#   - GPT 分区表
#     p1: EFI System Partition (FAT32, grub-x86_64-efi + vmlinuz + initramfs)
#     p2: Btrfs 根分区（顶层含子卷 @system / @data / @snapshots）
#   - 子卷：
#     @system    只读系统层（等价于 Ubuntu 的 @ 子卷）；initramfs 在 [5/7] 挂载
#     @data      用户/应用数据（UID 应用 + app 数据；镜像内为空）
#     @snapshots btrfs 快照目录（快照恢复由 system/snapshot.sh 管理）
#
# initramfs 的 [4/7] 会 btrfs 探测并在 [5/7] 挂载 @system（真实根）。
# 因此镜像可作为普通磁盘被 dd 到 USB/SATA/NVMe，或被 QEMU 直接引导，
# 实现"给人用的可安装镜像"（无需 /dev/loop + Windows 帮助）。
#
# 用法:
#   sudo PRODUCT=servecsys_qemu ./build_disk_image.sh [<输出路径>]
#   # CI 内由 Makefile 的 `make img` 触发（工作目录 build engineering/）
#
# 依赖: sgdisk(gdisk) mkfs.fat(dosfstools) mkfs.btrfs(btrfs-progs) grub-install
#

set -euo pipefail

# 需要 root（loop + mount + grub-install）
if [ "$(id -u)" -ne 0 ]; then
    if command -v sudo >/dev/null 2>&1; then
        echo "[img] not root, re-exec with sudo"
        exec sudo -E "$0" "$@"
    else
        echo "[img] ERROR: root required (loop devices + mount)"
        exit 1
    fi
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
PRODUCT="${PRODUCT:-servecsys_qemu}"
OUT_DIR="${OUT_DIR:-$PROJECT_ROOT/build/$PRODUCT}"
KERNEL="${KERNEL:-$OUT_DIR/vmlinuz}"
INITRAMFS="${INITRAMFS:-$OUT_DIR/initramfs.cpio.gz}"
IMG_OUT="${1:-$OUT_DIR/ConceptOS.img}"
IMG_SIZE_MB="${IMG_SIZE_MB:-2048}"
EFI_MB="256"
GRUB_CFG="$PROJECT_ROOT/boot/grub/grub.cfg"
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'
log_info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_step()  { echo -e "${BLUE}[STEP]${NC} $1"; }

check_deps() {
    local deps=("sgdisk" "mkfs.fat" "mkfs.btrfs" "btrfs" "losetup" "grub-install" "mount" "dd")
    local missing=()
    for dep in "${deps[@]}"; do
        command -v "$dep" >/dev/null 2>&1 || missing+=("$dep")
    done
    if [ ${#missing[@]} -ne 0 ]; then
        log_error "Missing dependencies: ${missing[*]}"
        log_info "Install: apt install -y gdisk dosfstools btrfs-progs grub-efi-amd64-bin grub-pc-bin"
        exit 1
    fi
}

cleanup() {
    local code=$?
    if [ -n "${LOOP:-}" ]; then
        local i
        for i in "$WORK/root" "$WORK/efi" "$WORK/top"; do
            mountpoint -q "$i" 2>/dev/null && umount "$i" 2>/dev/null || true
        done
        losetup -d "$LOOP" 2>/dev/null || true
    fi
    [ -d "${WORK:-}" ] && rm -rf "$WORK"
    exit $code
}

main() {
    log_step "ServEcosys 可安装磁盘镜像 (product: $PRODUCT)"

    if [ ! -f "$KERNEL" ]; then log_error "Kernel not found: $KERNEL"; exit 1; fi
    if [ ! -f "$INITRAMFS" ]; then log_error "Initramfs not found: $INITRAMFS"; exit 1; fi
    [ -f "$GRUB_CFG" ] || { log_error "GRUB config missing: $GRUB_CFG"; exit 1; }

    check_deps

    WORK="$(mktemp -d)"
    ROOT="$WORK/root"; EFI="$WORK/efi"; TOP="$WORK/top"
    mkdir -p "$ROOT" "$EFI" "$TOP"
    trap cleanup EXIT INT TERM

    rm -f "$IMG_OUT"
    log_step "创建稀疏磁盘镜像: $IMG_OUT (${IMG_SIZE_MB}M)"
    truncate -s "${IMG_SIZE_MB}M" "$IMG_OUT"

    log_step "写 GPT 分区表: p1=EFI(${EFI_MB}M) p2=Btrfs(余量)"
    sgdisk --zap-all "$IMG_OUT" >/dev/null
    sgdisk -n 1:0:+${EFI_MB}M -t 1:ef00 -c 1:"EFI System" "$IMG_OUT" >/dev/null
    sgdisk -n 2:0:0   -t 2:8300 -c 2:"ConceptOS Root" "$IMG_OUT" >/dev/null
    sgdisk -R -G "$IMG_OUT" >/dev/null 2>&1 || true

    log_step "挂载 loop 设备"
    LOOP="$(losetup -f -P --show "$IMG_OUT" 2>/dev/null || losetup -f --show "$IMG_OUT")"
    log_info "  loop: $LOOP"
    # 等待 partscan 就绪
    for i in $(seq 1 30); do
        [ -e "${LOOP}p1" ] && [ -e "${LOOP}p2" ] && break
        sleep 0.2
    done
    if [ ! -e "${LOOP}p2" ]; then
        partprobe "$LOOP" 2>/dev/null || true
        sleep 1
    fi
    [ -e "${LOOP}p1" ] || { log_error "partition nodes not ready (${LOOP}p1..p2)"; exit 1; }
    [ -e "${LOOP}p2" ] || { log_error "partition nodes not ready (${LOOP}p2)"; exit 1; }

    log_step "格式化为文件系统: EFI=fat32, Root=btrfs"
    mkfs.fat -F 32 -n "CEFI" "${LOOP}p1" >/dev/null
    mkfs.btrfs -f -L "ConceptOS" "${LOOP}p2" >/dev/null

    mount "${LOOP}p2" "$TOP"
    log_step "创建 Btrfs 子卷 @system @data @snapshots (Ubuntu/Android 风格)"
    btrfs subvolume create "$TOP/@system" >/dev/null
    btrfs subvolume create "$TOP/@data"   >/dev/null
    btrfs subvolume create "$TOP/@snapshots" >/dev/null
    # 默认子卷设为 @system，未指定 subvol 的 mount 也能得到根
    btrfs subvolume set-default "$TOP/@system" >/dev/null
    umount "$TOP"

    mount -o subvol=@system "${LOOP}p2" "$ROOT"
    mount "${LOOP}p1" "$EFI"

    log_step "填充 @system 根内容"
    # 1) 系统层（sysinit + 嵌入的 /system 层）
    if [ -d "$PROJECT_ROOT/system" ]; then
        cp -a "$PROJECT_ROOT/system/." "$ROOT/system/"
        chmod +x "$ROOT/system/sysinit" "$ROOT/system/"*.sh 2>/dev/null || true
    fi
    mkdir -p "$ROOT/bin" "$ROOT/sbin" "$ROOT/etc/servecosys" "$ROOT/boot/servecosys" "$ROOT/lib/modules" "$ROOT/dev" "$ROOT/proc" "$ROOT/sys" "$ROOT/tmp" "$ROOT/run"
    # 嵌入静态 busybox + applets（与 initramfs 一致的必需 subcommands）
    BUSYBOX_BIN="$(command -v busybox 2>/dev/null || echo /bin/busybox)"
    if [ -f "$BUSYBOX_BIN" ]; then
        cp -L "$BUSYBOX_BIN" "$ROOT/bin/busybox"
        chmod +x "$ROOT/bin/busybox"
        ( cd "$ROOT/bin" && for a in sh mount mountpoint cp mv ln mkdir mknod cat grep cut sed ls rm echo test sleep setsid cttyhack switch_root chroot modprobe insmod dmesg find readlink readahead hexdump; do ln -sf busybox "$a" 2>/dev/null || true; done )
        ( cd "$ROOT/sbin" && for a in init switch_root rmmod poweroff reboot; do ln -sf ../bin/busybox "$a" 2>/dev/null || true; done )
    else
        log_warn "  busybox not found, real root may lack a shell/init"
    fi
    # init 符号链接 -> /system/sysinit（同 live 布局）
    ln -sf /system/sysinit "$ROOT/sbin/init"
    # product.conf
    cat > "$ROOT/etc/servecosys/product.conf" <<EOF
# ServEcosys 产品配置
PRODUCT=$PRODUCT
EOF
    # 无 snapshot.conf：boot@system（init 中 [6/7] 无配置则用 @system）
    # daemons 放入 /system/backend... (与 initramfs 同样的部署)
    if [ -d "$OUT_DIR/sed" ] && [ -n "$(ls "$OUT_DIR/sed/"*.smle 2>/dev/null)" ]; then
        mkdir -p "$ROOT/system/backend/bin"
        cp "$OUT_DIR/sed/"*.smle "$ROOT/system/backend/bin/"
        chmod +x "$ROOT/system/backend/bin/"*.smle 2>/dev/null || true
    fi
    if [ -f "$OUT_DIR/selinux/servecosys.pp" ]; then
        mkdir -p "$ROOT/system/backend/etc/selinux"
        cp "$OUT_DIR/selinux/servecosys.pp" "$ROOT/system/backend/etc/selinux/"
    fi
    if [ -d "$OUT_DIR/uid" ] && [ -n "$(ls "$OUT_DIR/uid/"*.ssle 2>/dev/null)" ]; then
        mkdir -p "$ROOT/system/frontend/bin"
        cp "$OUT_DIR/uid/"*.ssle "$ROOT/system/frontend/bin/"
        chmod +x "$ROOT/system/frontend/bin/"*.ssle 2>/dev/null || true
    fi
    # 内核模块覆盖到根（modprobe 在真根仍可用）
    if [ -d "$OUT_DIR/modules" ]; then
        cp -a "$OUT_DIR/modules/." "$ROOT/lib/modules/"
    fi

    log_step "安装 GRUB 到 ESP 并放置引导文件"
    # movable 模式：把 BOOTX64.EFI 放到 EFI/BOOT（便于固件/OVMF 发现，无需 NV 配置项）
    # EFI 安装不传 device（x86_64-efi 仅需 --efi-directory 即可）
    grub-install --target=x86_64-efi --efi-directory="$EFI" --boot-directory="$EFI/boot" \
        --removable --no-floppy 2>&1 | tail -n 3 || {
        grub-install --target=x86_64-efi --efi-directory="$EFI" --boot-directory="$EFI/boot" \
            --removable --no-floppy --recheck 2>&1 | tail -n 3 || {
            log_error "grub-install 失败"; exit 1
        }
    }
    mkdir -p "$EFI/boot/grub"
    cp "$GRUB_CFG" "$EFI/boot/grub/grub.cfg"
    cp "$KERNEL" "$EFI/boot/vmlinuz"
    cp "$INITRAMFS" "$EFI/boot/initramfs.cpio.gz"

    sync

    log_step "卸载 + 释放 loop"
    umount "$ROOT" 2>/dev/null || true
    umount "$EFI"  2>/dev/null || true

    log_step "自检 btrfs（host 侧, detach 前）"
    log_info "  superblock copies (0/1/2/3, generation each):"
    for _sb in 0 1 2 3; do
        btrfs inspect-internal dump-super -s "$_sb" "${LOOP}p2" 2>/dev/null | grep -iE '^superblock:|^[[:space:]]*generation|^[[:space:]]*bytenr|^[[:space:]]*root[[:space:]]|^[[:space:]]*chunk_root[[:space:]]' | sed "s/^/    [super:$_sb] /" || true
        log_info ""
    done
    log_info "  SUPPORTED features (host btrfs-progs): $(btrfs inspect-internal dump-super "${LOOP}p2" 2>/dev/null | grep -i 'incompat_flags' | head -1)"
    log_info "  FS layout: $(btrfs inspect-internal dump-super "${LOOP}p2" 2>/dev/null | grep -iE 'sectorsize|nodesize|checksum' | sed 's/^ *//' | tr '\n' ';')"
    log_info "  csum type: $(btrfs inspect-internal dump-super "${LOOP}p2" 2>/dev/null | grep -iE '^checksum|^csum_type|csum_type' | head -1)"
    log_info "  leaf header flags (tree 1, first block):"
    btrfs inspect-internal dump-tree -t 1 "${LOOP}p2" 2>/dev/null | head -n 8 | sed 's/^/    /' || true
    log_info "  leaf header flags (tree id 3 = CHUNK_TREE):"
    btrfs inspect-internal dump-tree -t 3 "${LOOP}p2" 2>/dev/null | head -n 10 | sed 's/^/    /' || true
    log_info "  leaf header flags (tree id 7 = CSUM_TREE, all leaves):"
    btrfs inspect-internal dump-tree -t 7 "${LOOP}p2" 2>/dev/null | sed 's/^/    /' || true
    log_info "  targeted block dump (guest-failing leaves 31309824, 31457280):"
    btrfs inspect-internal dump-tree -b 31309824 "${LOOP}p2" 2>/dev/null | head -n 12 | sed 's/^/    /' || true
    btrfs inspect-internal dump-tree -b 31457280 "${LOOP}p2" 2>/dev/null | head -n 12 | sed 's/^/    /' || true

    if ! btrfs check --readonly "${LOOP}p2" >/dev/null 2>&1; then
        log_error "btrfs check(只读) 失败: 镜像内 btrfs 无效（host 侧已损坏）"
        exit 1
    else
        log_info "btrfs check (readonly) OK"
    fi

    log_step "btrfs check --repair（统一 super 副本 + 置 WRITTEN，防 guest 内核 6.6 WRITTEN 检查失败）"
    btrfs check --repair "${LOOP}p2" >"$WORK/repair.log" 2>&1
    _rc=$?
    tail -n 15 "$WORK/repair.log" | sed 's/^/    /' || true
    if [ "$_rc" -ne 0 ]; then
        log_error "btrfs check --repair 失败 (rc=$_rc)"
        exit 1
    fi
    if ! btrfs check --readonly "${LOOP}p2" >/dev/null 2>&1; then
        log_error "repair 后 btrfs check(只读) 失败"
        exit 1
    else
        log_info "repair 后 btrfs check (readonly) OK"
    fi

    log_step "最终 commit 探针：host 挂载最新子卷 -> 写探针 -> filesystem sync -> 干净卸载"
    PROBE="$WORK/probe"; mkdir -p "$PROBE"
    if mount -o subvol=@system "${LOOP}p2" "$PROBE" 2>/dev/null; then
        log_info "  mounted ${LOOP}p2 @system; writing probe + forcing commit"
        ( umask 077; : > "$PROBE/.servecosys_final_commit" ) 2>/dev/null || true
        sync
        btrfs filesystem sync "$PROBE" 2>/dev/null | sed 's/^/    /' || true
        btrfs property set "$PROBE" ro false 2>/dev/null >/dev/null || true
        umount "$PROBE" 2>/dev/null || true
        rmdir "$PROBE" 2>/dev/null || true
    else
        log_warn "  probe mount failed; skipping（仅诊断，不影响校验）"
    fi
    sync

    log_info "  post-commit superblock copies (0/1, generation/root/bytenr):"
    for _sb in 0 1; do
        btrfs inspect-internal dump-super -s "$_sb" "${LOOP}p2" 2>/dev/null | grep -iE '^superblock:|^[[:space:]]*generation|^[[:space:]]*bytenr|^[[:space:]]*root[[:space:]]|^[[:space:]]*csum[[:space:]]' | sed "s/^/    [super:$_sb] /" || true
    done
    if ! btrfs check --readonly "${LOOP}p2" >/dev/null 2>&1; then
        log_error "最终 commit 后 btrfs check(只读) 失败"
        exit 1
    else
        log_info "最终 commit 后 btrfs check (readonly) OK"
    fi
    log_info "  final tree-1 (ROOT_TREE) root leaf:"
    btrfs inspect-internal dump-tree -t 1 "${LOOP}p2" 2>/dev/null | head -n 4 | sed 's/^/    /' || true
    log_info "  final tree-7 (CSUM_TREE) root leaf:"
    btrfs inspect-internal dump-tree -t 7 "${LOOP}p2" 2>/dev/null | head -n 4 | sed 's/^/    /' || true
    log_info "  post-repair full tree, leaves with generation >= 12 (suspicious newer blocks):"
    btrfs inspect-internal dump-tree "${LOOP}p2" 2>/dev/null | grep -E '^leaf ' | awk '$5 ~ /^gen/ && $6 ~ /^[0-9]+/ && $6 >= 12 {print}' | sed 's/^/    /' || true
    log_info "  targeted block dump (also 31326208, guest-failing this round):"
    btrfs inspect-internal dump-tree -b 31326208 "${LOOP}p2" 2>/dev/null | head -n 12 | sed 's/^/    /' || true

    losetup -d "$LOOP" 2>/dev/null || true
    LOOP=""

    log_info "镜像完成: $IMG_OUT ($(wc -c < "$IMG_OUT") bytes)"
    log_info ""
    log_info "  # 验证引导（与 CI 相同的 smoke）:"
    log_info "  qemu-system-x86_64 -machine q35 -m 2048 -smp 2 \\"
    log_info "    -drive file=$IMG_OUT,format=raw,if=virtio -bios /usr/share/ovmf/OVMF.fd \\"
    log_info "    -display none -monitor none -serial file:boot.img.log -no-reboot"
    log_info ""
    log_info "  # 烧录到真实磁盘（Windows: Rufus / BalenaEtcher; Linux: dd）"
    log_info "  sudo dd if=$IMG_OUT of=/dev/sdX bs=4M status=progress && sync"

    # 由 sudo 构建时产物归 root; 放宽权限便于非特权 CI 步骤(QEMU) 读写(其按 O_RDWR 打开 raw 盘)
    chmod 666 "$IMG_OUT" 2>/dev/null || true
}

main "$@"