#!/usr/bin/env bash
set -euo pipefail

# One-shot: enter flake container and immediately launch the NixOS VM.
# `--cmd` keeps the session interactive after the VM exits (exec bash).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
"${SCRIPT_DIR}/cluster2nix.sh" --cmd "./.vm/nix2nixos-vm.sh && exec bash"