{
  description = "emap";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.05";

    pkgs-mod.url = "github:VITO-RMA/nix-pkgs/main";
    pkgs-mod.inputs.nixpkgs.follows = "nixpkgs";

    infra-src = {
      url = "github:VITObelgium/cpp-infra/master";
      flake = false;
    };

    geodynamix-src = {
      url = "github:VITObelgium/geodynamix/develop";
      flake = false;
    };
  };

  outputs =
    { self, nixpkgs, pkgs-mod, ... }@inputs:

    let
      supportedSystems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];

      useStatic = false;

      forEachSupportedSystem =
        f:
        inputs.nixpkgs.lib.genAttrs supportedSystems (
          system:
          f {
            # Regular build with static packages dynamically linked against glibc
            pkgs = import nixpkgs {
              inherit system;
              overlays = [
                (pkgs-mod.overlayForStatic true)
              ];
            };

            # Fully static musl build
            pkgsMusl = import nixpkgs {
              inherit system;
              static = true;

              overlays = [
                (pkgs-mod.overlayForStatic true)
              ];
            };

            pkgsWindows = (
              import nixpkgs {
                inherit system;
                crossSystem = {
                  config = "x86_64-w64-mingw32";
                };

                static = true;
                overlays = [
                  (pkgs-mod.overlayForStatic useStatic)
                ];
              }
            );
          }
        );
    in
    {
      packages = forEachSupportedSystem (
        {
          pkgs,
          pkgsMusl,
          pkgsWindows,
        }:
        let
          mkPackage =
            pkgsForBuild: pkgsForHost: isStatic: isWindows:
            let
              baseStdenv = pkgsForHost.stdenv;

              # On Windows, use win32 threads to get a fully static binary
              stdenv' =
                if isWindows then
                  let
                    buildPkgs = pkgsForHost.buildPackages;
                    gccWin32 = buildPkgs.wrapCC (
                      buildPkgs.gcc-unwrapped.override {
                        # use winthreads for fully static linking
                        threadsCross = {
                          model = "win32";
                          package = null;
                        };
                      }
                    );
                  in
                  pkgsForHost.overrideCC baseStdenv gccWin32
                else
                  baseStdenv;

              projectRoot = ./.;

            in
            stdenv'.mkDerivation {
              pname = "emap";
              version = "dev";

              src = projectRoot;
              # make sure deps/infra and deps/geodynamix exist in the build tree
              postPatch = ''
                rm -rf deps/infra deps/geodynamix
                mkdir -p deps
                ln -s ${inputs.infra-src} deps/infra
                ln -s ${inputs.geodynamix-src} deps/geodynamix
              '';

              nativeBuildInputs = with pkgsForBuild; [
                cmake
                ninja
                pkg-config
              ];

              buildInputs =
                with pkgsForHost;
                [
                  pkg-cryptopp
                  eigen  # header-only
                  fast-cpp-csv-parser # header-only
                  pkg-gdal
                  pkg-howard-hinnant-date
                  pkg-libxlsxwriter
                  pkg-lyra
                  pkg-indicators
                  pkg-type_safe
                  pkg-fmt
                  microsoft-gsl # header-only
                  pkg-spdlog
                  pkg-onetbb
                  pkg-tomlplusplus
                  pkg-vc
                ]
                ++ pkgsForHost.lib.optionals (pkgsForHost.stdenv.isLinux && !isStatic) [
                  glib
                ];

              checkInputs = with pkgsForHost; [ doctest ];

              cmakeFlags = [
                "-DCMAKE_BUILD_TYPE=Release"
                "-DENABLE_TESTS=OFF"
              ];

              # Explicitly strip binaries completely (including static builds)
              stripAllList = [ "bin" ];

              meta = {
                description = "Emission preprocessor for different Air Quality Models";
                homepage = "https://github.com/VITObelgium/emap";
                platforms = pkgsForHost.lib.platforms.unix ++ pkgsForHost.lib.platforms.windows;
              };
            };
        in
        {
          default = mkPackage pkgs pkgs false false;
          static = mkPackage pkgs pkgsMusl.pkgsStatic true false;
          windows = mkPackage pkgs pkgsWindows true true;
        }
      );

      checks = forEachSupportedSystem (
        { pkgs, ... }:
        {
          default = self.packages.${pkgs.system}.default;
          static = self.packages.${pkgs.system}.static;
        }
      );

      devShells = forEachSupportedSystem (
        { pkgs, ... }:
        let
          pkg = self.packages.${pkgs.system}.default;
        in
        {
          default =
            pkgs.mkShell.override
              {
                # Override stdenv in order to change compiler:
                # stdenv = pkgs.clangStdenv;
              }
              {
                inputsFrom = [ pkg ];
                name = "dev";
                packages =
                  with pkgs;
                  [
                    # development tools
                    clang-tools
                    cmake
                    cmakeCurses
                    ninja
                    just
                    python3
                  ]
                  ++ (if pkgs.system == "aarch64-darwin" then [ ] else [ gdb ]);
              };
        }
      );
    };
}
