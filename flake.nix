{
  inputs = {
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
    {
      nixosModules = {
        base =
          { pkgs, ... }:
          {
            system.stateVersion = "25.05";
            # Enable Nix flakes and nix-command on NixOS
            nix.settings.experimental-features = [
              "nix-command"
              "flakes"
            ];

            # Configure networking
            # networking.useDHCP = false;
            # networking.interfaces.eth0.useDHCP = true;

            # Create user "dev"
            services.getty.autologinUser = "dev";
            users.users.dev.isNormalUser = true;

            # Enable passwordless ‘sudo’ for the "test" user
            users.users.dev.extraGroups = [ "wheel" ];
            security.sudo.wheelNeedsPassword = false;

            #System Wide Services
            environment.systemPackages = with pkgs; [
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

        vmHost =
          { system, modulesPath, ... }:
          {
            imports = [
              "${modulesPath}/virtualisation/qemu-vm.nix"
            ];

            virtualisation = {
              host.pkgs = nixpkgs.legacyPackages.${system};
              sharedDirectories = {
                workspace = {
                  source = builtins.toString ./.;
                  target = "/workspace";
                };
              };
              # Make VM output to the terminal instead of a separate window
              vmVariant.virtualisation.graphics = false;

              # For SSH
              vmVariant.virtualisation.forwardPorts = [
                {
                  from = "host";
                  host.port = 2223;
                  guest.port = 22;
                }
              ];
            };
          };

        # VM Modules
        vmGuest =
          { ... }:
          {

            boot.loader.systemd-boot.enable = true;
            boot.loader.efi.canTouchEfiVariables = true;

            # mount cd to /workspace transparently
            # Change directory to /workspace on login
            environment.loginShellInit = "cd /workspace";
            fileSystems."/workspace" = {
              device = "workspace";
              fsType = "9p";
              options = "trans=virtio,version=9p2000.L,rw,cache=mmap,uid=1000,gid=100,fmode=0644,dmode=0755";
              neededForBoot = true;
            };

            # Enable OpenSSH server so the host can connect into the VM
            services.openssh.enable = true;
            # Allow incoming SSH connections inside the guest
            networking.firewall.allowedTCPPorts = [ 22 ];

            imports = [
              nixos-vscode-server.nixosModules.default
            ];

            # Enable VS Code server for Remote-SSH on NixOS
            services.vscode-server.enable = true;
          };
      };
      # Baremetal NixOS
      nixosConfigurations.linuxBase = nixpkgs.lib.nixosSystem {
        system = "x86_64-linux";
        modules = [
          self.nixosModules.base
        ];
      };

      # NixOS VM on amd64 Linux Host
      nixosConfigurations.linuxVM = nixpkgs.lib.nixosSystem {
        system = "x86_64-linux";
        modules = [
          self.nixosModules.base
          self.nixosModules.vmHost
          self.nixosModules.vmGuest
        ];
      };
      packages.x86_64-linux.linuxVM = self.nixosConfigurations.linuxVM.config.system.build.vm;

      # NixOS VM on arm64 MacOS Host
      nixosConfigurations.darwinVM = nixpkgs.lib.nixosSystem {
        system = "aarch64-linux";
        modules = [
          self.nixosModules.base
          self.nixosModules.vmHost
          self.nixosModules.vmGuest
          (
            { pkgs, ... }:
            {
              virtualisation.host.pkgs = nixpkgs.legacyPackages.aarch64-darwin;
            }
          )
        ];
      };
      packages.aarch64-darwin.darwinVM = self.nixosConfigurations.darwinVM.config.system.build.vm;
      # Development shell
      devShells = builtins.listToAttrs (
        map
          (system: {
            name = system;
            value =
              pkgs:
              pkgs.mkShell {
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
          })
          [
            # Dev Shell Systems
            "x86_64-linux"
            "aarch64-linux"
            "x86_64-darwin"
            "aarch64-darwin"
          ]
      );
    };
}
