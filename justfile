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

buildglibc:
    nix build .#packages.x86_64-linux.default

buildmusl:
    nix build .#packages.x86_64-linux.musl

buildmingw:
    nix build .#packages.x86_64-linux.windows

updateclusterdevversion: buildmusl
    scp ./result/emapcli cluster:/projects/E-MAP/03_Software/snapshots/devlatest/emapcli

# Download the latest GitHub release and install it in the cluster's releases directory.
updateclusterrelease host='cluster':
    #!/usr/bin/env bash
    set -euo pipefail
    version=$(gh release view --json tagName --jq '.tagName')
    tmpdir=$(mktemp -d)
    # Nix archives contain read-only directories; make them writable for cleanup.
    trap 'chmod -R u+w "$tmpdir"; rm -rf "$tmpdir"' EXIT
    archive="e-map-${version}-Linux-glibc.tar.gz"
    printf 'Downloading release %s (%s)\n' "$version" "$archive"
    gh release download "$version" --pattern "$archive" --dir "$tmpdir"
    mkdir "$tmpdir/$version"
    tar --extract --gzip --file "$tmpdir/$archive" --directory "$tmpdir/$version"
    printf 'Uploading release %s to %s\n' "$version" '{{host}}:/projects/E-MAP/03_Software/releases/'
    # Allow retries over releases previously uploaded with read-only permissions.
    ssh '{{host}}' "if test -d '/projects/E-MAP/03_Software/releases/$version'; then chmod -R u+w '/projects/E-MAP/03_Software/releases/$version'; fi"
    scp -r "$tmpdir/$version" '{{host}}:/projects/E-MAP/03_Software/releases/'
    printf 'Release %s uploaded successfully.\n' "$version"
