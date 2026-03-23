{
  description = "emap";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.11";

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
            buildEnv = pkgs-mod.lib.mkBuildEnv system;
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
            pkgsForBuild: pkgsForHost: isStatic:
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

              nativeBuildInputs = with pkgsForBuild; [
                cmake
                ninja
                pkg-config
              ];

              buildInputs =
                with pkgsForHost;
                [
                  pkg-mod-cryptopp
                  pkg-mod-eigen
                  pkg-mod-gdal
                  pkg-mod-howard-hinnant-date
                  pkg-mod-libxlsxwriter
                  pkg-mod-lyra
                  pkg-mod-indicators
                  pkg-mod-type_safe
                  pkg-mod-fmt
                  pkg-mod-spdlog
                  pkg-mod-onetbb
                  pkg-mod-tomlplusplus
                  pkg-mod-vc
                  fast-cpp-csv-parser # header-only
                  microsoft-gsl # header-only
                ]
                ++ pkgsForHost.lib.optionals (pkgsForHost.stdenv.isLinux && !isStatic) [
                  glib
                ];

              checkInputs = with pkgsForHost; [ pkg-mod-doctest ];

              cmakeFlags = [
                "-DCMAKE_BUILD_TYPE=Release"
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
          default = mkPackage buildEnv.pkgsDefault buildEnv.pkgsStatic false;
        }
        // inputs.nixpkgs.lib.optionalAttrs (nixpkgs.lib.strings.hasInfix "linux" system) {
          musl = mkPackage buildEnv.pkgsDefault buildEnv.pkgsStaticMusl true;
          windows = mkPackage buildEnvMingwCross.pkgsDefault buildEnvMingwCross.pkgsMingw true;
        }
      );

      checks = forEachSupportedSystem (
        { buildEnv, ... }:
        let
          pkgs = buildEnv.pkgsDefault;
          mkTest =
            pkg:
            pkg.overrideAttrs (old: {
              cmakeFlags = old.cmakeFlags or [ ] ++ [
                "-DBUILD_TESTING=ON"
              ];

              doCheck = true;

              checkPhase = ''
                ctest --output-on-failure
              '';
            });
        in
        {
          default = mkTest self.packages.${pkgs.system}.default;

        }
        // inputs.nixpkgs.lib.optionalAttrs pkgs.stdenv.isLinux {
          musl = mkTest self.packages.${pkgs.system}.musl;
        }
      );

      devShells = forEachSupportedSystem (
        { buildEnv, ... }:
        let
          pkgs = buildEnv.pkgsDefault;
          emap = self.packages.${pkgs.system}.default;
        in
        {
          default = pkgs.mkShell {
            inputsFrom = [ emap ];
            name = "dev";
            packages =
              with pkgs;
              [
                # languages servers/formatters
                nil
                nixfmt-rfc-style
                clang-tools
                neocmakelsp
                # development tools
                cmake
                cmakeCurses
                ninja
                just
                # test frameworks
                pkg-mod-doctest
              ]
              ++ (if pkgs.system == "aarch64-darwin" then [ ] else [ pkgs.gdb ]);
          };
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
