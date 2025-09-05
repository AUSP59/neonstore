{
  description = "NeonStoreSystem dev env";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-23.11";
  outputs = { self, nixpkgs }:
    let pkgs = import nixpkgs { system = "x86_64-linux"; };
    in {
      devShells.x86_64-linux.default = pkgs.mkShell {
        buildInputs = [ pkgs.cmake pkgs.gcc pkgs.clang pkgs.sqlite pkgs.doxygen pkgs.graphviz ];
      };
    };
}
