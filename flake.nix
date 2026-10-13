{
  description = "emap";

  inputs = {
    nixpkgs.follows = "pkgs-mod/nixpkgs";

    pkgs-mod.url = "github:VITO-RMA/nix-pkgs/main";

    infra-src = {
      url = "github:VITObelgium/cpp-infra/master";
      flake = false;
    };

    geodynamix-src = {
      url = "github:VITObelgium/geodynamix/main";
      flake = false;
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      pkgs-mod,
      ...
    }@inputs:

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
            inherit system;
            buildEnv = pkgs-mod.lib.mkBuildEnvWith {
              inherit system;
              glibcVersion = "2.28";
            };
            buildEnvMingwCross =
              if (nixpkgs.lib.strings.hasInfix "linux" system) then
                pkgs-mod.lib.mkBuildEnvMingwCross system { llvm = false; }
              else
                null;
          }
        );
    in
    {
      packages = forEachSupportedSystem (
        {
          system,
          buildEnv,
          buildEnvMingwCross,
        }:
        let
          mkPackage =
            pkgsForBuild: pkgsForHost: crossEmulator:
            let
              needsCrossEmulator = crossEmulator != null;
              canRunTests =
                pkgsForHost.stdenv.buildPlatform.canExecute pkgsForHost.stdenv.hostPlatform || needsCrossEmulator;
            in
            pkgsForHost.stdenv.mkDerivation {
              pname = "emap";
              version = "dev";

              src = ./.;
              # make sure deps/infra and deps/geodynamix exist in the build tree
              postPatch = ''
                rm -rf deps/infra deps/geodynamix
                mkdir -p deps
                ln -s ${inputs.infra-src} deps/infra
                ln -s ${inputs.geodynamix-src} deps/geodynamix
              '';

              nativeBuildInputs =
                with pkgsForBuild;
                [
                  cmake
                  ninja
                  pkg-config
                ]
                ++ lib.optionals needsCrossEmulator [ crossEmulator ];

              buildInputs = with pkgsForHost; [
                pkg-mod-cryptopp
                pkg-mod-eigen
                pkg-mod-gdal
                pkg-mod-libxlsxwriter
                pkg-mod-lyra
                pkg-mod-indicators
                pkg-mod-type_safe
                pkg-mod-fmt
                pkg-mod-spdlog
                pkg-mod-onetbb
                pkg-mod-tomlplusplus
                pkg-mod-vc
                pkg-mod-doctest
                pkg-mod-howard-hinnant-date
                fast-cpp-csv-parser # header-only
              ];

              cmakeGenerator = "Ninja";
              cmakeFlags = [
                "-DCMAKE_BUILD_TYPE=Release"
                # The sandbox source has no .git; use the flake's revision metadata.
                "-DPACKAGE_VERSION_COMMITHASH=${self.rev or self.dirtyRev or "dev"}"
                "-DEMAP_STRIP_BINARY=ON"
                "-DBUILD_TESTING=ON"
              ]
              ++
                pkgsForBuild.lib.optionals
                  (pkgsForHost.stdenv.hostPlatform.isLinux && pkgsForHost.stdenv.hostPlatform.isGnu)
                  [
                    "-DCMAKE_EXE_LINKER_FLAGS=-static-libstdc++"
                  ]
              ++ pkgsForBuild.lib.optionals needsCrossEmulator [
                "-DCMAKE_CROSSCOMPILING_EMULATOR=${pkgsForBuild.lib.getExe crossEmulator}"
                "-DGDX_AVX2=OFF"
              ];

              # Portable Zig/glibc binaries use a distro loader absent from the sandbox.
              # Other builds run tests directly or through the configured emulator.
              postBuild = pkgsForBuild.lib.optionalString canRunTests ''
                export HOME="$TMPDIR"
                ${pkgsForBuild.lib.optionalString needsCrossEmulator ''
                  export WINEDEBUG=-all
                  export WINEPREFIX="$TMPDIR/wine-prefix"
                ''}
                ctest --output-on-failure
              '';

              meta = {
                description = "Emission preprocessor for different Air Quality Models";
                homepage = "https://github.com/VITObelgium/emap";
                platforms = pkgsForHost.lib.platforms.unix ++ pkgsForHost.lib.platforms.windows;
              };
            };
        in
        {
          default = mkPackage buildEnv.pkgsDefault buildEnv.pkgsStatic null;
        }
        // inputs.nixpkgs.lib.optionalAttrs (nixpkgs.lib.strings.hasInfix "linux" system) {
          musl = mkPackage buildEnv.pkgsDefault buildEnv.pkgsStaticMusl null;
        }
        // inputs.nixpkgs.lib.optionalAttrs (system == "x86_64-linux") {
          windows =
            mkPackage buildEnvMingwCross.pkgsDefault buildEnvMingwCross.pkgsMingw
              buildEnv.pkgsDefault.wine64Packages.stable;
        }
      );

      apps = forEachSupportedSystem (
        { buildEnv, ... }:
        let
          pkgs = buildEnv.pkgsStatic;
          emap = self.packages.${pkgs.system}.default;
        in
        rec {
          emapcli = {
            type = "app";
            program = "${emap}/emapcli";
          };

          default = emapcli;
        }
      );
    };
}
