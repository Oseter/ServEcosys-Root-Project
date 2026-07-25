#!/bin/bash
#
# ServEcosys Bootable Disk Image Builder
#
# 生成可用于 QEMU/物理机的系统镜像文件
# 输出: build/servecosys.img
#
# 用法:
#   ./build_image.sh                   生成默认镜像 (4G)
#   ./build_image.sh -s 8G            指定大小 (8G)
#   ./build_image.sh -o myos.img      指定输出文件
#   ./build_image.sh -k vmlinuz       指定内核
#   ./build_image.sh -i initramfs.gz  指定 initramfs
#   ./build_image.sh -b bootloader.efi 指定引导
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
OUTPUT_IMG="${OUTPUT_IMG:-$BUILD_DIR/servecosys.img}"
IMAGE_SIZE="${IMAGE_SIZE:-4G}"

KERNEL="${KERNEL:-$BUILD_DIR/vmlinuz}"
INITRAMFS="${INITRAMFS:-$BUILD_DIR/initramfs.cpio.gz}"
BOOTLOADER="${BOOTLOADER:-$BUILD_DIR/bootloader.efi}"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'
log_info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_step()  { echo -e "${BLUE}[STEP]${NC} $1"; }

MOUNT_DIR="/tmp/servecosys-mount"
ESP_MOUNT="$MOUNT_DIR/esp"
ROOT_MOUNT="$MOUNT_DIR/root"

