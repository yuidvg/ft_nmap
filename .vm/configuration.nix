{ config, pkgs, modulesPath, ... }:
{
  # これを追加
  imports = [
    "${modulesPath}/virtualisation/qemu-vm.nix"
    (builtins.fetchTarball "https://github.com/nix-community/nixos-vscode-server/tarball/master")
  ];
  boot.loader.systemd-boot.enable = true;
  boot.loader.efi.canTouchEfiVariables = true;

  # Set root password for SSH access
  users.users.root = {
    initialPassword = "dev";
  };

  users.users.alice = {
    isNormalUser = true;
    extraGroups = [ "wheel" "docker" ];
    createHome = true;
    initialPassword = "test";
  };

  environment.systemPackages = with pkgs; [
    nmap
    nodejs_20
    vim
    git
    gcc
    gnumake
    libpcap
  ];

  # Automatically log in as root on the console
  services.getty.autologinUser = "root";

  
  virtualisation.sharedDirectories = {
    workspace = {                 
      source = "/workspace";
      target = "/workspace";
    };
  };

  fileSystems."/workspace" = {
    device  = "workspace";
    fsType  = "9p";
    options = "trans=virtio,version=9p2000.L,rw,cache=mmap,uid=1000,gid=100,fmode=0644,dmode=0755";
    neededForBoot = true;
  };

  # すでに用意していた "/workspace をコピーする" サービスは不要になるので
  # 有効にしていた場合は無効化しておく
  systemd.services.copy-workspace.enable = false;

  # Ensure workspace ownership is correct for alice
  systemd.services.fix-workspace-ownership = {
    description = "Fix workspace ownership for alice";
    after = [ "local-fs.target" ];
    wants = [ "local-fs.target" ];
    serviceConfig = {
      Type = "oneshot";
      ExecStart = "${pkgs.coreutils}/bin/chown -R alice:users /workspace";
      RemainAfterExit = true;
    };
    wantedBy = [ "multi-user.target" ];
  };

  # Change directory to /workspace on login
  environment.loginShellInit = "cd /workspace";

  # Enable OpenSSH server so the host can connect into the VM
  services.openssh.enable = true;
  # Allow incoming SSH connections inside the guest
  networking.firewall.allowedTCPPorts = [ 22 ];

  # Forward host port 2222 → guest port 22 (SSH)
  virtualisation.forwardPorts = [
    { from = "host"; host.port = 2222; guest.port = 22; }
  ];

  # Enable VS Code server for Remote-SSH on NixOS
  services.vscode-server.enable = true;

  # Allow running generic dynamically-linked binaries (e.g., bundled NodeJS)
  programs.nix-ld.enable = true;

  system.stateVersion = "24.05";
}
