#!/bin/sh
#
# 概念OS (Concept OS) - SED/UID 守护进程监督器
#
# sysinit 启动各域后，由本脚本在后台常驻，周期检查每个守护进程的
# pidfile 与其记录 PID 是否存活；若已退出则调用 start_daemon 重新拉起。
#
# 安全要点：
#   - permission_arbiter / system_manager 是授权链核心，必须尽快恢复
#     （仲裁器已支持 manager 崩溃后新实例接管管理权）
#
# 由 sysinit 在 exec 维护 shell 之前以后台方式启动。

. /system/lib.sh

SED_BIN=/system/backend/bin
UID_BIN=/system/frontend/bin
CHECK_INTERVAL=5

# 声明需要监督的守护进程：<名称> <可执行文件> <pidfile>
SERVICES="permission_arbiter|$SED_BIN/permission_arbiter.smle|$RUN_DIR/permission_arbiter.pid
system_manager|$SED_BIN/system_manager.smle|$RUN_DIR/system_manager.pid
ipc_bus|$SED_BIN/ipc_bus.smle|$RUN_DIR/ipc_bus.pid
hal_manager|$SED_BIN/hal_manager.smle|$RUN_DIR/hal_manager.pid
oipes_client|$SED_BIN/oipes_client.smle|$RUN_DIR/oipes_client.pid
display_server|$UID_BIN/display_server.ssle|$RUN_DIR/display_server.pid
input_manager|$UID_BIN/input_manager.ssle|$RUN_DIR/input_manager.pid
compositor|$UID_BIN/compositor.ssle|$RUN_DIR/compositor.pid
system_ui|$UID_BIN/system_ui.ssle|$RUN_DIR/system_ui.pid"

# 返回 0 = 进程存活；1 = 已死；2 = 无 pidfile
is_alive() {
    pf="$1"
    [ -f "$pf" ] || return 2
    pid=$(cat "$pf" 2>/dev/null)
    [ -n "$pid" ] || return 2
    kill -0 "$pid" 2>/dev/null && return 0
    return 1
}

log "supervisor: starting with ${CHECK_INTERVAL}s interval"

supervise_loop() {
    for entry in $SERVICES; do
        name=$(echo "$entry" | cut -d'|' -f1)
        exe=$(echo "$entry" | cut -d'|' -f2)
        pf=$(echo "$entry" | cut -d'|' -f3)

        # 无 pidfile：若可执行文件存在且属安全门已过，则补齐拉起
        is_alive "$pf"
        st=$?
        if [ "$st" -eq 0 ]; then
            continue
        fi

        if [ ! -x "$exe" ]; then
            log "[WARN] $name not installed ($exe), skip"
            continue
        fi

        # 权限仲裁器未就绪前，UID 域守护进程不得私自先行
        if [ "$name" != "permission_arbiter" ] && [ "$name" != "system_manager" ]; then
            if [ ! -e "$RUN_DIR/permission_arbiter.pid" ]; then
                continue
            fi
        fi

        if start_daemon "$name" "$exe" "$pf"; then
            log "$name respawned by supervisor"
        else
            log "[ERROR] $name failed to respawn"
        fi
        sleep 1
    done
}

while :; do
    supervise_loop
    sleep "$CHECK_INTERVAL"
done
