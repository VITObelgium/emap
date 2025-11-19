set export

import 'deps/infra/vcpkg_overlay/vcpkg.just'

local_vcpkg_root := join(justfile_directory(), "build/vcpkg")
vcpkg_bin := if os_family() == "windows" {
    join(local_vcpkg_root, "vcpkg.exe")
} else {
    join(local_vcpkg_root, "vcpkg")
}

bootstrap triplet=VCPKG_DEFAULT_TRIPLET $VCPKG_ROOT=local_vcpkg_root: bootstrap_vcpkg
    '{{vcpkg_bin}}' install --allow-unsupported --triplet {{triplet}}

# configure triplet=VCPKG_DEFAULT_TRIPLET $VCPKG_ROOT=local_vcpkg_root: bootstrap
#     cmake --preset {{triplet}}

# build_debug triplet=VCPKG_DEFAULT_TRIPLET: (configure triplet)
#     cmake --build ./build/cmake --config Debug

# build_release triplet=VCPKG_DEFAULT_TRIPLET: (configure triplet)
#     cmake --build ./build/cmake --config Release

# build_vckg triplet=VCPKG_DEFAULT_TRIPLET: (build_release triplet)
#     cmake --build ./build/cmake --config Release

configure:
    cmake  --preset nix

build: configure
    cmake --build ./build/nix --config Release

[windows]
configure_vs $VCPKG_ROOT=local_vcpkg_root: bootstrap
    cmake --preset x64-windows-static-vs

[windows]
build_vs $CMAKE_CONFIGURATION_TYPES="Debug;Release": configure_vs
    cmake --build ./build/visualstudio --config Release

build_dist triplet=VCPKG_DEFAULT_TRIPLET $GIT_COMMIT_HASH=`git rev-parse HEAD`: git_status_clean (bootstrap triplet)
    rm -rf ./build/{{triplet}}-dist
    cmake --preset {{triplet}}-dist
    cmake --build ./build/{{triplet}}-dist --config Release --target package

update_devlatest: (build_dist 'x64-linux-cluster')
    cp -v ./build/x64-linux-cluster-dist/Release/emapcli /projects/E-MAP/03_Software/snapshots/devlatest/

test_debug: build
    ctest --verbose --test-dir ./build/cmake --output-on-failure -C Debug

test_release: build
    ctest --verbose --test-dir ./build/cmake --output-on-failure -C Release

test: test_release

buildmusl:
    echo "Building static musl binary"
    docker build --build-arg="GIT_HASH={{`git rev-parse HEAD`}}" -f ./docker/MuslStaticBuild.Dockerfile -t emapmuslbuild .
    docker create --name extract emapmuslbuild
    docker cp extract:/project/build/x64-linux-static-dist/packages ./build
    docker rm extract
