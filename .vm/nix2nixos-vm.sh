#!/usr/bin/env bash
set -euo pipefail

rm -f ./nixos.qcow2

nix-build '<nixpkgs/nixos>' -A vm -I nixpkgs=channel:nixos-unstable -I nixos-config=.vm/configuration.nix

QEMU_KERNEL_PARAMS=console=ttyS0 ./result/bin/run-nixos-vm -nographic
reset
