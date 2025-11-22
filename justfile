set export

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

# buildmusl:
#     echo "Building static musl binary"
#     docker build --build-arg="GIT_HASH={{`git rev-parse HEAD`}}" -f ./docker/MuslStaticBuild.Dockerfile -t emapmuslbuild .
#     docker create --name extract emapmuslbuild
#     docker cp extract:/project/build/x64-linux-static-dist/packages ./build
#     docker rm extract
