FROM alpine:3.20.2 AS builder

ARG GIT_HASH=dev
WORKDIR /project
RUN apk add bash linux-headers gcc g++ curl zip unzip tar git python3 cmake ninja-is-really-ninja make automake autoconf autoconf-archive libtool pkgconfig
RUN echo insecure >> $HOME/.curlrc

COPY . .
RUN python3 bootstrap.py --triplet=x64-linux-static
RUN python3 build.py --triplet=x64-linux-static --dist --git-hash=${GIT_HASH}
