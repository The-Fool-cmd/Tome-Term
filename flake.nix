{
  description = "Terminal-based personal book library in C with ncurses and SQLite";
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
