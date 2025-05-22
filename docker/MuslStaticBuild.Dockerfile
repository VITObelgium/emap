FROM alpine:3.21.3 AS builder

ARG GIT_HASH=dev
WORKDIR /project
RUN apk add bash linux-headers gcc g++ curl zip unzip tar git python3 just cmake ninja-is-really-ninja make automake autoconf autoconf-archive libtool pkgconfig
RUN echo insecure >> $HOME/.curlrc
RUN git clone --depth 1 https://github.com/microsoft/vcpkg.git
RUN cd vcpkg && ./bootstrap-vcpkg.sh
ENV VCPKG_ROOT=/project/vcpkg

COPY . .
RUN just bootstrap x64-linux-static
RUN just dist x64-linux-static $GIT_HASH
