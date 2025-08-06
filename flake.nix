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
              memorySize = 4096;

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
            # Set zsh as the login shell for all users
            users.defaultUserShell = "/run/current-system/sw/bin/zsh";

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

            programs.zsh =
              let
                gitPromptScript = ./script/git-prompt.sh;
              in
              {
                enable = true;
                enableCompletion = true;
                # enableAutosuggestions = true;
                # syntaxHighlighting.enable = true;
                # setOptions = [ "AUTO_CD" ];
                shellAliases = {
                  ll = "ls -l";
                  la = "ls -la";
                  lt = "tree";
                  remake = "make -j clean && make -j";
                };
                interactiveShellInit =
                  let
                    initExtraBeforeCompInit = lib.mkOrder 550 ''
                      # Add completion to fpath
                      fpath=(/Users/yui/.docker/completions $fpath)
                    '';

                    initExtra = lib.mkOrder 1000 ''
                      # Source git prompt script
                      source ${gitPromptScript}
                      GIT_PS1_SHOWUPSTREAM="verbose"
                      precmd () { __git_ps1 "%F{cyan}%~%f%F{blue}" "%s %f" }

                      # ssh agent
                      ssh-add >/dev/null 2>/dev/null
                      ssh-add --apple-use-keychain ~/.ssh/github >/dev/null 2>/dev/null
                    '';
                  in
                  lib.mkMerge [
                    initExtraBeforeCompInit
                    initExtra
                  ];
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
          value = rec {
            vm = self.nixosConfigurations.${host}.config.system.build.vm;
            default = vm;
          };
        }) hostSystems
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
