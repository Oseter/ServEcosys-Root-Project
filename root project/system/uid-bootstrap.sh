#!/bin/sh
#
# 概念OS (Concept OS) UID bootstrap
# 概念OS = ServEcosys 系操作系统标准的概念化呈现（顶部导航栏 + 交互终端）
#
# Starts the Frontend Interaction Domain (.ssle services) only after the
# SED security gate (permission_arbiter + system_manager) is authoritative.
# Each UID daemon is then registered with the system manager so the user can
# grant it an authorization level via the manager console:
#     system_manager authorize <pid> <0-11>
#
# Boot-time default: frontend daemons are authorized up to level 2 (user).
#

. /system/lib.sh

BIN=/system/frontend/bin
MGR=/system/backend/bin/system_manager.smle

log "=== 概念OS (Concept OS) UID bootstrap ==="

# Security gate: UI must never render before the authority services are up
if [ ! -e "$PID_PERM_ARBITER" ] || [ ! -e "$PID_SYS_MGR" ]; then
    log "[ERROR] SED security gate not up; refusing to start UI"
    return 1
fi

start_daemon display_server \
    "$BIN/display_server.ssle" \
    "$PID_DISPLAY"

start_daemon input_manager \
    "$BIN/input_manager.ssle" \
    "$PID_INPUT"

start_daemon compositor \
    "$BIN/compositor.ssle" \
    "$PID_COMPOSITOR"

start_daemon system_ui \
    "$BIN/system_ui.ssle" \
    "$PID_SYS_UI"

# Register every running frontend daemon with the system manager.
# Uses the manager CLI (authorize <pid> <level> -> manager console socket).
if [ -x "$MGR" ]; then
    for name in display_server input_manager compositor system_ui; do
        # Map name to PID variable
        case "$name" in
            display_server) pf="$PID_DISPLAY" ;;
            input_manager)  pf="$PID_INPUT" ;;
            compositor)     pf="$PID_COMPOSITOR" ;;
            system_ui)      pf="$PID_SYS_UI" ;;
            *) continue ;;
        esac
        [ -f "$pf" ] || continue
        pid=$(cat "$pf" 2>/dev/null)
        [ -n "$pid" ] || continue
        "$MGR" authorize "$pid" 2 >> "$LOG_DIR/uid-bootstrap.log" 2>&1
        log "authorized $name (pid $pid) to level 2"
    done
else
    log "[WARN] manager CLI unavailable ($MGR); frontend processes not pre-authorized"
fi

log "=== UID bootstrap done ==="