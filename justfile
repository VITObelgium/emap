set export

import 'deps/infra/vcpkg.just'

bootstrap triplet=VCPKG_DEFAULT_TRIPLET $VCPKG_ROOT=vcpkg_root:
    '{{vcpkg_root}}/vcpkg' install --allow-unsupported --triplet {{triplet}}

configure triplet=VCPKG_DEFAULT_TRIPLET $VCPKG_ROOT=vcpkg_root: bootstrap
    cmake --preset {{triplet}}

build_debug triplet=VCPKG_DEFAULT_TRIPLET: (configure triplet)
    cmake --build ./build/cmake --config Debug

build_release triplet=VCPKG_DEFAULT_TRIPLET: (configure triplet)
    cmake --build ./build/cmake --config Release

build triplet=VCPKG_DEFAULT_TRIPLET: (build_release triplet)
    cmake --build ./build/cmake --config Release

[windows]
configure_vs $VCPKG_ROOT=vcpkg_root: bootstrap
    cmake --preset x64-windows-static-vs

[windows]
build_vs: configure_vs
    cmake --build ./build/visualstudio --config Release

build_dist triplet=VCPKG_DEFAULT_TRIPLET: git_status_clean (bootstrap triplet)
    rm -rf ./build/dist
    cmake --preset {{triplet}}-dist
    cmake --build ./build/{{triplet}}-dist --config Release --target package

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
