{
  description = "Dev shell for SYN scanner";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.05"; # or your preferred version

  outputs = { self, nixpkgs }: {
    devShells.default = nixpkgs.lib.mkShell {
      buildInputs = [
        nixpkgs.pkgs.libpcap
        nixpkgs.pkgs.gcc
      ];
    };
  };
}
