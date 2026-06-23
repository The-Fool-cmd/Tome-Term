{
  description = "Falling sand sim in C with ncurses";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };
  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs { inherit system; };
      in
      {
        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            valgrind
            clang-tools
            clang
            ncurses
            pkg-config
            gdb
            gnumake
            sqlite
          ];
        };
      }
    );
}
