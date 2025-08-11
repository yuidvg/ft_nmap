{
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
      hostSystems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];

      basePackagesFor =
        pkgs: with pkgs; [
          clang-tools
          clang
          nmap
          git
          lldb
          gnumake
          libpcap
          tcpdump
          pkg-config
          nil
          nixfmt-rfc-style
          bear
          #haskell
          ghc
          cabal-install
          haskell-language-server
          hlint
          cabal-install
          ormolu
          haskellPackages.cabal-gild
          haskellPackages.ghci-dap
          haskellPackages.haskell-debug-adapter
          haskellPackages.haskell-dap
          haskellPackages.implicit-hie
        ];

      vmPackagesFor =
        pkgs: with pkgs; [
          strace
          valgrind
          nodejs_20
        ];
    in
    {
      nixosModules = {
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
                  source = builtins.getEnv "PWD";
                  target = "/workspace";
                };
              };
              # Make VM output to the terminal instead of a separate window
              graphics = false;
              memorySize = 8192;
              cores = 4;

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
          { pkgs, lib, ... }:
          {
            system.stateVersion = "25.05";
            boot.loader.systemd-boot.enable = true;
            boot.loader.efi.canTouchEfiVariables = true;
            boot.kernel.sysctl = {
              "net.core.rmem_max" = 134217728;
              "net.core.wmem_max" = 134217728;
              "net.core.netdev_max_backlog" = 250000;
            };

            services.journald = {
              storage = "volatile";
              rateLimitInterval = "30s";
              rateLimitBurst = 1000;
            };

            # Enable Nix flakes and nix-command on NixOS
            nix.settings.experimental-features = [
              "nix-command"
              "flakes"
            ];

            # Configure networking
            # networking.useDHCP = false;
            # networking.interfaces.eth0.useDHCP = true;

            # Create user "dev"
            services.getty.autologinUser = "root";

            users.users.root = {
              password = "a";
            };

            #System Wide Services
            environment = {
              systemPackages = basePackagesFor pkgs ++ vmPackagesFor pkgs;
              variables.PKG_CONFIG_PATH = "/run/current-system/sw/lib/pkgconfig:/run/current-system/sw/share/pkgconfig";
            };
            # mount cd to /workspace transparently
            # Change directory to /workspace on login
            environment.loginShellInit = "cd /workspace";
            fileSystems."/workspace" = {
              device = "workspace";
              fsType = "9p";
              options = "trans=virtio,version=9p2000.L,rw,cache=mmap,uid=0,gid=0,fmode=0644,dmode=0755";
              neededForBoot = true;
            };
            systemd.services.copy-workspace.enable = false;

            programs.git = {
              enable = true;
              config.safe.directory = [
                "/workspace"
              ];
            };
            programs.ssh.extraConfig = ''
              Host github.com
                StrictHostKeyChecking no
                UserKnownHostsFile=/dev/null
            '';
            programs.direnv = {
              enable = true;
              nix-direnv.enable = true;
            };

            # Enable OpenSSH server so the host can connect into the VM
            services.openssh = {
              enable = true;
              settings.PermitRootLogin = "yes";
              settings.PasswordAuthentication = true;
            };

            # Allow incoming SSH connections inside the guest
            networking.firewall.allowedTCPPorts = [ 22 ];

            imports = [
              nixos-vscode-server.nixosModules.default
            ];

            # Enable VS Code server for Remote-SSH on NixOS
            services.vscode-server.enable = true;
          };
      };
      nixosConfigurations = builtins.listToAttrs (
        map (
          host:
          let
            arch = builtins.head (builtins.split "-" host);
            guest = "${arch}-linux";
            hostPkgs = builtins.getAttr host nixpkgs.legacyPackages;
          in
          {
            name = host;
            value = nixpkgs.lib.nixosSystem {
              system = guest;
              modules = [
                (self.nixosModules.mkVmHost hostPkgs)
                self.nixosModules.vmGuest
              ];
            };
          }
        ) hostSystems
      );
      packages = builtins.listToAttrs (
        map (host: {
          name = host;
          value =
            let
              pkgsFor = import nixpkgs { system = host; };
            in
            rec {
              vm = self.nixosConfigurations.${host}.config.system.build.vm;
              # Build the Haskell e2e executable defined in test/e2e/e2e.cabal (manual wiring, no cabal2nix)
              e2e = pkgsFor.haskellPackages.mkDerivation {
                pname = "e2e";
                version = "0.1.0.0";
                # Use builtins.path to include even if the test dir is not tracked by Git yet
                src = builtins.path {
                  path = ./test/e2e;
                  name = "e2e-src";
                };
                isLibrary = false;
                isExecutable = true;
                executableHaskellDepends = with pkgsFor.haskellPackages; [
                  base
                  tasty
                  tasty-hunit
                  process
                  directory
                  containers
                ];
                license = pkgsFor.lib.licenses.bsd3;
                description = "ft_nmap end-to-end tests";
                doCheck = false;
              };
              default = vm;
            };
        }) hostSystems
      );
      # App to run Haskell e2e tests in one shot: `nix run .#e2e`
      apps = builtins.listToAttrs (
        map (
          system:
          let
            pkgsFor = import nixpkgs { inherit system; };
            e2eDrv = self.packages.${system}.e2e;
            # Wrap the built e2e binary to ensure runtime deps (like nmap) are on PATH
            runner = pkgsFor.writeShellApplication {
              name = "ftnmap-e2e";
              runtimeInputs = [ pkgsFor.nmap ];
              text = ''
                set -euo pipefail
                # Run from repo root so relative fixtures like script/res/correct_output.txt resolve
                cd ${self}
                exec ${e2eDrv}/bin/e2e "$@"
              '';
            };
          in
          {
            name = system;
            value = {
              e2e = {
                type = "app";
                program = "${runner}/bin/ftnmap-e2e";
              };
              default = {
                type = "app";
                program = "${runner}/bin/ftnmap-e2e";
              };
            };
          }
        ) hostSystems
      );
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
              default = pkgsFor.mkShell rec {
                buildInputs = basePackagesFor pkgsFor;
                CPATH = pkgsFor.lib.makeSearchPathOutput "dev" "include" buildInputs;
              };
            };
          }
        ) hostSystems
      );

    };
}
