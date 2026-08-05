#!/bin/sh
#
# ServEcosys system init - shared helpers (sourced by boot scripts)
#
# Provides:
#   log()          - timestamped console + audit log entry
#   wait_ready()   - wait up to N seconds for a path (pidfile/socket) to appear
#   start_daemon() - launch a .smle/.ssle daemon, gate on its pidfile
#

RUN_DIR=/var/run
LOG_DIR=/var/log/sed
SYS_LOG="$LOG_DIR/sysinit.log"

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
