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

            # Allow running generic dynamically-linked binaries (e.g., bundled NodeJS)
            programs.nix-ld.enable = true;

            system.stateVersion = "24.05";
          }
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
