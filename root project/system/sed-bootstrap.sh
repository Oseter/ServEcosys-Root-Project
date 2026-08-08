#!/bin/sh
#
# ServEcosys SED bootstrap
#
# Brings up the Backend Security Domain (.smle services) in dependency order:
#   1. permission_arbiter  - security core (authority gate)
#   2. system_manager      - registers itself as the sole authorization source
#   3. ipc_bus             - SED<->UID transport
#   4. selinux_manager     - policy manager (optional, needs compiled policy)
#   5. hal_manager         - hardware abstraction
#   6. oipes_client        - ecosystem proxy (optional)
#
# Every daemon is gated on its /var/run pidfile before the next one starts.
#

. /system/lib.sh

BIN=/system/backend/bin

log "=== SED bootstrap ==="
mkdir -p "$RUN_DIR" "$LOG_DIR" 2>/dev/null

# 1. Permission arbitration - must be up before anything else
start_daemon permission_arbiter \
    "$BIN/permission_arbiter.smle" \
    "$RUN_DIR/permission_arbiter.pid"

if [ ! -e "$RUN_DIR/permission_arbiter.pid" ]; then
    log "[ERROR] permission_arbiter failed to start; cannot proceed to UID"
fi

# 2. System manager - claims the manager authority (only it may authorize)
start_daemon system_manager \
    "$BIN/system_manager.smle" \
    "$RUN_DIR/system_manager.pid"

if [ ! -e "$RUN_DIR/system_manager.pid" ]; then
    log "[ERROR] system_manager failed to register as authority"
fi

# 3. IPC bus - transport between the two domains
start_daemon ipc_bus \
    "$BIN/ipc_bus.smle" \
    "$RUN_DIR/ipc_bus.pid"

# 4. SELinux policy manager - only if a compiled policy is present
if [ -f /system/backend/etc/selinux/servecosys.pp ]; then
    start_daemon selinux_manager \
        "$BIN/selinux_manager.smle" \
        "$RUN_DIR/selinux_manager.pid"
else
    log "selinux policy not found (/system/backend/etc/selinux/servecosys.pp), skipping selinux_manager"
fi

# 5. Hardware abstraction layer
start_daemon hal_manager \
    "$BIN/hal_manager.smle" \
    "$RUN_DIR/hal_manager.pid"

# 6. OIPES client proxy - optional, network may be absent
start_daemon oipes_client \
    "$BIN/oipes_client.smle" \
    "$RUN_DIR/oipes_client.pid"

log "=== SED bootstrap done ==="
