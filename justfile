set export := true

configure:
    cmake --preset nix

build_debug: configure
    cmake --build --preset nix-debug

build_release: configure
    cmake --build --preset nix-release

build: build_release

rebuild:
    cmake --build --preset nix-release --clean-first

# update_devlatest: (build_dist 'x64-linux-cluster')
#     cp -v ./build/x64-linux-cluster-dist/Release/emapcli /projects/E-MAP/03_Software/snapshots/devlatest/

test_debug: build
    ctest --verbose --preset nix-debug --output-on-failure

test_release: build
    ctest --verbose --preset nix-release --output-on-failure

test: test_release

update:
    nix flake update

updatedeps:
    nix flake update --update-input pkgs-mod

buildmusl:
    nix build .#packages.x86_64-linux.musl

buildmingw:
    nix build .#packages.x86_64-linux.windows

updateclusterdevversion: buildmusl
    scp ./result/emapcli cluster:/projects/E-MAP/03_Software/snapshots/devlatest/emapcli
