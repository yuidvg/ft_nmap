{
  description = "ft_nmap development environment with NixOS VM";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    nixos-vscode-server = {
      url = "github:nix-community/nixos-vscode-server";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      nixos-vscode-server,
    }:
    let
      system = "aarch64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in
    {
      # NixOS VM configuration
      nixosConfigurations.vm = nixpkgs.lib.nixosSystem {
        inherit system;
        modules = [
          self.nixosModules.base
          self.nixosModules.vm
          {
            virtualisation.vmVariant.virtualisation.host.pkgs = nixpkgs.legacyPackages.aarch64-darwin;
          }
          nixos-vscode-server.nixosModules.default
          {
            boot.loader.systemd-boot.enable = true;
            boot.loader.efi.canTouchEfiVariables = true;

            programs.git.enable = true;
            programs.git.config.safe.directory = [
              "/workspace"
            ];

            # Automatically log in as root on the console
            services.getty.autologinUser = "root";
            users.users.root = {
              password = "";
            };

            virtualisation = {
              # Forward host port 2222 → guest port 22 (SSH)
              forwardPorts = [
                {
                  from = "host";
                  host.port = 2222;
                  guest.port = 22;
                }
              ];
              memorySize = 4096;
              sharedDirectories = {
                workspace = {
                  source = "/workspace";
                  target = "/workspace";
                };
              };
            };

            fileSystems."/workspace" = {
              device = "workspace";
              fsType = "9p";
              options = "trans=virtio,version=9p2000.L,rw,cache=mmap,uid=1000,gid=100,fmode=0644,dmode=0755";
              neededForBoot = true;
            };

            systemd.services.copy-workspace.enable = false;

            # Ensure workspace ownership is correct for alice
            # systemd.services.fix-workspace-ownership = {
            #   description = "Fix workspace ownership for alice";
            #   after = [ "local-fs.target" ];
            #   wants = [ "local-fs.target" ];
            #   serviceConfig = {
            #     Type = "oneshot";
            #     ExecStart = "${pkgs.coreutils}/bin/chown -R alice:users /workspace";
            #     RemainAfterExit = true;
            #   };
            #   wantedBy = [ "multi-user.target" ];
            # };

            environment = {
              systemPackages = with pkgs; [
                nmap
                nodejs_20
                vim
                gcc
                gnumake
                libpcap
                tcpdump
                gdb
                valgrind
                direnv
                pkg-config
              ];

              # Change directory to /workspace on login
              loginShellInit = "cd /workspace";

              # Ensure pkg-config can locate .pc files installed under /run/current-system/sw
              variables = {
                PKG_CONFIG_PATH = "/run/current-system/sw/lib/pkgconfig:/run/current-system/sw/share/pkgconfig";
              };
            };

            # Enable OpenSSH server so the host can connect into the VM
            services.openssh = {
              enable = true;
              passwordAuthentication = true; # allow passwords
              permitRootLogin = "yes"; # root allowed
              settings = {
                PermitEmptyPasswords = "yes"; # accept empty passwords (very insecure)
              };
            };

            # Allow PAM to accept null passwords for SSH
            security.pam.services.sshd.allowNullPassword = true;
            # Allow incoming SSH connections inside the guest
            networking.firewall.allowedTCPPorts = [ 22 ];

            # Enable VS Code server for Remote-SSH on NixOS
            services.vscode-server = {
              enable = true;
            };

            # Allow running generic dynamically-linked binaries (e.g., bundled NodeJS)
            programs.nix-ld.enable = true;
            # Enable Nix flakes and nix-command inside the VM
            nix.settings.experimental-features = [
              "nix-command"
              "flakes"
            ];

            system.stateVersion = "24.05";
          }
        ];
      };

      # Development shell
      # devShells.${system}.default = pkgs.mkShell {
      #   buildInputs = with pkgs; [
      #     nmap
      #     nodejs_20
      #     vim
      #     git
      #     gcc
      #     gnumake
      #     libpcap
      #     tcpdump
      #   ];
      # };

      # VM package for easy access
      packages.${system} = {
        vm = self.nixosConfigurations.vm.config.system.build.vm;
        default = self.packages.${system}.vm;
      };
      packages.aarch64-darwin.darwinVM = self.nixosConfigurations.darwinVM.config.system.build.vm;

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
