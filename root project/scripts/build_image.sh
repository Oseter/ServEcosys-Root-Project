#!/bin/bash
#
# ServEcosys Bootable ISO Image Builder
#
# 生成标准 ISO9660 + El Torito UEFI 可启动镜像
# 概念OS (ConceptOS) - ServEcosys 系操作系统标准概念呈现
# 输出: build/ConceptOS.iso
#
# iOS 设备可直接从 GitHub Actions 下载 .iso 文件
# 用法:
#   ./build_image.sh                   生成默认 ISO
#   ./build_image.sh -o myos.iso       指定输出
#   ./build_image.sh -k vmlinuz       指定内核
#   ./build_image.sh -i initramfs.gz  指定 initramfs
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
OUTPUT_ISO="${OUTPUT_ISO:-$BUILD_DIR/ConceptOS.iso}"

KERNEL="${KERNEL:-$BUILD_DIR/vmlinuz}"
INITRAMFS="${INITRAMFS:-$BUILD_DIR/initramfs.cpio.gz}"
BOOTLOADER="${BOOTLOADER:-$BUILD_DIR/bootloader.efi}"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'
log_info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_step()  { echo -e "${BLUE}[STEP]${NC} $1"; }

ISODIR="/tmp/servecosys-isodir"
EFI_IMG="/tmp/servecosys-efi.img"

