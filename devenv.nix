{
  pkgs,
  inputs,
  lib,
  ...
}:
let
  # Use pkgs-mod's mkBuildEnv to get properly configured musl/mingw packages
  # buildEnv = inputs.pkgs-mod.lib.mkBuildEnv pkgs.system;
  mingwBuildEnv = inputs.pkgs-mod.lib.mkBuildEnvMingwCross pkgs.system { };

  buildEnvPackages = with pkgs; [
    just
    lld
    cmake
    ninja
    pkg-config
  ];

  pkgModDeps =
    p: with p; [
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
      pkg-mod-doctest
      fast-cpp-csv-parser # header-only
      microsoft-gsl # header-only
    ];

  getAllPropagated =
    p: lib.unique (lib.flatten (map (x: [ x ] ++ getAllPropagated (x.propagatedBuildInputs or [ ])) p));

  # Cross-compilation setup for MinGW
  pkgsMingw = mingwBuildEnv.pkgsMingw;
  pkgsCross = pkgsMingw.pkgsCross.mingwW64;

in
{
  overlays = [
    (inputs.pkgs-mod.lib.mkOverlay {
      static = true;
    })
  ];

  profiles = {
    mingw.module = {
      env = {
        ENVIRONMENT = "mingw";
      };

      packages = [
        pkgsCross.stdenv.cc
        pkgsCross.windows.pthreads
      ];
    };
  };

  languages.cplusplus = {
    enable = true;
  };

  tasks = {
    "emap:build" = {
      exec = "just build";
      before = [ "devenv:enterTest" ];
      showOutput = true;
    };
    "emap:test" = {
      exec = "ctest --preset nix-release --verbose --output-on-failure --no-compress-output";
      before = [ "devenv:enterTest" ];
      after = [ "emap:build" ];
      showOutput = true;
    };
  };

  packages = buildEnvPackages ++ (pkgModDeps pkgs);
}