check_deps() {
    local deps=("dd" "parted" "mkfs.fat" "mkfs.btrfs" "losetup" "mount" "umount" "btrfs")
    local missing=()

    for dep in "${deps[@]}"; do
        if ! command -v "$dep" &> /dev/null; then
            missing+=("$dep")
        fi
    done

    if [ ${#missing[@]} -ne 0 ]; then
        log_error "Missing dependencies: ${missing[*]}"
        log_info "Install: sudo apt install parted dosfstools btrfs-progs"
        exit 1
    fi
}

create_disk_image() {
    log_step "Creating disk image: $OUTPUT_IMG ($IMAGE_SIZE)"

    mkdir -p "$(dirname "$OUTPUT_IMG")"
    dd if=/dev/zero of="$OUTPUT_IMG" bs=1 count=0 seek="$IMAGE_SIZE" 2>/dev/null
    log_info "  Image created: $OUTPUT_IMG ($(ls -lh "$OUTPUT_IMG" | awk '{print $5}'))"
}

partition_disk() {
    log_step "Partitioning disk (GPT)"

    parted -s "$OUTPUT_IMG" mklabel gpt

    # 分区1: EFI System Partition (512MB)
    parted -s "$OUTPUT_IMG" mkpart primary fat32 1MiB 513MiB
    parted -s "$OUTPUT_IMG" set 1 esp on

    # 分区2: Btrfs 根分区 (剩余空间)
    parted -s "$OUTPUT_IMG" mkpart primary btrfs 513MiB 100%

    log_info "  Partition table:"
    parted -s "$OUTPUT_IMG" print
}

setup_loopback() {
    log_step "Setting up loopback devices"

    LOOP_DEV=$(losetup -f)
    losetup -P "$LOOP_DEV" "$OUTPUT_IMG"

    ESP_PART="${LOOP_DEV}p1"
    ROOT_PART="${LOOP_DEV}p2"

    log_info "  Loop device: $LOOP_DEV"
    log_info "  ESP:         $ESP_PART"
    log_info "  Root:        $ROOT_PART"

    sleep 1

    # 等待分区设备出现
    for i in $(seq 1 10); do
        if [ -b "$ESP_PART" ] && [ -b "$ROOT_PART" ]; then break; fi
        sleep 1
    done
}

format_partitions() {
    log_step "Formatting partitions"

    log_info "  Formatting ESP (FAT32)..."
    mkfs.fat -F 32 -n "SERVECOSYS-ESP" "$ESP_PART" > /dev/null

    log_info "  Formatting root (Btrfs)..."
    mkfs.btrfs -f -L "SERVECOSYS-ROOT" "$ROOT_PART" > /dev/null

    log_info "  Partitions formatted"
}

create_subvolumes() {
    log_step "Creating Btrfs subvolumes"

    mkdir -p "$ROOT_MOUNT"
    mount -t btrfs "$ROOT_PART" "$ROOT_MOUNT"

    btrfs subvolume create "$ROOT_MOUNT/@system" > /dev/null
    btrfs subvolume create "$ROOT_MOUNT/@data" > /dev/null
    btrfs subvolume create "$ROOT_MOUNT/@snapshots" > /dev/null

    umount "$ROOT_MOUNT"
    log_info "  Subvolumes: @system, @data, @snapshots"
}

install_bootloader() {
    log_step "Installing UEFI bootloader"

    mkdir -p "$ESP_MOUNT"
    mount "$ESP_PART" "$ESP_MOUNT"

    mkdir -p "$ESP_MOUNT/EFI/BOOT"

    if [ -f "$BOOTLOADER" ]; then
        cp "$BOOTLOADER" "$ESP_MOUNT/EFI/BOOT/BOOTX64.EFI"
        log_info "  Bootloader: $BOOTLOADER -> /EFI/BOOT/BOOTX64.EFI"
    else
        log_warn "  Bootloader not found at $BOOTLOADER"
        log_warn "  Creating fallback BOOTX64.EFI..."
        cat > "$ESP_MOUNT/EFI/BOOT/BOOTX64.EFI" << 'EFI_EOF'
#!/bin/sh
echo "ServEcosys Fallback Bootloader"
echo "Place bootloader.efi at EFI/BOOT/BOOTX64.EFI"
EFI_EOF
    fi

    # grubx64 fallback
    if command -v grub-mkimage &> /dev/null; then
        log_info "  GRUB detected, creating grubx64.efi..."
    fi

    umount "$ESP_MOUNT"
    log_info "  Bootloader installed"
}

install_system_files() {
    log_step "Installing system files"

    mount -t btrfs -o subvol=@system "$ROOT_PART" "$ROOT_MOUNT"

    # 创建系统目录结构
    mkdir -p "$ROOT_MOUNT"/{boot/servecosys,dev,etc,proc,sys,run}
    mkdir -p "$ROOT_MOUNT/system/backend/bin"
    mkdir -p "$ROOT_MOUNT/system/backend/data"
    mkdir -p "$ROOT_MOUNT/system/backend/etc/selinux"
    mkdir -p "$ROOT_MOUNT/system/frontend/bin"
    mkdir -p "$ROOT_MOUNT/system/frontend/apps/system"
    mkdir -p "$ROOT_MOUNT/system/frontend/apps/third_party"
    mkdir -p "$ROOT_MOUNT/system/app-data"
    mkdir -p "$ROOT_MOUNT/var/log/sed"
    mkdir -p "$ROOT_MOUNT/var/run"
    mkdir -p "$ROOT_MOUNT/.snapshots"
    mkdir -p "$ROOT_MOUNT/sbin"
    mkdir -p "$ROOT_MOUNT/lib/modules"

    # 拷贝内核与 initramfs
    if [ -f "$KERNEL" ]; then
        mkdir -p "$ROOT_MOUNT/boot"
        cp "$KERNEL" "$ROOT_MOUNT/boot/vmlinuz"
        log_info "  Kernel: $KERNEL -> /boot/vmlinuz"
    else
        log_warn "  Kernel not found at $KERNEL"
    fi

    if [ -f "$INITRAMFS" ]; then
        cp "$INITRAMFS" "$ROOT_MOUNT/boot/initramfs.cpio.gz"
        log_info "  Initramfs: $INITRAMFS -> /boot/initramfs.cpio.gz"
    else
        log_warn "  Initramfs not found at $INITRAMFS"
    fi

    # 安装 SED 后端 .smle 服务
    if [ -d "$BUILD_DIR/sed" ]; then
        cp -r "$BUILD_DIR/sed/"*.smle "$ROOT_MOUNT/system/backend/bin/" 2>/dev/null || true
        log_info "  SED daemons (.smle) installed"
    fi

    # 安装 UID 前端 .ssle 服务
    if [ -d "$BUILD_DIR/uid" ]; then
        cp -r "$BUILD_DIR/uid/"*.ssle "$ROOT_MOUNT/system/frontend/bin/" 2>/dev/null || true
        log_info "  UID daemons (.ssle) installed"
    fi

    # 安装 SELinux 策略
    if [ -d "$BUILD_DIR/selinux" ]; then
        cp -r "$BUILD_DIR/selinux/"* "$ROOT_MOUNT/system/backend/etc/selinux/" 2>/dev/null || true
        log_info "  SELinux policy installed"
    fi

    # 安装 Btrfs 快照配置
    mkdir -p "$ROOT_MOUNT/boot/servecosys"
    echo "SNAPSHOT=LATEST" > "$ROOT_MOUNT/boot/servecosys/snapshot.conf"

    umount "$ROOT_MOUNT"
    log_info "  System files installed"
}

create_version_file() {
    log_step "Creating version information"

    mount -t btrfs -o subvol=@system "$ROOT_PART" "$ROOT_MOUNT"

    {
        echo "ServEcosys Root Project"
        echo "Version: 0.1.0 'Genesis'"
        echo "Build: $(date '+%Y-%m-%d %H:%M:%S')"
        echo "Kernel: $(basename "${KERNEL:-unknown}")"
        echo "Arch: x86_64"
    } > "$ROOT_MOUNT/system/build.info"

    umount "$ROOT_MOUNT"
}

cleanup() {
    log_step "Cleaning up"

    mount | grep "$MOUNT_DIR" | awk '{print $1}' | while read -r dev; do
        umount "$dev" 2>/dev/null || true
    done

    if [ -n "$LOOP_DEV" ]; then
        losetup -d "$LOOP_DEV" 2>/dev/null || true
    fi

    rm -rf "$MOUNT_DIR"
    log_info "  Cleanup complete"
}

verify_image() {
    log_step "Verifying image"

    if [ ! -f "$OUTPUT_IMG" ]; then
        log_error "Image not found: $OUTPUT_IMG"
        exit 1
    fi

    local img_size
    img_size=$(stat -c%s "$OUTPUT_IMG" 2>/dev/null || stat -f%z "$OUTPUT_IMG" 2>/dev/null)
    log_info "  Image size: $(numfmt --to=iec $img_size 2>/dev/null || echo "$img_size bytes")"

    log_info "  Partition table:"
    parted -s "$OUTPUT_IMG" print 2>/dev/null || fdisk -l "$OUTPUT_IMG" 2>/dev/null || true

    log_info "  To test with QEMU:"
    echo ""
    echo "    qemu-system-x86_64 \\"
    echo "      -drive file=$OUTPUT_IMG,format=raw,if=virtio \\"
    echo "      -m 2G \\"
    echo "      -smp 2 \\"
    echo "      -bios /usr/share/ovmf/OVMF.fd \\"
    echo "      -nographic"
    echo ""
    echo "    qemu-system-x86_64 \\"
    echo "      -drive file=$OUTPUT_IMG,format=raw,if=virtio \\"
    echo "      -m 2G \\"
    echo "      -smp 2 \\"
    echo "      -vga virtio \\"
    echo "      -display gtk"
    echo ""
}

# ============================================
# Main
# ============================================

main() {
    while getopts "s:o:k:i:b:h" opt; do
        case $opt in
            s) IMAGE_SIZE="$OPTARG" ;;
            o) OUTPUT_IMG="$OPTARG" ;;
            k) KERNEL="$OPTARG" ;;
            i) INITRAMFS="$OPTARG" ;;
            b) BOOTLOADER="$OPTARG" ;;
            h)
                echo "ServEcosys Image Builder v0.1.0"
                echo ""
                echo "Usage: $0 [options]"
                echo "  -s <size>     Image size (default: 4G)"
                echo "  -o <file>     Output image path"
                echo "  -k <file>     Kernel image path"
                echo "  -i <file>     Initramfs path"
                echo "  -b <file>     Bootloader EFI path"
                echo "  -h            Show this help"
                exit 0
                ;;
            *) exit 1 ;;
        esac
    done

    echo "============================================="
    echo " ServEcosys Bootable Image Builder"
    echo "============================================="
    echo ""
    log_info "Output:  $OUTPUT_IMG"
    log_info "Size:    $IMAGE_SIZE"
    log_info "Kernel:  ${KERNEL:-auto}"
    log_info "Initrd:  ${INITRAMFS:-auto}"
    log_info "Boot:    ${BOOTLOADER:-auto}"
    echo ""

    check_deps

    trap cleanup EXIT INT TERM

    create_disk_image
    partition_disk
    setup_loopback
    format_partitions
    create_subvolumes
    install_bootloader
    install_system_files
    create_version_file

    echo ""
    log_info "Image build complete!"
    echo "============================================="
    echo ""
    verify_image
}

main "$@"
