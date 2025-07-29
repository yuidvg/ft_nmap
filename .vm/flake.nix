{
  description = "ft_nmap development environment with NixOS VM";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    nixos-vscode-server = {
      url = "github:nix-community/nixos-vscode-server";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, nixos-vscode-server }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in
    {
      # NixOS VM configuration
      nixosConfigurations.vm = nixpkgs.lib.nixosSystem {
        inherit system;
        modules = [
          "${nixpkgs}/nixos/modules/virtualisation/qemu-vm.nix"
          nixos-vscode-server.nixosModules.default
          {
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
            # Enable Nix flakes and nix-command inside the VM
            nix.settings.experimental-features = [ "nix-command" "flakes" ];

            system.stateVersion = "24.05";
          }
        ];
      };

      # Development shell
      devShells.${system}.default = pkgs.mkShell {
        buildInputs = with pkgs; [
          nmap
          nodejs_20
          vim
          git
          gcc
          gnumake
          libpcap
          tcpdump
        ];
      };

      # VM package for easy access
      packages.${system} = {
        vm = self.nixosConfigurations.vm.config.system.build.vm;
        default = self.packages.${system}.vm;
      };

      # Apps for running the VM
      apps.${system} = {
        vm = {
          type = "app";
          program = "${pkgs.writeShellScript "run-vm" ''
            export QEMU_KERNEL_PARAMS=console=ttyS0
            exec ${self.packages.${system}.vm}/bin/run-nixos-vm -nographic "$@"
          ''}";
        };
        default = self.apps.${system}.vm;
      };
    };
} 