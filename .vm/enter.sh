#!/usr/bin/env bash
# ... existing code ...
set -euo pipefail

# Build a minimal NixOS VM based on the configuration in .vm/configuration.nix.
# We quote the angled-bracket path to avoid shell redirection.
# The build result will appear under ./result/
# shellcheck disable=SC2086
nix-build '<nixpkgs/nixos>' \
          -A vm \
          -I nixpkgs=channel:nixos-25.05 \
          -I nixos-config=./.vm/configuration.nix

# Once the build completes successfully we can launch the VM.
# QEMU will print its serial console on stdout so we pass -nographic
# and force the kernel to use ttyS0.
QEMU_KERNEL_PARAMS="console=ttyS0" ./result/bin/run-nixos-vm -nographic

# When QEMU exits restore the terminal state.
reset