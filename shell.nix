{ pkgs ? import <nixpkgs> {}, ... }:
let
  inherit (pkgs) stdenv;
in stdenv.mkDerivation {
  name = "my-app";
  buildInputs = with pkgs; [
    gitFull

    global
    cloc

  ];

  shellHook=''
    echo "Nix shell setup "
  '';
}
