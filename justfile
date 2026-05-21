set export := true

import 'deps/infra/vcpkg_overlay/vcpkg.just'

project_name := "emap"
export GIT_COMMIT_HASH := `git rev-parse HEAD`
export VCPKG_FORCE_DOWNLOADED_BINARIES := "1"

[windows]
bootstrap triplet=VCPKG_DEFAULT_TRIPLET $VCPKG_ROOT=vcpkg_root:
    - mkdir -p '{{ join(justfile_directory(), "build", "vcpkgs") }}'
    '{{ vcpkg_root }}/vcpkg' install --allow-unsupported --triplet {{ triplet }} --x-install-root=./build/vcpkgs/{{ triplet }}

[windows]
configure_vs $VCPKG_ROOT=vcpkg_root: bootstrap
    cmake --preset x64-windows-static-vs

[windows]
build_vs: configure_vs
    cmake --build ./build/emap-vs --config Release

[windows]
configure triplet=VCPKG_DEFAULT_TRIPLET $VCPKG_ROOT=vcpkg_root:
    cmake --preset {{ triplet }}

[windows]
build_debug triplet=VCPKG_DEFAULT_TRIPLET: (configure triplet)
    cmake --build --preset {{ triplet }}-debug

[windows]
build_release triplet=VCPKG_DEFAULT_TRIPLET: (configure triplet)
    cmake --build --preset {{ triplet }}-release

[windows]
build triplet=VCPKG_DEFAULT_TRIPLET: (build_release triplet)

[unix]
configure:
    cmake --preset nix

[unix]
build_debug: configure
    cmake --build --preset nix-debug

[unix]
build_release: configure
    cmake --build --preset nix-release

[unix]
build: build_release

[unix]
rebuild:
    cmake --build --preset nix-release --clean-first

# update_devlatest: (build_dist 'x64-linux-cluster')
#     cp -v ./build/x64-linux-cluster-dist/Release/emapcli /projects/E-MAP/03_Software/snapshots/devlatest/

[unix]
test_debug: build
    ctest --verbose --preset nix-debug --output-on-failure

[unix]
test_release: build
    ctest --verbose --preset nix-release --output-on-failure

[unix]
test: test_release

[unix]
update:
    nix flake update

[unix]
updatedeps:
    nix flake update --update-input pkgs-mod

# buildmusl:
#     echo "Building static musl binary"
#     docker build --build-arg="GIT_HASH={{`git rev-parse HEAD`}}" -f ./docker/MuslStaticBuild.Dockerfile -t emapmuslbuild .
#     docker create --name extract emapmuslbuild
#     docker cp extract:/project/build/x64-linux-static-dist/packages ./build
#     docker rm extract
