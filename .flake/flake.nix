{
  description = "ft_nmap development environment";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.05";

  outputs = { self, nixpkgs }:
    let
      supportedSystems = [
        "x86_64-linux" "aarch64-linux"
        "x86_64-darwin" "aarch64-darwin"
      ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in {
      devShells = forAllSystems (system: let
        pkgs = import nixpkgs { inherit system; };
      in {
        default = pkgs.mkShell {
          buildInputs = with pkgs; [ nmap libpcap gcc pkg-config ];
        };
      });
    };
}