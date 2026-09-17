#!/bin/sh
#
# ServEcosys system init - shared helpers (sourced by boot scripts)
#
# Provides:
#   log()          - timestamped console + audit log entry
#   wait_ready()   - wait up to N seconds for a path (pidfile/socket) to appear
#   start_daemon() - launch a .smle/.ssle daemon, gate on its pidfile
#
# 统一管理 socket / pidfile 路径，修改仅需改此处

RUN_DIR=/var/run
LOG_DIR=/var/log/sed
SYS_LOG="$LOG_DIR/sysinit.log"

# Socket 路径（所有 .smle/.ssle 服务统一约定）
SOCK_PERM_ARBITER="$RUN_DIR/servecosys_perm.sock"
SOCK_SYS_MGR="$RUN_DIR/servecosys_mgr.sock"
SOCK_IPC_BUS="$RUN_DIR/servecosys_ipc.sock"
SOCK_DISPLAY="$RUN_DIR/servecosys_display.sock"
SOCK_INPUT="$RUN_DIR/servecosys_input.sock"

# PID 文件路径
PID_PERM_ARBITER="$RUN_DIR/permission_arbiter.pid"
PID_SYS_MGR="$RUN_DIR/system_manager.pid"
PID_IPC_BUS="$RUN_DIR/ipc_bus.pid"
PID_HAL_MGR="$RUN_DIR/hal_manager.pid"
PID_OIPES="$RUN_DIR/oipes_client.pid"
PID_DISPLAY="$RUN_DIR/display_server.pid"
PID_INPUT="$RUN_DIR/input_manager.pid"
PID_COMPOSITOR="$RUN_DIR/compositor.pid"
PID_SYS_UI="$RUN_DIR/system_ui.pid"
PID_SELINUX_MGR="$RUN_DIR/selinux_manager.pid"

log() {
    echo "[sysinit] $*"
    mkdir -p "$LOG_DIR" 2>/dev/null
    echo "[$(date '+%F %T')] $*" >> "$SYS_LOG" 2>/dev/null || true
}

# wait_ready <seconds> <path> : returns 0 when <path> exists, else 1 on timeout
wait_ready() {
    t="$1"
    p="$2"
    n=0
    while [ "$n" -lt "$t" ]; do
        [ -e "$p" ] && return 0
        n=$((n + 1))
        sleep 1
    done
    return 1
}

# 原子写 pidfile：临时文件 + rename，避免 wait_ready 读到不完整内容
write_pidfile_atomic() {
    local pidfile="$1"
    local pid="$2"
    local tmp="${pidfile}.tmp.$$"
    echo "$pid" > "$tmp" && mv -f "$tmp" "$pidfile"
}

# 恢复 SELinux 文件上下文（若 selinux 已启用且 restorecon 可用）
restore_selinux_context() {
    local path="$1"
    if [ -x /sbin/restorecon ] && [ -f /sys/fs/selinux/enforce ] && [ "$(cat /sys/fs/selinux/enforce 2>/dev/null)" = "1" ]; then
        /sbin/restorecon -F "$path" 2>/dev/null || true
    fi
}

# start_daemon <label> <executable> <pidfile>
#   Launches the daemon in the background, logging to $LOG_DIR/<label>.log.
#   Returns 0 only if the daemon signals readiness (pidfile) within 8s.
start_daemon() {
    label="$1"
    exe="$2"
    pidfile="$3"

    if [ ! -x "$exe" ]; then
        log "[WARN] $label missing or not executable: $exe (skipped)"
        return 1
    fi

    mkdir -p "$RUN_DIR" "$LOG_DIR" 2>/dev/null

    # Clear stale runstate from a previous boot/crash so wait_ready() cannot
    # be fooled into thinking the daemon is already up.
    rm -f "$pidfile"

    # 恢复可执行文件的 SELinux 上下文（首次部署/更新后必需）
    restore_selinux_context "$exe"

    "$exe" >> "$LOG_DIR/$label.log" 2>&1 &

    if wait_ready 8 "$pidfile"; then
        pid=$(cat "$pidfile" 2>/dev/null)
        log "started $label (pid ${pid:-unknown})"
        return 0
    fi

    log "[WARN] $label did not signal readiness (no $pidfile); log follows:"
    tail -n 5 "$LOG_DIR/$label.log" 2>/dev/null | sed 's/^/    /' || true
    return 1
}