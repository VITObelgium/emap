set export

import 'deps/infra/vcpkg.just'

bootstrap $VCPKG_ROOT=vcpkg_root:
    '{{vcpkg_root}}/vcpkg' install --allow-unsupported --triplet {{VCPKG_DEFAULT_TRIPLET}}

configure $VCPKG_ROOT=vcpkg_root: bootstrap
    cmake --preset {{cmake_preset}}

build_debug: configure
    cmake --build ./build/cmake --config Debug

build_release: configure
    cmake --build ./build/cmake --config Release

build: build_release

[windows]
configure_vs $VCPKG_ROOT=vcpkg_root: bootstrap
    cmake --preset windows-visual-studio

[windows]
build_vs: configure_vs
    cmake --build ./build/visualstudio --config Release

build_dist: git_status_clean bootstrap
    rm -rf ./build/dist
    cmake --preset {{cmake_preset}}-dist
    cmake --build ./build/dist --config Release --target package

test_debug: build
    ctest --verbose --test-dir ./build/cmake --output-on-failure -C Debug

test_release: build
    ctest --verbose --test-dir ./build/cmake --output-on-failure -C Release

test: test_release

buildmusl:
    echo "Building static musl binary"
    docker build --build-arg="GIT_HASH={{`git rev-parse HEAD`}}" -f ./docker/MuslStaticBuild.Dockerfile -t emapmuslbuild .
    docker create --name extract emapmuslbuild
    docker cp extract:/project/build/emap-release-x64-linux-static-Release-dist/packages ./build
    docker rm extract
