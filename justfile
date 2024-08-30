git_hash := `git rev-parse HEAD`

cleangitcondition:
    git diff --exit-code

buildmusl:
    echo "Building static musl binary: {{git_hash}}"
    docker build --build-arg="GIT_HASH={{git_hash}}" -f ./docker/MuslStaticBuild.Dockerfile -t emapmuslbuild .
    docker create --name extract emapmuslbuild
    docker cp extract:/project/build/emap-release-x64-linux-static-Release-dist/packages ./build
    docker rm extract
