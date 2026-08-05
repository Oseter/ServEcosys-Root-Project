#!/bin/sh
#
# 概念OS (Concept OS) UID bootstrap - user-facing layer of ServEcosys
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
if [ ! -e "$RUN_DIR/permission_arbiter.pid" ] || [ ! -e "$RUN_DIR/system_manager.pid" ]; then
    log "[ERROR] SED security gate not up; refusing to start UI"
    return 1
fi

start_daemon display_server \
    "$BIN/display_server.ssle" \
    "$RUN_DIR/display_server.pid"

start_daemon input_manager \
    "$BIN/input_manager.ssle" \
    "$RUN_DIR/input_manager.pid"

start_daemon compositor \
    "$BIN/compositor.ssle" \
    "$RUN_DIR/compositor.pid"

start_daemon system_ui \
    "$BIN/system_ui.ssle" \
    "$RUN_DIR/system_ui.pid"

# Register every running frontend daemon with the system manager.
# Uses the manager CLI (authorize <pid> <level> -> manager console socket).
if [ -x "$MGR" ]; then
    for name in display_server input_manager compositor system_ui; do
        pf="$RUN_DIR/$name.pid"
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
