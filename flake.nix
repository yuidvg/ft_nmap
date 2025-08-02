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
    let
      hostSystems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
    in
    {
      nixosModules = {
        base =
          { pkgs, ... }:
          {
            system.stateVersion = "25.05";
            boot.loader.systemd-boot.enable = true;
            boot.loader.efi.canTouchEfiVariables = true;

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

        mkVmHost =
          hostPkgs:
          { modulesPath, ... }:
          {
            virtualisation.host.pkgs = hostPkgs;
            imports = [
              "${modulesPath}/virtualisation/qemu-vm.nix"
            ];
            virtualisation = {
              sharedDirectories = {
                workspace = {
                  source = builtins.toString ./.;
                  target = "/workspace";
                };
              };
              # Make VM output to the terminal instead of a separate window
              graphics = false;

              # For SSH
              forwardPorts = [
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
      nixosConfigurations = {
        # NixOS VM on amd64 Linux Host
        linuxVM = nixpkgs.lib.nixosSystem {
          system = "x86_64-linux";
          modules = [
            self.nixosModules.base
            (self.nixosModules.mkVmHost nixpkgs.legacyPackages.x86_64-linux)
            self.nixosModules.vmGuest
          ];
        };

        # NixOS VM on arm64 MacOS Host
        darwinVM = nixpkgs.lib.nixosSystem {
          system = "aarch64-linux";
          modules = [
            self.nixosModules.base
            (self.nixosModules.mkVmHost nixpkgs.legacyPackages.aarch64-darwin)
            self.nixosModules.vmGuest
          ];
        };
      };
      packages = {
        x86_64-linux.vm = self.nixosConfigurations.linuxVM.config.system.build.vm;
        aarch64-darwin.vm = self.nixosConfigurations.darwinVM.config.system.build.vm;
      };
      # Development shell
      devShells = builtins.listToAttrs (
        map (
          system:
          let
            pkgsFor = import nixpkgs { inherit system; };
          in
          {
            name = system;
            value = {
              default = pkgsFor.mkShell {
                buildInputs = with pkgsFor; [
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
            };
          }
        ) hostSystems
      );

    };
}
