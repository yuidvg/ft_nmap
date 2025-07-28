{ config, pkgs, modulesPath, ... }:
{
  # これを追加
  imports = [
    "${modulesPath}/virtualisation/qemu-vm.nix"
  ];
  boot.loader.systemd-boot.enable = true;
  boot.loader.efi.canTouchEfiVariables = true;

  users.users.alice = {
    isNormalUser = true;
    extraGroups = [ "wheel" "docker" ]; # Enable ‘sudo’ and Docker access for the user.
    createHome = true; # Home dir provided by bind mount
    initialPassword = "test";
  };

  environment.systemPackages = with pkgs; [
    nmap
  ];


  # Automatically log in as alice on the console
  services.getty.autologinUser = "alice";

  
  virtualisation.sharedDirectories = {
    workspace = {                 
      source = "/workspace";
      target = "/workspace";
    };
  };

  fileSystems."/workspace" = {
    device  = "workspace";
    fsType  = "9p";
    options = "trans=virtio,version=9p2000.L,rw,cache=mmap";
    neededForBoot = true;
  };

  # すでに用意していた “/workspace をコピーする” サービスは不要になるので
  # 有効にしていた場合は無効化しておく
  systemd.services.copy-workspace.enable = false;

  # Change directory to /workspace on login
  environment.loginShellInit = "cd /workspace";

  system.stateVersion = "24.05";
}
