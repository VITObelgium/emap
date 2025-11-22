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

      forEachSupportedSystem =
        f:
        inputs.nixpkgs.lib.genAttrs supportedSystems (
          system:
          f {
            buildEnv = pkgs-mod.lib.mkBuildEnv system;
            buildEnvMingwCross = pkgs-mod.lib.mkBuildEnvMingwCross system;
          }
        );
    in
    {
      packages = forEachSupportedSystem (
        {
          buildEnv, buildEnvMingwCross,
        }:
        let
          mkPackage =
            pkgsForBuild: pkgsForHost: isStatic:
            let
              projectRoot = ./.;
            in
            pkgsForHost.stdenv.mkDerivation {
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
                "-DBUILD_TESTING=OFF"
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
          musl = mkPackage buildEnv.pkgsDefault buildEnv.pkgsStaticMusl.pkgsStatic true;
          windows = mkPackage buildEnvMingwCross.pkgsDefault buildEnvMingwCross.pkgsMingw true;
        }
      );

      checks = forEachSupportedSystem (
        { buildEnv, ... }:
        {
          #default = self.packages.${pkgs.pkgsStaticGlibc.system}.default;
          #musl = self.packages.${pkgs.pkgsStpkgsStaticMusl.system}.musl;

          default = self.packages.${buildEnv.pkgsDefault.system}.default;
          musl = self.packages.${buildEnv.pkgsDefault.system}.musl;
        }
      );

      devShells = forEachSupportedSystem (
        { buildEnv, ... }:
        let pkgs = buildEnv.pkgsDefault;
        in
        {
          default =
            buildEnv.pkgsDefault.mkShell
              {
                inputsFrom = [ self.packages.${buildEnv.pkgsDefault.system}.default ];
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
                  ++ (if buildEnv.pkgsDefault.system == "aarch64-darwin" then [ ] else [ gdb ]);
              };
        }
      );
    };
}
