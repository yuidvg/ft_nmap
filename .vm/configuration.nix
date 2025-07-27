{ config, pkgs, modulesPath, ... }:
{
  # Import the generic QEMU VM module so that `nixos-rebuild build-vm` works
  imports = [ (modulesPath + "/virtualisation/qemu-vm.nix") ];

  ###### Basic machine parameters ####################################################
  networking.hostName = "ft-nmap-dev";

  # Allocate modest resources; tweak to taste
  virtualisation.memorySize = 2048; # MiB
  virtualisation.cores      = 2;

  ###### Users ######################################################################
  users.users.dev = {
    isNormalUser = true;
    extraGroups  = [ "wheel" ];  # sudo rights
    password     = "";           # blank → log-in with empty password (dev-only!)
  };
  security.sudo.wheelNeedsPassword = false;

  ###### Packages ###################################################################
  environment.systemPackages = with pkgs; [
    # Tool-chain
    gcc               # C compiler
    clang             # alternate compiler / static analyser
    gnumake           # build tool (attribute is `gnumake` in Nixpkgs)
    gdb               # debugger
    valgrind          # dynamic analysis
    pkg-config        # pkg metadata lookup

    # Project-specific deps
    libpcap           # packet-capture library required by ft_nmap
    nmap              # reference implementation for comparison

    # Misc helpers
    git vim tmux curl htop jq
  ];

  ###### Services ###################################################################
  services.openssh.enable                  = true;  # optional remote access
  services.openssh.settings.PermitRootLogin = "yes"; # option renamed in newer NixOS versions
  networking.firewall.allowedTCPPorts      = [ 22 ];

  ###### Convenience ################################################################
  # Start a shell as our dev user automatically when the VM boots under `run-vm`.
  virtualisation.qemu.options = [ "-nic" "user,model=virtio-net-pci" ];
  virtualisation.forwardPorts = [ { from = "host"; proto = "tcp"; host.port = 2222; guest.port = 22; } ];

  ###### NixOS bookkeeping ##########################################################
  # Use the native platform (aarch64-linux in this case)
  # nixpkgs.hostPlatform = "x86_64-linux";  # Commented out to use native platform

  # Pin the state version so future upgrades are explicit.
  system.stateVersion = "23.11";  # Adjust on next NixOS upgrade
}
