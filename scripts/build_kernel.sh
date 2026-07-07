#!/bin/bash
#
# ServEcosys Kernel Build Script
# 
# 编译主内核和设备模块集
# 
# 用法:
#   ./build_kernel.sh [pc|mobile|all|initramfs|sign|full]
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
KERNEL_DIR="$PROJECT_ROOT/kernel"
OUTPUT_DIR="$PROJECT_ROOT/build"
BOOT_DIR="$PROJECT_ROOT/boot"
KEYS_DIR="$PROJECT_ROOT/scripts/keys"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

# 检查依赖
check_dependencies() {
    log_step "Checking dependencies..."
    
    local deps=("make" "gcc" "bc" "kmod" "cpio" "openssl" "git" "ld" "objcopy")
    local missing=()
    
    for dep in "${deps[@]}"; do
        if ! command -v "$dep" &> /dev/null; then
            missing+=("$dep")
        fi
    done
    
    if [ ${#missing[@]} -ne 0 ]; then
        log_error "Missing dependencies: ${missing[*]}"
        log_info "Install with: sudo apt-get install build-essential libncurses-dev bison flex libssl-dev libelf-dev binutils-dev gnu-efi"
        exit 1
    fi
    
    # 检查 gnu-efi
    if [ ! -f "/usr/include/efi/efi.h" ]; then
        log_warn "gnu-efi headers not found. Bootloader compilation may fail."
        log_info "Install with: sudo apt-get install gnu-efi libgnuefi-dev"
    fi
    
    log_info "All dependencies OK"
}

# 编译主内核
build_kernel_core() {
    log_step "Building kernel core..."
    
    # 检查内核源码
    if [ ! -d "$KERNEL_DIR/linux-src" ]; then
        log_error "Linux kernel source not found at $KERNEL_DIR/linux-src"
        log_info "Download with: wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.6.tar.xz"
        exit 1
    fi
    
    cd "$KERNEL_DIR/linux-src"
    
    # 使用 ServEcosys 配置
    log_info "Applying ServEcosys defconfig..."
    cp "$KERNEL_DIR/core/servecosys_defconfig" .config
    make olddefconfig
    
    # 编译内核
    log_info "Compiling kernel image..."
    make -j$(nproc) bzImage
    
    # 编译模块
    log_info "Compiling kernel modules..."
    make -j$(nproc) modules
    
    # 安装模块
    log_info "Installing modules..."
    make modules_install INSTALL_MOD_PATH="$OUTPUT_DIR/modules"
    
    # 复制产物
    cp arch/x86/boot/bzImage "$OUTPUT_DIR/vmlinuz"
    
    log_info "Kernel core build complete: $OUTPUT_DIR/vmlinuz"
}

# 编译 UEFI 引导程序
build_bootloader() {
    log_step "Building UEFI bootloader..."
    
    if [ ! -f "/usr/include/efi/efi.h" ]; then
        log_warn "gnu-efi not installed, skipping bootloader build"
        return 0
    fi
    
    cd "$BOOT_DIR"
    
    # 清理并编译
    make clean
    make ARCH=x86_64
    
    # 复制产物
    if [ -f "bootloader.efi" ]; then
        cp bootloader.efi "$OUTPUT_DIR/"
        log_info "Bootloader built: $OUTPUT_DIR/bootloader.efi"
    else
        log_error "Bootloader build failed"
        exit 1
    fi
}

# 编译 PC 模块集
build_pc_modules() {
    log_info "Building PC modules..."
    
    cd "$KERNEL_DIR/modules/pc"
    
    # TODO: 编译 PC 设备驱动模块
    # make -C /lib/modules/$(uname -r)/build M=$PWD modules
    
    log_info "PC modules build complete"
}

# 编译移动模块集
build_mobile_modules() {
    log_info "Building mobile modules..."
    
    cd "$KERNEL_DIR/modules/mobile"
    
    # TODO: 编译移动设备驱动模块
    # make -C /lib/modules/$(uname -r)/build M=$PWD modules
    
    log_info "Mobile modules build complete"
}

# 编译硬件探测模块
build_probe_modules() {
    log_info "Building probe modules..."
    
    cd "$KERNEL_DIR/modules/probe"
    
    # TODO: 编译硬件指纹探测模块
    
    log_info "Probe modules build complete"
}

# 生成 initramfs
generate_initramfs() {
    log_step "Generating initramfs..."
    
    local initramfs_src="$PROJECT_ROOT/scripts/initramfs"
    local initramfs_out="$OUTPUT_DIR/initramfs.cpio.gz"
    
    # 创建 initramfs 结构
    mkdir -p "$initramfs_src"/{bin,sbin,lib,usr,etc,proc,sys,dev,mnt,snapshots}
    
    # 复制 BusyBox（如果存在）
    if command -v busybox &> /dev/null; then
        cp "$(which busybox)" "$initramfs_src/bin/"
        cd "$initramfs_src/bin"
        # 创建常用命令链接
        for cmd in sh mount mkdir cat ls cp mv rm modprobe insmod dmesg; do
            ln -sf busybox "$cmd" 2>/dev/null || true
        done
    else
        log_warn "BusyBox not found, creating minimal initramfs"
        # 创建最小化 init 脚本
        cat > "$initramfs_src/init" << 'INIT_EOF'
#!/bin/sh
echo "ServEcosys initramfs"
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
echo "Waiting for root device..."
sleep 2
exec /sbin/init
INIT_EOF
        chmod +x "$initramfs_src/init"
    fi
    
    # 创建 fstab
    cat > "$initramfs_src/etc/fstab" << 'FSTAB_EOF'
# ServEcosys fstab
proc /proc proc defaults 0 0
sysfs /sys sysfs defaults 0 0
devtmpfs /dev devtmpfs defaults 0 0
FSTAB_EOF
    
    # 打包
    cd "$initramfs_src"
    find . | cpio -H newc -o 2>/dev/null | gzip > "$initramfs_out"
    
    log_info "initramfs generated: $initramfs_out ($(ls -lh $initramfs_out | awk '{print $5}'))"
}

# 生成密钥对
generate_keys() {
    log_step "Generating cryptographic keys..."
    
    mkdir -p "$KEYS_DIR"
    
    # 项目维护者密钥
    if [ ! -f "$KEYS_DIR/maintainer.key" ]; then
        log_info "Generating maintainer key pair..."
        openssl genrsa -out "$KEYS_DIR/maintainer.key" 4096
        openssl req -new -key "$KEYS_DIR/maintainer.key" \
            -out "$KEYS_DIR/maintainer.csr" \
            -subj "/CN=ServEcosys Maintainer/O=ServEcosys Project/C=CN"
        openssl x509 -req -days 3650 -in "$KEYS_DIR/maintainer.csr" \
            -signkey "$KEYS_DIR/maintainer.key" \
            -out "$KEYS_DIR/maintainer.crt"
    fi
    
    # 审计方密钥
    if [ ! -f "$KEYS_DIR/auditor.key" ]; then
        log_info "Generating auditor key pair..."
        openssl genrsa -out "$KEYS_DIR/auditor.key" 4096
        openssl req -new -key "$KEYS_DIR/auditor.key" \
            -out "$KEYS_DIR/auditor.csr" \
            -subj "/CN=ServEcosys Auditor/O=Independent Security Audit/C=CN"
        openssl x509 -req -days 3650 -in "$KEYS_DIR/auditor.csr" \
            -signkey "$KEYS_DIR/auditor.key" \
            -out "$KEYS_DIR/auditor.crt"
    fi
    
    log_info "Keys generated: $KEYS_DIR/"
}

# 签名内核
sign_kernel() {
    log_step "Signing kernel image..."
    
    local kernel_img="$OUTPUT_DIR/vmlinuz"
    local signature="$OUTPUT_DIR/vmlinuz.sig"
    
    if [ ! -f "$kernel_img" ]; then
        log_error "Kernel image not found: $kernel_img"
        exit 1
    fi
    
    # 生成密钥（如果不存在）
    generate_keys
    
    # 使用维护者密钥签名
    log_info "Signing with maintainer key..."
    openssl dgst -sha256 -sign "$KEYS_DIR/maintainer.key" \
        -out "$signature" "$kernel_img"
    
    # 验证签名
    log_info "Verifying signature..."
    if openssl dgst -sha256 -verify "$KEYS_DIR/maintainer.crt" \
        -signature "$signature" "$kernel_img" > /dev/null 2>&1; then
        log_info "Kernel signed and verified: $signature"
    else
        log_error "Signature verification failed!"
        exit 1
    fi
}

# 完整构建（全部）
build_full() {
    log_step "Starting full ServEcosys build..."
    
    mkdir -p "$OUTPUT_DIR"
    
    check_dependencies
    
    # 1. 编译引导程序
    build_bootloader
    
    # 2. 编译内核
    build_kernel_core
    
    # 3. 编译设备模块
    build_pc_modules
    build_mobile_modules
    build_probe_modules
    
    # 4. 生成 initramfs
    generate_initramfs
    
    # 5. 签名内核
    sign_kernel
    
    # 6. 显示产物
    log_step "Build artifacts:"
    ls -lh "$OUTPUT_DIR"/
    
    log_info "Full build complete!"
    log_info ""
    log_info "Next steps:"
    log_info "  1. Copy bootloader.efi to ESP partition"
    log_info "  2. Configure UEFI to boot from servecosys/bootloader.efi"
    log_info "  3. Test with: qemu-system-x86_64 -kernel $OUTPUT_DIR/vmlinuz -initrd $OUTPUT_DIR/initramfs.cpio.gz"
}

# 主函数
main() {
    local target="${1:-all}"
    
    log_info "ServEcosys Kernel Build System"
    log_info "Target: $target"
    log_info "Output: $OUTPUT_DIR"
    log_info ""
    
    mkdir -p "$OUTPUT_DIR"
    
    case "$target" in
        pc)
            check_dependencies
            build_kernel_core
            build_pc_modules
            build_probe_modules
            ;;
        mobile)
            check_dependencies
            build_kernel_core
            build_mobile_modules
            build_probe_modules
            ;;
        all)
            check_dependencies
            build_kernel_core
            build_pc_modules
            build_mobile_modules
            build_probe_modules
            generate_initramfs
            ;;
        bootloader)
            check_dependencies
            build_bootloader
            ;;
        initramfs)
            generate_initramfs
            ;;
        sign)
            sign_kernel
            ;;
        full)
            build_full
            ;;
        keys)
            generate_keys
            ;;
        clean)
            log_step "Cleaning build artifacts..."
            rm -rf "$OUTPUT_DIR"
            rm -rf "$PROJECT_ROOT/scripts/initramfs"
            log_info "Clean complete"
            ;;
        *)
            log_error "Unknown target: $target"
            echo ""
            echo "Usage: $0 [target]"
            echo ""
            echo "Targets:"
            echo "  bootloader  - Build UEFI bootloader only"
            echo "  pc          - Build kernel + PC modules"
            echo "  mobile      - Build kernel + mobile modules"
            echo "  all         - Build kernel + all modules + initramfs"
            echo "  sign        - Sign kernel image"
            echo "  keys        - Generate cryptographic keys"
            echo "  initramfs   - Generate initramfs only"
            echo "  full        - Complete build (bootloader + kernel + modules + initramfs + sign)"
            echo "  clean       - Remove all build artifacts"
            echo ""
            exit 1
            ;;
    esac
    
    log_info "Build complete!"
}

main "$@"
