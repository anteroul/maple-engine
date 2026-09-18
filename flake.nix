{
  description = "MapleEngine - 2D game engine (OpenGL + Box2D 3)";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        lib = pkgs.lib;

        # The engine uses the Box2D 3 C API (b2WorldId, b2CreateWorld,
        # b2World_Step with a substep count). nixpkgs only started shipping
        # Box2D 3 in 25.05 -- 24.11 and older have 2.4.2, which has none of
        # those symbols. On an old pin, build 3.x here instead of failing
        # with a wall of "unknown type name" errors.
        box2d3 =
          if lib.versionAtLeast pkgs.box2d.version "3" then
            pkgs.box2d
          else
            pkgs.stdenv.mkDerivation (finalAttrs: {
              pname = "box2d";
              version = "3.1.0";

              src = pkgs.fetchFromGitHub {
                owner = "erincatto";
                repo = "box2d";
                tag = "v${finalAttrs.version}";
                hash = "sha256-QTSU1+9x8GoUK3hlTDMh43fc4vbNfFR7syt6xVHIuPs=";
              };

              # 3.1.0 turns warnings into errors in two separate ways, and
              # recent gcc trips -Wmaybe-uninitialized in distance.c. Both
              # have to go: -DCMAKE_COMPILE_WARNING_AS_ERROR=OFF does not
              # help, because the explicit flag and the per-target property
              # both win over it.
              postPatch = ''
                substituteInPlace src/CMakeLists.txt \
                  --replace-fail "-Werror" "" \
                  --replace-fail "COMPILE_WARNING_AS_ERROR ON" "COMPILE_WARNING_AS_ERROR OFF"
              '';

              nativeBuildInputs = [ pkgs.cmake ];

              cmakeFlags = [
                (lib.cmakeBool "BOX2D_SAMPLES" false)
                (lib.cmakeBool "BOX2D_UNIT_TESTS" false)
                (lib.cmakeBool "BOX2D_BENCHMARKS" false)
                (lib.cmakeBool "BUILD_SHARED_LIBS" true)
              ];

              meta = {
                description = "2D physics engine for games";
                homepage = "https://box2d.org";
                license = lib.licenses.mit;
              };
            });

        nativeDeps = [ pkgs.cmake pkgs.ninja pkgs.pkg-config ];

        # glfw propagates the X11 libraries it needs, so listing libX11,
        # libXrandr, libXinerama, libXcursor and libXi by hand is redundant
        # -- and the xorg.* aliases are deprecated on current unstable.
        # libGLU is not used by any source file in this repo.
        buildDeps = [ box2d3 pkgs.glfw pkgs.glew pkgs.libGL pkgs.glm ];

        maple-engine = pkgs.stdenv.mkDerivation {
          pname = "maple-engine";
          version = "0.1.0";
          src = ./.;

          nativeBuildInputs = nativeDeps;
          buildInputs = buildDeps;

          # The physics tests never open a window, so they run in the
          # sandbox without a display.
          doCheck = true;
          checkPhase = ''
            runHook preCheck
            ctest --output-on-failure
            runHook postCheck
          '';

          # CMakeLists.txt has no install() rules yet, so install by hand.
          installPhase = ''
            runHook preInstall
            install -Dm755 MapleEngine $out/bin/MapleEngine
            runHook postInstall
          '';

          meta.mainProgram = "MapleEngine";
        };
      in
      {
        packages.default = maple-engine;
        packages.maple-engine = maple-engine;
        checks.default = maple-engine;

        devShells.default = pkgs.mkShell {
          # mkShell's `packages` is the right place for both tools and
          # libraries; `buildInputs` still works but says something subtly
          # different about cross-compilation.
          packages = nativeDeps ++ buildDeps ++ [ pkgs.gdb ];

          shellHook = ''
            # On NixOS the real GL driver (Mesa or NVIDIA GLX) lives here.
            # Pointing LD_LIBRARY_PATH at the libGL store path instead gives
            # you libglvnd's dispatch library with no driver behind it, which
            # is how you end up with "GLX: Failed to create context" or no
            # usable FBConfigs. There is also no need to set PKG_CONFIG_PATH:
            # pkg-config's setup hook already covers every dependency above
            # (and ''${pkgs.glfw}/lib/pkgconfig does not even exist, since
            # glfw keeps its .pc file in its `dev` output).
            export LD_LIBRARY_PATH=/run/opengl-driver/lib''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}

            echo "MapleEngine dev shell - box2d ${box2d3.version}, glfw ${pkgs.glfw.version}, glew ${pkgs.glew.version}"
          '';
        };
      }
    );
}
