#!/bin/bash
#
# ServEcosys Btrfs Snapshot Manager
#
# 功能：
#   - 创建系统快照（更新前自动快照）
#   - 列出所有可用快照
#   - 回滚到指定快照
#   - 删除旧快照
#   - 配置引导时启动的快照
#
# 用法：
#   ./snapshot.sh create [name]   创建快照
#   ./snapshot.sh list             列出快照
#   ./snapshot.sh rollback <id>    回滚到快照
#   ./snapshot.sh delete <id>      删除快照
#   ./snapshot.sh boot <id>        设置引导快照
#   ./snapshot.sh info             显示磁盘使用信息
#

set -e

SNAPSHOTS_DIR="/.snapshots"
ROOT_DEV="/dev/sda2"
CONFIG_FILE="/boot/servecosys/snapshot.conf"
MAX_SNAPSHOTS=10

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; }

check_root() {
    if [ "$(id -u)" -ne 0 ]; then
        error "This command must be run as root (permission level 5+ required)"
        exit 1
    fi
}

check_btrfs() {
    if ! command -v btrfs &> /dev/null; then
        error "btrfs command not found. Install btrfs-progs."
        exit 1
    fi
}

detect_root_device() {
    local root_mnt="$1"
    local dev

    dev=$(findmnt -n -o SOURCE --target "$root_mnt" 2>/dev/null | sed 's/\[.*\]//')
    if [ -n "$dev" ]; then
        ROOT_DEV="$dev"
    fi
}