check_deps() {
    local deps=("xorriso" "dd" "mkfs.fat" "mmd" "mcopy")
    local missing=()

    for dep in "${deps[@]}"; do
        if ! command -v "$dep" &> /dev/null; then
            missing+=("$dep")
        fi
    done

    if [ ${#missing[@]} -ne 0 ]; then
        log_error "Missing dependencies: ${missing[*]}"
        log_info "Install: sudo apt install xorriso mtools dosfstools"
        exit 1
    fi
}

prepare_iso_dir() {
    log_step "Preparing ISO directory"

    rm -rf "$ISODIR"
    mkdir -p "$ISODIR"/{boot,EFI/BOOT,system/backend/bin,system/frontend/bin,system/frontend/apps,system/backend/etc/selinux,bin,sbin,etc}

    # 拷贝内核
    if [ -f "$KERNEL" ]; then
        cp "$KERNEL" "$ISODIR/boot/vmlinuz"
        log_info "  Kernel: $KERNEL"
    else
        log_error "Kernel not found: $KERNEL"
        exit 1
    fi

    # 拷贝 initramfs
    if [ -f "$INITRAMFS" ]; then
        cp "$INITRAMFS" "$ISODIR/boot/initramfs.cpio.gz"
        log_info "  Initramfs: $INITRAMFS"
    else
        log_error "Initramfs not found: $INITRAMFS"
        exit 1
    fi

    # 拷贝 SED 后端 .smle 服务
    if [ -d "$BUILD_DIR/sed" ]; then
        cp "$BUILD_DIR/sed/"*.smle "$ISODIR/system/backend/bin/" 2>/dev/null || true
        log_info "  SED daemons (.smle)"
    fi

    # 拷贝 UID 前端 .ssle 服务
    if [ -d "$BUILD_DIR/uid" ]; then
        cp "$BUILD_DIR/uid/"*.ssle "$ISODIR/system/frontend/bin/" 2>/dev/null || true
        log_info "  UID daemons (.ssle)"
    fi

    # 拷贝 SELinux 策略
    if [ -d "$BUILD_DIR/selinux" ]; then
        cp "$BUILD_DIR/selinux/"* "$ISODIR/system/backend/etc/selinux/" 2>/dev/null || true
    fi

    # 安装 system 集成层（真正的 /sbin/init + 启动脚本）
    # 注意：ISO9660 不支持符号链接，这里用实拷贝 + 顶层副本
    if [ -d "$PROJECT_ROOT/system" ]; then
        cp -r "$PROJECT_ROOT/system/." "$ISODIR/system/"
        chmod +x "$ISODIR"/system/sysinit "$ISODIR"/system/*.sh 2>/dev/null || true
        mkdir -p "$ISODIR/sbin"
        cp "$ISODIR/system/sysinit" "$ISODIR/sbin/init"
        cp "$ISODIR/system/sysinit" "$ISODIR/init"
        log_info "  System init layer: /sbin/init <- /system/sysinit"
    else
        log_warn "  system/ layer not found - ISO will lack a usable init"
    fi

    # GRUB 配置
    if [ -d "$PROJECT_ROOT/boot/grub" ]; then
        cp -r "$PROJECT_ROOT/boot/grub" "$ISODIR/boot/"
        log_info "  GRUB config: boot/grub/grub.cfg"
    fi

    # 版本信息
    {
        echo "概念OS (ConceptOS) - ServEcosys 系操作系统标准与概念呈现"
        echo "Version: 0.1.0 'Genesis'"
        echo "Build: $(date '+%Y-%m-%d %H:%M:%S')"
        echo "Kernel: $(basename "${KERNEL:-unknown}")"
        echo "Arch: x86_64"
    } > "$ISODIR/system/build.info"

    if command -v grub-mkrescue &> /dev/null; then
        log_info "  Using GRUB for UEFI boot"
    fi

    log_info "  ISO directory prepared"
}

create_efi_boot_image() {
    log_step "Creating EFI boot image"

    rm -f "$EFI_IMG"
    dd if=/dev/zero of="$EFI_IMG" bs=1k count=4096 2>/dev/null
    mkfs.fat -F 12 -n "CONCEPTOS" "$EFI_IMG" > /dev/null

    if command -v grub-mkrescue &> /dev/null; then
        log_info "  Skipping EFI image - using grub-mkrescue"
        return
    fi

    # 使用 mtools 操作 FAT 镜像
    if [ -f "$BOOTLOADER" ]; then
        mmd -i "$EFI_IMG" "::EFI/BOOT" 2>/dev/null || true
        mcopy -i "$EFI_IMG" "$BOOTLOADER" "::EFI/BOOT/BOOTX64.EFI" 2>/dev/null
        log_info "  Bootloader: $BOOTLOADER -> EFI/BOOT/BOOTX64.EFI"
    else
        log_warn "  Bootloader not found, ISO will use GRUB if available"
        log_warn "  Creating placeholder EFI image"
        mmd -i "$EFI_IMG" "::EFI/BOOT" 2>/dev/null || true
        echo "ServEcosys - install GRUB for UEFI boot" | mcopy -i "$EFI_IMG" - "::EFI/BOOT/BOOTX64.EFI" 2>/dev/null || true
    fi

    log_info "  EFI boot image created: $EFI_IMG"
}

build_iso() {
    log_step "Building ISO image: $OUTPUT_ISO"

    mkdir -p "$(dirname "$OUTPUT_ISO")"

    if command -v grub-mkrescue &> /dev/null; then
        # 使用 GRUB 创建完整可启动 ISO
        log_info "  Using grub-mkrescue..."
        grub-mkrescue -o "$OUTPUT_ISO" "$ISODIR" 2>/dev/null
    else
        # 使用 xorriso 手动创建 UEFI 可启动 ISO
        log_info "  Using xorriso..."
        xorriso -as mkisofs \
            -iso-level 3 \
            -full-iso9660-filenames \
            -volid "CONCEPTOS" \
            -appid "ConceptOS (ServEcosys Series)" \
            -publisher "ServEcosys" \
            -eltorito-boot boot/efi.img \
            -no-emul-boot \
            -boot-load-size 4 \
            -boot-info-table \
            -eltorito-alt-boot \
            -e boot/efi.img \
            -no-emul-boot \
            -isohybrid-gpt-basdat \
            -isohybrid-apm-hfsplus \
            -o "$OUTPUT_ISO" \
            "$ISODIR" 2>/dev/null
    fi

    log_info "  ISO built: $OUTPUT_ISO"
}

verify_iso() {
    log_step "Verifying ISO"

    if [ ! -f "$OUTPUT_ISO" ]; then
        log_error "ISO not found: $OUTPUT_ISO"
        exit 1
    fi

    local size
    size=$(stat -c%s "$OUTPUT_ISO" 2>/dev/null || stat -f%z "$OUTPUT_ISO" 2>/dev/null)
    log_info "  Size: $(numfmt --to=iec $size 2>/dev/null || echo "$size bytes")"

    file "$OUTPUT_ISO"

    echo ""
    log_info "============================================="
    log_info " QEMU 测试命令:"
    echo ""
    echo "    qemu-system-x86_64 \\"
    echo "      -cdrom $OUTPUT_ISO \\"
    echo "      -m 2G \\"
    echo "      -smp 2 \\"
    echo "      -bios /usr/share/ovmf/OVMF.fd \\"
    echo "      -vga virtio -display gtk"
    echo ""
    echo "    命令行模式:"
    echo "    qemu-system-x86_64 \\"
    echo "      -cdrom $OUTPUT_ISO \\"
    echo "      -m 2G \\"
    echo "      -smp 2 \\"
    echo "      -bios /usr/share/ovmf/OVMF.fd \\"
    echo "      -nographic \\"
    echo "      -kernel /boot/vmlinuz \\"
    echo "      -initrd /boot/initramfs.cpio.gz \\"
    echo "      -append \"console=ttyS0\""
    echo ""
    log_info " iOS 下载后可用 UTM 或 aQEMU 加载此 ISO"
    log_info "============================================="
}

cleanup() {
    log_step "Cleaning up"
    rm -rf "$ISODIR" "$EFI_IMG" /tmp/servecosys-cmdline
    log_info "  Cleanup complete"
}

# ============================================
# Main
# ============================================

main() {
    while getopts "o:k:i:b:h" opt; do
        case $opt in
            o) OUTPUT_ISO="$OPTARG" ;;
            k) KERNEL="$OPTARG" ;;
            i) INITRAMFS="$OPTARG" ;;
            b) BOOTLOADER="$OPTARG" ;;
            h)
                echo "ServEcosys ISO Builder v0.1.0"
                echo "Generate bootable ISO with UEFI support"
                echo ""
                echo "Usage: $0 [options]"
                echo "  -o <file>     Output ISO path (default: build/ConceptOS.iso)"
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
    echo " ServEcosys ISO Image Builder"
    echo "============================================="
    echo ""
    log_info "Output:  $OUTPUT_ISO"
    log_info "Kernel:  ${KERNEL:-auto}"
    log_info "Initrd:  ${INITRAMFS:-auto}"
    log_info "Boot:    ${BOOTLOADER:-auto}"
    echo ""

    check_deps
    trap cleanup EXIT INT TERM

    prepare_iso_dir
    create_efi_boot_image
    build_iso
    verify_iso

    echo ""
    log_info "Done! ISO ready: $OUTPUT_ISO"
    echo "============================================="
}

main "$@"
