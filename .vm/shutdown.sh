#!/usr/bin/env bash
set -euo pipefail

# shutdown.sh - Gracefully stop all running Docker containers and local VMs
#
# 1. Docker containers (rootless)
#    Uses the user-level Docker socket set up by cluster2nix.sh.
# 2. QEMU virtual machines
#    Looks for qemu-system-* processes owned by the current user and
#    sends SIGTERM (15) so they can exit cleanly.
#
# This script is idempotent: if nothing is running, nothing happens.

main() {
    stop_docker_containers
    stop_qemu_vms
    echo "[SUCCESS] All Docker containers and VMs have been stopped."
}

stop_docker_containers() {
    if ! command -v docker >/dev/null 2>&1; then
        echo "[INFO] docker command not found – skipping Docker shutdown." >&2
        return
    fi

    # Point Docker CLI at the rootless daemon socket used elsewhere in this repo
    export DOCKER_HOST="unix:///run/user/$(id -u)/docker.sock"

    local running
    running=$(docker ps -q || true)
    if [[ -z "$running" ]]; then
        echo "[INFO] No running Docker containers." >&2
        return
    fi

    echo "[INFO] Stopping Docker containers..." >&2
    # Use --time to give containers a grace period before SIGKILL (default 10s)
    docker stop --time 5 $running || true
}

stop_qemu_vms() {
    # Match typical qemu-system-* binaries
    if ! pgrep -u "$(id -u)" -f '^qemu-system-' >/dev/null 2>&1; then
        echo "[INFO] No running QEMU VMs." >&2
        return
    fi

    echo "[INFO] Stopping QEMU VMs..." >&2
    # Send SIGTERM (15) to allow graceful guest shutdown
    pkill -u "$(id -u)" -f '^qemu-system-' || true
}

main "$@" 