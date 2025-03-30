{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = [
    pkgs.cmake
    pkgs.gcc
    pkgs.glew
    pkgs.glfw
    pkgs.box2d
    pkgs.stb
    pkgs.glm
  ];

  shellHook = ''
    echo "Development environment for MapleEngine is ready."
  '';
}
