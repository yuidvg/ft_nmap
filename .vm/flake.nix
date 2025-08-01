{
  description = "ft_nmap development environment";

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
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in
    {
      devShells = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          default = pkgs.mkShell {
            buildInputs = with pkgs; [
              gnumake
              gcc
              gdb
              nmap
              libpcap
              tcpdump
              direnv
              pkg-config
            ];
          };
        }
      );

      # NixOS VM(Only for amd64 Linux)
      nixosConfigurations = nixpkgs.lib.optionalAttrs (builtins.elem "x86_64-linux" systems) {
        vm = nixpkgs.lib.nixosSystem {
          system = "x86_64-linux";
          modules = [
            "${nixpkgs}/nixos/modules/virtualisation/qemu-vm.nix"
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

              environment = {
                systemPackages = with nixpkgs.legacyPackages.x86_64-linux; [
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
      };

      # VM package for easy access (Linux only)
      packages = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        if system == "x86_64-linux" then
          {
            vm = self.nixosConfigurations.vm.config.system.build.vm;
            default = self.packages.${system}.vm;
          }
        else
          {
            default = pkgs.hello; # placeholder for non-Linux systems
          }
      );

      # Apps for running the VM (Linux only)
      apps = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        if system == "x86_64-linux" then
          {
            vm = {
              type = "app";
              program = "${pkgs.writeShellScript "run-vm" ''
                export QEMU_KERNEL_PARAMS=console=ttyS0
                exec ${self.packages.${system}.vm}/bin/run-nixos-vm -nographic "$@"
              ''}";
            };
            default = self.apps.${system}.vm;
          }
        else
          {
            default = {
              type = "app";
              program = "${pkgs.writeShellScript "no-vm" ''
                echo "VM is only available on x86_64-linux"
                exit 1
              ''}";
            };
          }
      );
    };
}
