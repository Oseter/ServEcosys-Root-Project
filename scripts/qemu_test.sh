#!/bin/bash
#
# ServEcosys QEMU Test Script
#
# 快速测试内核和 initramfs
# 支持 x86_64 和 ARM64 架构
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# 默认配置
ARCH="${ARCH:-x86_64}"
MEMORY="${MEMORY:-2048}"
SMP="${SMP:-2}"
NETWORK="${NETWORK:-true}"
GRAPHIC="${GRAPHIC:-false}"
DEBUG="${DEBUG:-false}"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

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

# 显示用法
usage() {
    cat << EOF
ServEcosys QEMU Test Script

用法: $0 [选项]

选项:
  -a, --arch <arch>      架构 (x86_64, arm64) [默认：x86_64]
  -m, --memory <MB>      内存大小 (MB) [默认：2048]
  -s, --smp <CPUs>       CPU 核心数 [默认：2]
  -n, --no-network       禁用网络
  -g, --graphic          启用图形输出（默认文本模式）
  -d, --debug            调试模式（详细日志）
  -h, --help             显示此帮助

示例:
  $0                              # 默认 x86_64 测试
  $0 -a arm64 -m 4096 -s 4        # ARM64, 4GB 内存，4 核
  $0 -d -g                        # 调试模式 + 图形界面
  $0 --no-network                 # 无网络测试

EOF
    exit 0
}

# 解析参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -a|--arch)
            ARCH="$2"
            shift 2
            ;;
        -m|--memory)
            MEMORY="$2"
            shift 2
            ;;
        -s|--smp)
            SMP="$2"
            shift 2
            ;;
        -n|--no-network)
            NETWORK=false
            shift
            ;;
        -g|--graphic)
            GRAPHIC=true
            shift
            ;;
        -d|--debug)
            DEBUG=true
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            log_error "未知选项：$1"
            usage
            ;;
    esac
done

# 检查构建产物
check_build_artifacts() {
    log_step "Checking build artifacts..."
    
    if [ "$ARCH" = "x86_64" ]; then
        KERNEL="$BUILD_DIR/vmlinuz"
        INITRD="$BUILD_DIR/initramfs.cpio.gz"
    elif [ "$ARCH" = "arm64" ]; then
        KERNEL="$BUILD_DIR/vmlinuz-arm64"
        INITRD="$BUILD_DIR/initramfs-arm64.cpio.gz"
    else
        log_error "不支持的架构：$ARCH"
        exit 1
    fi
    
    if [ ! -f "$KERNEL" ]; then
        log_error "内核镜像未找到：$KERNEL"
        log_info "请先运行：./scripts/build_kernel.sh all"
        exit 1
    fi
    
    if [ ! -f "$INITRD" ]; then
        log_warn "initramfs 未找到：$INITRD"
        log_info "将尝试无 initrd 启动"
        INITRD=""
    fi
    
    log_info "Kernel: $KERNEL"
    [ -n "$INITRD" ] && log_info "Initrd: $INITRD"
}

# 构建 QEMU 命令
build_qemu_cmd() {
    local qemu_bin=""
    local machine_opts=""
    local kernel_opts=""
    local console_opts=""
    
    if [ "$ARCH" = "x86_64" ]; then
        qemu_bin="qemu-system-x86_64"
        machine_opts="-machine q35 -cpu qemu64"
        kernel_opts=""
        console_opts="console=ttyS0"
    elif [ "$ARCH" = "arm64" ]; then
        qemu_bin="qemu-system-aarch64"
        machine_opts="-machine virt -cpu cortex-a57"
        kernel_opts=""
        console_opts="console=ttyAMA0"
    fi
    
    # 检查 QEMU 是否存在
    if ! command -v "$qemu_bin" &> /dev/null; then
        log_error "QEMU 未安装：$qemu_bin"
        log_info "安装：sudo apt-get install qemu-system-x86 qemu-system-arm"
        exit 1
    fi
    
    # 构建完整命令
    CMD="$qemu_bin"
    
    # 机器配置
    CMD="$CMD $machine_opts"
    CMD="$CMD -m $MEMORY"
    CMD="$CMD -smp $SMP"
    
    # 内核
    CMD="$CMD -kernel $KERNEL"
    [ -n "$INITRD" ] && CMD="$CMD -initrd $INITRD"
    
    # 内核参数
    if [ "$DEBUG" = true ]; then
        CMD="$CMD -append \"$console_opts loglevel=7 debug earlyprintk\""
    else
        CMD="$CMD -append \"$console_opts quiet\""
    fi
    
    # 网络
    if [ "$NETWORK" = true ]; then
        CMD="$CMD -netdev user,id=net0 -device e1000,netdev=net0"
    else
        CMD="$CMD -netdev none"
    fi
    
    # 显示
    if [ "$GRAPHIC" = true ]; then
        CMD="$CMD -vga std"
    else
        CMD="$CMD -nographic"
    fi
    
    # 调试选项
    if [ "$DEBUG" = true ]; then
        CMD="$CMD -d guest_errors,unimp"
        CMD="$CMD --no-reboot"
    fi
    
    echo "$CMD"
}

# 主函数
main() {
    log_info "ServEcosys QEMU Test"
    log_info "Architecture: $ARCH"
    log_info "Memory: ${MEMORY}MB"
    log_info "CPUs: $SMP"
    log_info "Network: $NETWORK"
    log_info "Graphic: $GRAPHIC"
    log_info "Debug: $DEBUG"
    echo ""
    
    check_build_artifacts
    
    QEMU_CMD=$(build_qemu_cmd)
    
    log_step "QEMU command:"
    echo "$QEMU_CMD"
    echo ""
    
    log_info "Starting QEMU..."
    echo "=========================================="
    echo "按 Ctrl+A 然后 X 退出 QEMU"
    echo "=========================================="
    echo ""
    
    # 执行 QEMU
    eval "$QEMU_CMD"
}

main "$@"
