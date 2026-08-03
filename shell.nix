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
    # Expose the system OpenGL drivers (Mesa GLX/EGL) so GLX can find FBConfigs
    export LD_LIBRARY_PATH=/run/opengl-driver/lib''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
    echo "Development environment for MapleEngine is ready."
  '';
}