snapshot_create() {
    local name="$1"
    local timestamp
    local snapshot_path

    check_root
    check_btrfs

    timestamp=$(date +%Y%m%d_%H%M%S)
    if [ -z "$name" ]; then
        name="snapshot_${timestamp}"
    fi

    mkdir -p "$SNAPSHOTS_DIR"
    snapshot_path="${SNAPSHOTS_DIR}/${name}"

    info "Creating snapshot: $name"
    info "Source: / (@system subvolume)"

    btrfs subvolume snapshot -r / "$snapshot_path"

    info "Snapshot created: $snapshot_path"
    echo "$timestamp" > "${snapshot_path}.meta"

    local count
    count=$(ls -d "$SNAPSHOTS_DIR"/*/ 2>/dev/null | wc -l)
    if [ "$count" -gt "$MAX_SNAPSHOTS" ]; then
        warn "More than $MAX_SNAPSHOTS snapshots exist."
        warn "Run './snapshot.sh cleanup' to remove old ones."
    fi

    return 0
}

snapshot_list() {
    check_btrfs

    if [ ! -d "$SNAPSHOTS_DIR" ]; then
        info "No snapshots directory found at $SNAPSHOTS_DIR"
        return 0
    fi

    echo ""
    echo "============================================="
    echo " ServEcosys Btrfs Snapshots"
    echo "============================================="
    echo ""

    local boot_id=""
    if [ -f "$CONFIG_FILE" ]; then
        boot_id=$(grep "^SNAPSHOT=" "$CONFIG_FILE" 2>/dev/null | cut -d= -f2)
    fi

    local count=0
    for snap_dir in "$SNAPSHOTS_DIR"/*/; do
        if [ -d "$snap_dir" ]; then
            local name
            local created
            local size
            local boot_marker=""

            name=$(basename "$snap_dir")
            created=$(cat "${snap_dir}.meta" 2>/dev/null || echo "unknown")
            size=$(btrfs qgroup show -e "$snap_dir" 2>/dev/null | tail -1 | awk '{print $2}' || echo "N/A")

            if [ "$name" = "$boot_id" ]; then
                boot_marker=" <-- BOOT"
            fi

            printf "  %-3s  %-30s  %-20s  %-10s%s\n" "$count" "$name" "$created" "$size" "$boot_marker"
            count=$((count + 1))
        fi
    done

    if [ "$count" -eq 0 ]; then
        info "No snapshots found in $SNAPSHOTS_DIR"
    else
        echo ""
        echo "  Total: $count snapshots"
        echo "  Max:   $MAX_SNAPSHOTS snapshots"
    fi
    echo ""
}

snapshot_rollback() {
    local target="$1"

    check_root
    check_btrfs

    if [ ! -d "$SNAPSHOTS_DIR/$target" ]; then
        error "Snapshot '$target' not found in $SNAPSHOTS_DIR"
        exit 1
    fi

    info "WARNING: Rolling back to snapshot: $target"
    warn "This will overwrite the current @system subvolume!"
    echo -n "Are you sure? [y/N] "
    read -r confirm
    if [ "$confirm" != "y" ] && [ "$confirm" != "Y" ]; then
        info "Rollback cancelled."
        exit 0
    fi

    local current_date
    current_date=$(date +%Y%m%d_%H%M%S)
    info "Backing up current system to ${SNAPSHOTS_DIR}/pre_rollback_${current_date}..."
    btrfs subvolume snapshot / "${SNAPSHOTS_DIR}/pre_rollback_${current_date}"

    info "Rolling back to $target..."
    btrfs subvolume delete / 2>/dev/null || true
    btrfs subvolume snapshot "$SNAPSHOTS_DIR/$target" /

    info "Rollback complete. Reboot recommended."
    echo -n "Reboot now? [y/N] "
    read -r reboot_confirm
    if [ "$reboot_confirm" = "y" ] || [ "$reboot_confirm" = "Y" ]; then
        reboot
    fi
}

snapshot_delete() {
    local target="$1"

    check_root
    check_btrfs

    if [ ! -d "$SNAPSHOTS_DIR/$target" ]; then
        error "Snapshot '$target' not found"
        exit 1
    fi

    info "Deleting snapshot: $target"
    btrfs subvolume delete "${SNAPSHOTS_DIR}/${target}"
    rm -f "${SNAPSHOTS_DIR}/${target}.meta"
    info "Snapshot deleted: $target"
}

snapshot_set_boot() {
    local target="$1"

    check_root
    check_btrfs

    if [ "$target" = "LATEST" ]; then
        info "Setting boot to latest snapshot"
        mkdir -p /boot/servecosys
        echo "SNAPSHOT=LATEST" > "$CONFIG_FILE"
        info "Boot snapshot set to LATEST"
        return 0
    fi

    if [ ! -d "$SNAPSHOTS_DIR/$target" ]; then
        error "Snapshot '$target' not found"
        exit 1
    fi

    mkdir -p /boot/servecosys
    echo "SNAPSHOT=${target}" > "$CONFIG_FILE"
    info "Boot snapshot set to: $target"
    info "Next boot will use this snapshot."
}

snapshot_info() {
    check_btrfs

    echo ""
    echo "============================================="
    echo " ServEcosys Btrfs Disk Usage"
    echo "============================================="
    echo ""

    if [ -d "$SNAPSHOTS_DIR" ]; then
        local total_size
        total_size=$(du -sh "$SNAPSHOTS_DIR" 2>/dev/null | cut -f1)
        echo "  Snapshot storage: $SNAPSHOTS_DIR"
        echo "  Total size:       $total_size"
        echo ""
    fi

    btrfs filesystem usage / 2>/dev/null || btrfs filesystem df /
    echo ""

    info "Subvolume layout:"
    echo "  @system  - System files (read-only)"
    echo "  @data    - User data (read-write)"
    echo "  @snapshots - Snapshots (read-write)"
    echo ""
}

snapshot_cleanup() {
    check_root
    check_btrfs

    if [ ! -d "$SNAPSHOTS_DIR" ]; then
        info "No snapshots to clean up."
        return 0
    fi

    local count=0
    local boot_id=""
    if [ -f "$CONFIG_FILE" ]; then
        boot_id=$(grep "^SNAPSHOT=" "$CONFIG_FILE" 2>/dev/null | cut -d= -f2)
    fi

    info "Cleaning up old snapshots (keeping max $MAX_SNAPSHOTS)..."

    local snap_list=()
    for snap_dir in "$SNAPSHOTS_DIR"/*/; do
        if [ -d "$snap_dir" ]; then
            snap_list+=("$(basename "$snap_dir")")
        fi
    done

    local total=${#snap_list[@]}
    if [ "$total" -le "$MAX_SNAPSHOTS" ]; then
        info "Only $total snapshots (max $MAX_SNAPSHOTS), no cleanup needed."
        return 0
    fi

    snap_list=($(for s in "${snap_list[@]}"; do echo "$s"; done | sort))

    local to_delete=$((total - MAX_SNAPSHOTS))
    for ((i=0; i<to_delete; i++)); do
        if [ "${snap_list[$i]}" != "$boot_id" ]; then
            snapshot_delete "${snap_list[$i]}"
        fi
    done

    info "Cleanup complete."
}

case "${1:-help}" in
    create)
        snapshot_create "$2"
        ;;
    list)
        snapshot_list
        ;;
    rollback)
        if [ -z "$2" ]; then
            error "Usage: $0 rollback <snapshot_name>"
            echo "  Use '$0 list' to see available snapshots."
            exit 1
        fi
        snapshot_rollback "$2"
        ;;
    delete)
        if [ -z "$2" ]; then
            error "Usage: $0 delete <snapshot_name>"
            exit 1
        fi
        snapshot_delete "$2"
        ;;
    boot)
        if [ -z "$2" ]; then
            error "Usage: $0 boot <snapshot_name|LATEST>"
            exit 1
        fi
        snapshot_set_boot "$2"
        ;;
    info)
        snapshot_info
        ;;
    cleanup)
        snapshot_cleanup
        ;;
    help|*)
        echo ""
        echo "ServEcosys Btrfs Snapshot Manager"
        echo ""
        echo "Usage:"
        echo "  $0 create [name]     Create a new snapshot"
        echo "  $0 list              List all snapshots"
        echo "  $0 rollback <id>     Rollback to a snapshot"
        echo "  $0 delete <id>       Delete a snapshot"
        echo "  $0 boot <id|LATEST>  Set boot snapshot"
        echo "  $0 info              Show disk usage"
        echo "  $0 cleanup           Remove old snapshots"
        echo ""
        ;;
esac
