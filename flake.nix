{
  description = "ffmpegthumbnailer";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-25.05";
  };

  outputs =
    { self, ... }@inputs:

    let
      supportedSystems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];

      forEachSupportedSystem =
        f:
        inputs.nixpkgs.lib.genAttrs supportedSystems (
          system:
          f {
            pkgs = import inputs.nixpkgs {
              inherit system;
            };

            pkgsStatic = import inputs.nixpkgs {
              inherit system;
            };

            pkgsWindows = (
              import inputs.nixpkgs {
                inherit system;
                crossSystem = {
                  config = "x86_64-w64-mingw32";
                };
              }
            );
          }
        );
    in
    {
      packages = forEachSupportedSystem (
        {
          pkgs,
          pkgsStatic,
          pkgsWindows,
        }:
        let
          mkPackage =
            pkgsForBuild: pkgsForHost: isStatic: isWindows:
            let
              baseStdenv = pkgsForHost.stdenv;

              # On Windows, use win32 threads to get a fully static binary
              stdenv' =
                if isWindows && baseStdenv.cc.isGNU && baseStdenv.targetPlatform.isWindows then
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

              # zlib-ng in zlib-compatible mode
              zlibNgCompat = pkgsForHost.zlib-ng.override {
                withZlibCompat = true;
              };

              # Use zlib-ng and force it to be static-only (we mainly care on Windows)
              zlibNgStatic = zlibNgCompat.overrideAttrs (old: {
                dontDisableStatic = true;
                cmakeFlags = (old.cmakeFlags or [ ]) ++ [
                  "-DBUILD_SHARED_LIBS=OFF"
                ];
              });

              # libpng that uses zlib-ng instead of plain zlib
              libpngWithZlibNg = pkgsForHost.libpng.override {
                zlib = zlibNgStatic;
              };

              projectRoot = ./.;

              infra = projectRoot + "/libs/foo";
              geodynamix = projectRoot + "/libs/bar";

            in
            stdenv'.mkDerivation {
              pname = "emap";
              version = "dev";

              src = self;

              nativeBuildInputs = with pkgsForBuild; [
                cmake
                ninja
                pkg-config
              ];

              buildInputs =
                with pkgsForHost;
                [
                  (gdalMinimal.override {
                  })

                  cryptopp
                  doctest
                  eigen
                  fast-cpp-csv-parser
                  geos
                  howard-hinnant-date
                  libxlsxwriter
                  (pkgsForHost.callPackage ./deps/nix/lyra/package.nix { })
                  (pkgsForHost.callPackage ./deps/nix/indicators/package.nix { })
                  (pkgsForHost.callPackage ./deps/nix/type_safe/package.nix { })
                  fmt
                  microsoft-gsl
                  proj
                  spdlog
                  sqlite
                  tbb_2022_0
                  tomlplusplus
                  vc

                  # proj

                  # "type-safe",
                  # "indicators",

                  # # Use static versions of libpng and libjpeg for Windows
                  # (
                  #   if isWindows then
                  #     libjpeg.override {
                  #       enableStatic = true;
                  #       enableShared = false;
                  #     }
                  #   else
                  #     libjpeg
                  # )
                  # (
                  #   if isWindows then
                  #     libpngWithZlibNg.overrideAttrs (old: {
                  #       dontDisableStatic = true;
                  #       configureFlags = (old.configureFlags or [ ]) ++ [
                  #         "--enable-static"
                  #         "--disable-shared"
                  #       ];
                  #     })
                  #   else
                  #     libpng
                  # )

                ]
                ++ pkgsForHost.lib.optionals isWindows [
                  # ffmpeg transitive dependencies needed for linking on Windows
                  zlibNgStatic
                  (xz.overrideAttrs (old: {
                    dontDisableStatic = true;
                    configureFlags = (old.configureFlags or [ ]) ++ [
                      "--enable-static"
                      "--disable-shared"
                    ];
                  }))
                  (bzip2.overrideAttrs (old: {
                    dontDisableStatic = true;
                    configureFlags = (old.configureFlags or [ ]) ++ [
                      "--enable-static"
                      "--disable-shared"
                    ];
                  }))
                  (libiconv.overrideAttrs (old: {
                    dontDisableStatic = true;
                    configureFlags = (old.configureFlags or [ ]) ++ [
                      "--enable-static"
                      "--disable-shared"
                    ];
                  }))
                ]
                ++ pkgsForHost.lib.optionals (pkgsForHost.stdenv.isLinux && !isStatic) [
                  glib
                ];

              cmakeFlags = [
                "-DCMAKE_BUILD_TYPE=Release"
                "-DENABLE_TESTS=ON"
                "-DTYPESAFE_INSOURCE=ON"
              ];

              # Explicitly strip binaries completely (including static builds)
              stripAllList = [ "bin" ];

              meta = {
                description = "Emission preprocessor for different Air Quality Models";
                homepage = "https://github.com/dirkvdb/ffmpegthumbnailer";
                platforms = pkgsForHost.lib.platforms.unix ++ pkgsForHost.lib.platforms.windows;
              };
            };
        in
        {
          default = mkPackage pkgs pkgs false false;
          static = mkPackage pkgs pkgsStatic.pkgsStatic true false;
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
